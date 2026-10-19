/* SPDX-License-Identifier: CC0-1.0 */
#include "wifi_setup.h"
#include <string.h>
#include "wifi_clock.h"
#include "wifi_credentials.h"

static lv_obj_t *s_overlay;
static lv_obj_t *s_ssid;
static lv_obj_t *s_password;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_message;
static lv_obj_t *s_form;
static lv_obj_t *s_networks;
static lv_obj_t *s_network_list;
static lv_obj_t *s_scan_message;
static lv_obj_t *s_rescan;
static lv_obj_t *s_networks_button;
static wifi_scan_info_t s_ui_scan;
static lv_font_t s_network_font;
static bool s_local_error;

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int width, int height);

static void show_networks(lv_event_t *event)
{
    (void)event;
    lv_keyboard_set_textarea(s_keyboard, NULL);
    lv_textarea_set_text(s_password, "");
    lv_obj_add_flag(s_form, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_networks, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_networks_button, LV_OBJ_FLAG_HIDDEN);
}

static void show_form(const char *ssid)
{
    s_local_error = false;
    lv_textarea_set_text(s_ssid, ssid != NULL ? ssid : "");
    lv_textarea_set_text(s_password, "");
    lv_obj_add_flag(s_networks, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_form, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_networks_button, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, ssid != NULL ? s_password : s_ssid);
    wifi_setup_refresh();
}

static void network_clicked(lv_event_t *event)
{
    const wifi_network_t *selected = lv_event_get_user_data(event);
    if (selected == NULL || !selected->supported) return;
    wifi_network_t network = *selected;
    show_form(network.ssid);
    lv_textarea_set_placeholder_text(s_password, network.password_required ?
        "Wi-Fi password" : "Open network: leave empty");
}

static void manual_clicked(lv_event_t *event)
{
    (void)event;
    show_form(NULL);
    lv_textarea_set_placeholder_text(s_password, "Wi-Fi password");
}

static void scan_clicked(lv_event_t *event)
{
    (void)event;
    esp_err_t err = wifi_clock_scan();
    if (err == ESP_OK) {
        lv_obj_add_state(s_rescan, LV_STATE_DISABLED);
        lv_label_set_text(s_scan_message, "Scan requested...");
    } else {
        lv_label_set_text(s_scan_message,
            "Wi-Fi is starting or a scan is already pending. Tap Rescan shortly.");
    }
}

