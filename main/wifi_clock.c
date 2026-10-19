/* SPDX-License-Identifier: CC0-1.0 */
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_clock.h"
#include "wifi_credentials.h"

#define CONNECT_TIMEOUT_US (30LL * 1000 * 1000)
#define SAVED_RETRY_US (5LL * 1000 * 1000)

typedef enum {
    COMMAND_CONNECT, DRIVER_STARTED, DRIVER_STOPPED,
    DRIVER_DISCONNECTED, DRIVER_GOT_IP
} command_kind_t;

typedef struct {
    command_kind_t kind;
    wifi_credentials_t credentials;
} wifi_command_t;

typedef struct {
    uint32_t version;
    wifi_credentials_t credentials;
} saved_network_t;

/* Only the worker task reads or changes this object, including the password. */
typedef struct {
    wifi_credentials_t credentials;
    bool running, switching, auto_reconnect, save_pending, attempt_active;
    unsigned retries;
    int64_t deadline_us, retry_at_us;
} controller_t;

static const char *TAG = "wifi_clock";
static _Atomic(QueueHandle_t) s_commands;
static portMUX_TYPE s_info_lock = portMUX_INITIALIZER_UNLOCKED;
static wifi_clock_info_t s_info = {.state = WIFI_CLOCK_STARTING};
static bool s_sntp_started; /* Worker task only. */

void wifi_clock_get_info(wifi_clock_info_t *info)
{
    if (info == NULL) return;
    portENTER_CRITICAL(&s_info_lock);
    *info = s_info;
    portEXIT_CRITICAL(&s_info_lock);
}

bool wifi_clock_is_connected(void)
{
    wifi_clock_info_t info;
    wifi_clock_get_info(&info);
    return info.connected;
}

static void set_info(const controller_t *c, wifi_clock_state_t state,
                     bool connected, esp_err_t error)
{
    wifi_clock_info_t info = {
        .state = state, .connected = connected,
        .saved = c->auto_reconnect && !c->save_pending, .last_error = error,
    };
    memcpy(info.ssid, c->credentials.ssid, sizeof(info.ssid));
    portENTER_CRITICAL(&s_info_lock);
    s_info = info;
    portEXIT_CRITICAL(&s_info_lock);
}

static esp_err_t read_saved_network(wifi_credentials_t *credentials)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("s31_wifi", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    saved_network_t record = {0};
    size_t size = sizeof(record);
    err = nvs_get_blob(handle, "station", &record, &size);
    nvs_close(handle);
    if (err != ESP_OK) return err;
    if (size != sizeof(record) || record.version != 1 ||
        memchr(record.credentials.ssid, '\0', sizeof(record.credentials.ssid)) == NULL ||
        memchr(record.credentials.password, '\0', sizeof(record.credentials.password)) == NULL ||
        wifi_credentials_error(record.credentials.ssid, record.credentials.password) != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *credentials = record.credentials;
    return ESP_OK;
}

static esp_err_t save_network(const wifi_credentials_t *credentials)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("s31_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    saved_network_t record = {0};
    record.version = 1;
    record.credentials = *credentials;
    err = nvs_set_blob(handle, "station", &record, sizeof(record));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t wifi_clock_connect(const char *ssid, const char *password)
{
    if (wifi_credentials_error(ssid, password) != NULL) return ESP_ERR_INVALID_ARG;
    QueueHandle_t queue = atomic_load(&s_commands);
    if (queue == NULL) return ESP_ERR_INVALID_STATE;
    /* Reserve twelve slots for driver events when Connect is tapped rapidly. */
    if (uxQueueMessagesWaiting(queue) >= 4) return ESP_ERR_TIMEOUT;
    wifi_command_t command = {.kind = COMMAND_CONNECT};
    memcpy(command.credentials.ssid, ssid, strlen(ssid));
    memcpy(command.credentials.password, password, strlen(password));
    return xQueueSend(queue, &command, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    wifi_command_t command = {0};
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) command.kind = DRIVER_STARTED;
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) command.kind = DRIVER_STOPPED;
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) command.kind = DRIVER_DISCONNECTED;
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) command.kind = DRIVER_GOT_IP;
    else return;
    if (xQueueSend(atomic_load(&s_commands), &command, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Wi-Fi event queue is full");
    }
}

static void fail_attempt(controller_t *c, esp_err_t error)
{
    c->attempt_active = false;
    c->deadline_us = 0;
    c->retry_at_us = c->auto_reconnect ? esp_timer_get_time() + SAVED_RETRY_US : 0;
    set_info(c, c->auto_reconnect ? WIFI_CLOCK_CONNECTING : WIFI_CLOCK_CONNECT_FAILED,
             false, error);
}

static void connect_now(controller_t *c)
{
    c->attempt_active = true;
    if (c->deadline_us == 0) c->deadline_us = esp_timer_get_time() + CONNECT_TIMEOUT_US;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) fail_attempt(c, err);
}

