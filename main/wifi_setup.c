/* SPDX-License-Identifier: CC0-1.0 */
#include "wifi_setup.h"
#include "wifi_clock.h"
#include "wifi_credentials.h"

static lv_obj_t *s_overlay;
static lv_obj_t *s_ssid;
static lv_obj_t *s_password;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_message;
static bool s_local_error;

void wifi_setup_refresh(void)
{
    if (s_overlay == NULL || s_local_error) return;
    wifi_clock_info_t info;
    wifi_clock_get_info(&info);
    const char *message;
    switch (info.state) {
    case WIFI_CLOCK_STARTING:
        message = "Wi-Fi is starting. Please wait.";
        break;
    case WIFI_CLOCK_SETUP_REQUIRED:
        message = "Enter a 2.4 GHz Wi-Fi network.\nLeave password empty only for open Wi-Fi.";
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

    lv_obj_t *ssid_label = lv_label_create(s_overlay);
    lv_label_set_text(ssid_label, "SSID");
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 0, 52);
    s_ssid = lv_textarea_create(s_overlay);
    lv_obj_set_size(s_ssid, 610, 44);
    lv_obj_align(s_ssid, LV_ALIGN_TOP_LEFT, 140, 44);
    lv_obj_set_style_pad_all(s_ssid, 6, 0);
    lv_textarea_set_one_line(s_ssid, true);
    lv_textarea_set_max_length(s_ssid, WIFI_SSID_MAX_BYTES);
    lv_textarea_set_placeholder_text(s_ssid, "Wi-Fi network name");
    wifi_clock_info_t info;
    wifi_clock_get_info(&info);
    lv_textarea_set_text(s_ssid, info.ssid);
    lv_obj_add_event_cb(s_ssid, on_input, LV_EVENT_ALL, NULL);

    lv_obj_t *password_label = lv_label_create(s_overlay);
    lv_label_set_text(password_label, "Password");
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 0, 104);
    s_password = lv_textarea_create(s_overlay);
    lv_obj_set_size(s_password, 610, 44);
    lv_obj_align(s_password, LV_ALIGN_TOP_LEFT, 140, 96);
    lv_obj_set_style_pad_all(s_password, 6, 0);
    lv_textarea_set_one_line(s_password, true);
    lv_textarea_set_password_mode(s_password, true);
    lv_textarea_set_max_length(s_password, WIFI_PASSWORD_MAX_BYTES);
    lv_textarea_set_placeholder_text(s_password, "Wi-Fi password");
    lv_obj_add_event_cb(s_password, on_input, LV_EVENT_ALL, NULL);

    lv_obj_t *connect = make_button(s_overlay, "Connect", 180, 46);
    lv_obj_align(connect, LV_ALIGN_TOP_LEFT, 0, 152);
    lv_obj_add_event_cb(connect, connect_clicked, LV_EVENT_CLICKED, NULL);
    s_message = lv_label_create(s_overlay);
    lv_obj_set_width(s_message, 560);
    lv_obj_set_style_text_font(s_message, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_message, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_message, LV_ALIGN_TOP_LEFT, 196, 152);

    s_keyboard = lv_keyboard_create(s_overlay);
    lv_obj_set_size(s_keyboard, lv_pct(100), 240);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(s_keyboard, &lv_font_montserrat_24, LV_PART_ITEMS);
    lv_keyboard_set_textarea(s_keyboard, s_ssid);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_CANCEL, NULL);
    wifi_setup_refresh();
}