static void refresh_networks(void)
{
    wifi_scan_info_t scan;
    wifi_clock_get_scan(&scan);
    if (scan.revision == s_ui_scan.revision) return;
    lv_obj_clean(s_network_list);
    s_ui_scan = scan;
    bool busy = scan.state == WIFI_SCAN_WAITING || scan.state == WIFI_SCAN_RUNNING;
    if (busy) lv_obj_add_state(s_rescan, LV_STATE_DISABLED);
    else lv_obj_remove_state(s_rescan, LV_STATE_DISABLED);
    switch (scan.state) {
    case WIFI_SCAN_WAITING:
        lv_label_set_text(s_scan_message,
            "Waiting for the current connection attempt, then scanning...");
        break;
    case WIFI_SCAN_RUNNING:
        lv_label_set_text(s_scan_message, "Searching nearby 2.4 GHz Wi-Fi...");
        break;
    case WIFI_SCAN_DONE:
        lv_label_set_text(s_scan_message, scan.count == 0 ?
            "No named networks found. Tap Rescan or enter a hidden name manually." :
            "Tap your network. Strongest signals first; scroll for more.");
        break;
    case WIFI_SCAN_FAILED:
        lv_label_set_text(s_scan_message, "Scan failed. Tap Rescan to try again.");
        break;
    default:
        lv_label_set_text(s_scan_message, "Tap Rescan to find nearby Wi-Fi.");
        break;
    }
    for (size_t i = 0; i < scan.count; ++i) {
        const wifi_network_t *network = &s_ui_scan.networks[i];
        lv_obj_t *button = lv_button_create(s_network_list);
        lv_obj_set_size(button, lv_pct(100), 64);
        lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(button, network_clicked, LV_EVENT_CLICKED, (void *)network);
        if (!network->supported) lv_obj_add_state(button, LV_STATE_DISABLED);

        /* Render control bytes harmlessly; keep the original SSID for connecting. */
        char display_name[sizeof(network->ssid)];
        memcpy(display_name, network->ssid, sizeof(display_name));
        for (size_t j = 0; display_name[j] != '\0'; ++j) {
            unsigned char ch = (unsigned char)display_name[j];
            if (ch < 32 || ch == 127) display_name[j] = '?';
        }
        lv_obj_t *name = lv_label_create(button);
        lv_label_set_text(name, display_name);
        lv_obj_set_style_text_font(name, &s_network_font, 0);
        lv_obj_set_width(name, lv_pct(70));
        lv_obj_set_height(name, 30);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *detail = lv_label_create(button);
        lv_obj_set_style_text_font(detail, LV_FONT_DEFAULT, 0);
        lv_label_set_text_fmt(detail, "%d dBm\n%s", (int)network->rssi,
            !network->supported ? "Unsupported security" :
            network->password_required ? "Password required" : "Open network");
        lv_obj_align(detail, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void wifi_setup_refresh(void)
{
    if (s_overlay == NULL) return;
    refresh_networks();
    if (s_local_error) return;
    wifi_clock_info_t info;
    wifi_clock_get_info(&info);
    const char *message;
    switch (info.state) {
    case WIFI_CLOCK_STARTING:
        message = "Wi-Fi is starting. Please wait.";
        break;
    case WIFI_CLOCK_SETUP_REQUIRED:
        message = "Enter the password for your selected network.\nLeave it empty only for open Wi-Fi.";
        break;
    case WIFI_CLOCK_CONNECTING:
        message = "Connecting...\nA new connection can take up to 30 seconds.";
        break;
    case WIFI_CLOCK_CONNECTED:
        message = "Connected and saved.\nThe next boot will reconnect automatically.";
        break;
    case WIFI_CLOCK_CONNECT_FAILED:
        message = "Could not connect. Check name/password, then retry.\nThe last saved network has not been replaced.";
        break;
    case WIFI_CLOCK_SAVE_FAILED:
        message = "Connected, but saving failed.\nTap Connect again to retry saving.";
        break;
    default:
        message = "Use Connect to try this network.";
        break;
    }
    lv_label_set_text(s_message, message);
}

static void close_setup(lv_event_t *event)
{
    (void)event;
    if (s_overlay == NULL) return;
    lv_obj_t *overlay = s_overlay;
    lv_keyboard_set_textarea(s_keyboard, NULL);
    lv_textarea_set_text(s_password, "");
    s_overlay = s_ssid = s_password = s_keyboard = s_message = NULL;
    s_form = s_networks = s_network_list = s_scan_message = s_rescan = NULL;
    s_networks_button = NULL;
    lv_obj_delete_async(overlay);
}

static void on_input(lv_event_t *event)
{
    if (s_keyboard == NULL) return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(s_keyboard, lv_event_get_target_obj(event));
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        s_local_error = false;
    }
}

static void connect_clicked(lv_event_t *event)
{
    (void)event;
    if (s_overlay == NULL) return;
    const char *ssid = lv_textarea_get_text(s_ssid);
    const char *password = lv_textarea_get_text(s_password);
    const char *error = wifi_credentials_error(ssid, password);
    if (error != NULL) {
        s_local_error = true;
        lv_label_set_text(s_message, error);
        return;
    }
    esp_err_t err = wifi_clock_connect(ssid, password);
    s_local_error = err != ESP_OK;
    lv_label_set_text(s_message, err == ESP_OK ? "Connecting..." :
        "Wi-Fi is busy or still starting. Please try Connect again.");
}

static void keyboard_event(lv_event_t *event)
{
    if (s_overlay == NULL) return;
    if (lv_event_get_code(event) == LV_EVENT_CANCEL) {
        close_setup(event);
    } else if (lv_event_get_code(event) == LV_EVENT_READY) {
        if (lv_keyboard_get_textarea(s_keyboard) == s_ssid) {
            lv_keyboard_set_textarea(s_keyboard, s_password);
        } else {
            connect_clicked(event);
        }
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int width, int height)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void wifi_setup_open(lv_event_t *event)
{
    (void)event;
    if (s_overlay != NULL) return;
    s_local_error = false;
    memset(&s_ui_scan, 0, sizeof(s_ui_scan));
    /* A mutable copy adds Chinese fallback without changing LVGL's const font. */
    s_network_font = lv_font_montserrat_24;
    s_network_font.fallback = &lv_font_source_han_sans_sc_16_cjk;
    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x142030), 0);
    lv_obj_set_style_text_color(s_overlay, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_overlay, &lv_font_montserrat_24, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 12, 0);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_label_set_text(title, "Wi-Fi setup");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 2);
    lv_obj_t *back = make_button(s_overlay, "Back", 90, 36);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(back, close_setup, LV_EVENT_CLICKED, NULL);

    s_networks_button = make_button(s_overlay, "Networks", 160, 36);
    lv_obj_align(s_networks_button, LV_ALIGN_TOP_RIGHT, -110, 0);
    lv_obj_add_event_cb(s_networks_button, show_networks, LV_EVENT_CLICKED, NULL);

    s_form = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_form);
    lv_obj_set_size(s_form, lv_pct(100), 410);
    lv_obj_align(s_form, LV_ALIGN_TOP_LEFT, 0, 44);
    lv_obj_remove_flag(s_form, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ssid_label = lv_label_create(s_form);
    lv_label_set_text(ssid_label, "SSID");
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 0, 8);
    s_ssid = lv_textarea_create(s_form);
    lv_obj_set_size(s_ssid, 610, 44);
    lv_obj_align(s_ssid, LV_ALIGN_TOP_LEFT, 140, 0);
    lv_obj_set_style_text_font(s_ssid, &s_network_font, 0);
    lv_obj_set_style_pad_all(s_ssid, 6, 0);
    lv_textarea_set_one_line(s_ssid, true);
    lv_textarea_set_max_length(s_ssid, WIFI_SSID_MAX_BYTES);
    lv_textarea_set_placeholder_text(s_ssid, "Wi-Fi network name");
    lv_obj_add_event_cb(s_ssid, on_input, LV_EVENT_ALL, NULL);

    lv_obj_t *password_label = lv_label_create(s_form);
    lv_label_set_text(password_label, "Password");
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 0, 60);
    s_password = lv_textarea_create(s_form);
    lv_obj_set_size(s_password, 610, 44);
    lv_obj_align(s_password, LV_ALIGN_TOP_LEFT, 140, 52);
    lv_obj_set_style_pad_all(s_password, 6, 0);
    lv_textarea_set_one_line(s_password, true);
    lv_textarea_set_password_mode(s_password, true);
    lv_textarea_set_max_length(s_password, WIFI_PASSWORD_MAX_BYTES);
    lv_textarea_set_placeholder_text(s_password, "Wi-Fi password");
    lv_obj_add_event_cb(s_password, on_input, LV_EVENT_ALL, NULL);

    lv_obj_t *connect = make_button(s_form, "Connect", 180, 46);
    lv_obj_align(connect, LV_ALIGN_TOP_LEFT, 0, 108);
    lv_obj_add_event_cb(connect, connect_clicked, LV_EVENT_CLICKED, NULL);
    s_message = lv_label_create(s_form);
    lv_obj_set_width(s_message, 560);
    lv_obj_set_style_text_font(s_message, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_message, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_message, LV_ALIGN_TOP_LEFT, 196, 108);

    s_keyboard = lv_keyboard_create(s_form);
    lv_obj_set_size(s_keyboard, lv_pct(100), 240);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
    lv_keyboard_set_textarea(s_keyboard, s_ssid);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);

    s_networks = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_networks);
    lv_obj_set_size(s_networks, lv_pct(100), 410);
    lv_obj_align(s_networks, LV_ALIGN_TOP_LEFT, 0, 44);
    lv_obj_remove_flag(s_networks, LV_OBJ_FLAG_SCROLLABLE);
    s_scan_message = lv_label_create(s_networks);
    lv_obj_set_style_text_font(s_scan_message, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(s_scan_message, lv_pct(100));
    lv_label_set_text(s_scan_message, "Preparing Wi-Fi scan...");

    s_network_list = lv_obj_create(s_networks);
    lv_obj_set_size(s_network_list, lv_pct(100), 308);
    lv_obj_align(s_network_list, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_flex_flow(s_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_network_list, 6, 0);
    lv_obj_set_style_pad_row(s_network_list, 8, 0);
    lv_obj_set_style_bg_color(s_network_list, lv_color_hex(0x142030), 0);

    lv_obj_t *manual = make_button(s_networks, "Manual name", 230, 44);
    lv_obj_align(manual, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(manual, manual_clicked, LV_EVENT_CLICKED, NULL);
    s_rescan = make_button(s_networks, "Rescan", 180, 44);
    lv_obj_align(s_rescan, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(s_rescan, scan_clicked, LV_EVENT_CLICKED, NULL);
    show_networks(NULL);
    scan_clicked(NULL);
    wifi_setup_refresh();
}