static void start_candidate(controller_t *c)
{
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, c->credentials.ssid, strlen(c->credentials.ssid));
    memcpy(config.sta.password, c->credentials.password, strlen(c->credentials.password));
    config.sta.threshold.authmode = c->credentials.password[0] == '\0'
                                       ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) { fail_attempt(c, err); return; }
    c->running = true;
    c->attempt_active = true;
    c->deadline_us = esp_timer_get_time() + CONNECT_TIMEOUT_US;
    set_info(c, WIFI_CLOCK_CONNECTING, false, ESP_OK);
}

static void request_connection(controller_t *c, const wifi_credentials_t *credentials)
{
    c->credentials = *credentials;
    c->auto_reconnect = false;
    c->save_pending = true;
    c->attempt_active = false;
    c->deadline_us = c->retry_at_us = 0;
    c->retries = 0;
    set_info(c, WIFI_CLOCK_CONNECTING, false, ESP_OK);
    if (c->switching) return; /* STOP will apply the latest request. */
    if (c->running) {
        c->switching = true;
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_OK) return; /* Wait for STOP before changing config. */
        c->switching = false;
        if (err != ESP_ERR_WIFI_NOT_STARTED) { fail_attempt(c, err); return; }
        c->running = false;
    }
    start_candidate(c);
}

static void handle_connected(controller_t *c)
{
    wifi_ap_record_t ap;
    if (!c->running || c->switching || !c->attempt_active ||
        esp_wifi_sta_get_ap_info(&ap) != ESP_OK ||
        strncmp((const char *)ap.ssid, c->credentials.ssid, 32) != 0) return;

    c->auto_reconnect = true;
    c->retries = 0;
    c->deadline_us = c->retry_at_us = 0;
    esp_err_t err = ESP_OK;
    if (c->save_pending) {
        err = save_network(&c->credentials);
        if (err == ESP_OK) {
            c->save_pending = false;
            ESP_LOGI(TAG, "Wi-Fi connected; network saved for the next boot.");
        } else {
            ESP_LOGE(TAG, "Connected, but NVS save failed: %s", esp_err_to_name(err));
        }
    }
    set_info(c, err == ESP_OK ? WIFI_CLOCK_CONNECTED : WIFI_CLOCK_SAVE_FAILED, true, err);
    if (!s_sntp_started) {
        err = esp_netif_sntp_start();
        if (err == ESP_OK) s_sntp_started = true;
        else ESP_LOGE(TAG, "Could not start network time: %s", esp_err_to_name(err));
    }
}

static void wifi_worker(void *arg)
{
    (void)arg;
    controller_t c = {0};
    esp_err_t err = read_saved_network(&c.credentials);
    if (err == ESP_OK) {
        c.auto_reconnect = true;
        start_candidate(&c);
    } else {
        set_info(&c, WIFI_CLOCK_SETUP_REQUIRED, false, err);
        ESP_LOGI(TAG, "Use the screen's Wi-Fi button to configure a network.");
    }
    for (;;) {
        wifi_command_t command;
        if (xQueueReceive(atomic_load(&s_commands), &command, pdMS_TO_TICKS(200)) == pdTRUE) {
            switch (command.kind) {
            case COMMAND_CONNECT:
                request_connection(&c, &command.credentials);
                break;
            case DRIVER_STARTED:
                if (c.running && !c.switching && c.attempt_active) connect_now(&c);
                break;
            case DRIVER_STOPPED:
                c.running = false;
                if (c.switching) { c.switching = false; start_candidate(&c); }
                break;
            case DRIVER_DISCONNECTED:
                if (c.running && !c.switching && c.attempt_active) {
                    c.attempt_active = false;
                    if (c.auto_reconnect || ++c.retries <= 3) {
                        c.retry_at_us = esp_timer_get_time() +
                            (c.auto_reconnect ? SAVED_RETRY_US : 1500000);
                        if (c.auto_reconnect) c.deadline_us = 0;
                        set_info(&c, WIFI_CLOCK_CONNECTING, false, ESP_OK);
                    } else fail_attempt(&c, ESP_FAIL);
                }
                break;
            case DRIVER_GOT_IP:
                handle_connected(&c);
                break;
            }
        }
        int64_t now = esp_timer_get_time();
        if (c.retry_at_us != 0 && now >= c.retry_at_us) {
            c.retry_at_us = 0;
            if (c.deadline_us != 0 && now >= c.deadline_us) fail_attempt(&c, ESP_ERR_TIMEOUT);
            else connect_now(&c);
        }
        if (c.attempt_active && c.deadline_us != 0 && now >= c.deadline_us) {
            fail_attempt(&c, ESP_ERR_TIMEOUT);
            (void)esp_wifi_disconnect();
        }
    }
}

static void on_time_synchronized(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "Network time synchronized. Display uses Beijing time (UTC+8).");
}

void wifi_clock_start(void)
{
    if (setenv("TZ", "CST-8", 1) != 0) abort();
    tzset();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (esp_netif_create_default_wifi_sta() == NULL) abort();
    QueueHandle_t queue = xQueueCreate(16, sizeof(wifi_command_t));
    if (queue == NULL) abort();
    atomic_store(&s_commands, queue);
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    time_config.start = false;
    time_config.sync_cb = on_time_synchronized;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&time_config));
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));
    if (xTaskCreate(wifi_worker, "wifi_setup", 6144, NULL, 5, NULL) != pdPASS) abort();
}
