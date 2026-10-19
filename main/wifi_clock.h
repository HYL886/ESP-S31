#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "wifi_networks.h"

typedef enum {
    WIFI_CLOCK_STARTING,
    WIFI_CLOCK_SETUP_REQUIRED,
    WIFI_CLOCK_CONNECTING,
    WIFI_CLOCK_CONNECTED,
    WIFI_CLOCK_CONNECT_FAILED,
    WIFI_CLOCK_SAVE_FAILED,
} wifi_clock_state_t;

typedef struct {
    wifi_clock_state_t state;
    bool connected;
    bool saved;
    char ssid[33];
    esp_err_t last_error;
} wifi_clock_info_t;

typedef enum {
    WIFI_SCAN_IDLE,
    WIFI_SCAN_WAITING,
    WIFI_SCAN_RUNNING,
    WIFI_SCAN_DONE,
    WIFI_SCAN_FAILED,
} wifi_scan_state_t;

typedef struct {
    wifi_scan_state_t state;
    uint32_t revision;
    esp_err_t last_error;
    size_t count;
    wifi_network_t networks[WIFI_NETWORK_LIMIT];
} wifi_scan_info_t;

/* 初始化 Wi-Fi 和网络校时。仅在 app_main 中调用一次，不会等待联网。 */
void wifi_clock_start(void);

/* Copy a UI-safe snapshot. Passwords are never returned. */
void wifi_clock_get_info(wifi_clock_info_t *info);

/* Queue a new connection. Save it to NVS only after obtaining an IP address. */
esp_err_t wifi_clock_connect(const char *ssid, const char *password);

/* Queue a scan. Only the network worker waits for the radio, never LVGL. */
esp_err_t wifi_clock_scan(void);
void wifi_clock_get_scan(wifi_scan_info_t *info);
