/* SPDX-License-Identifier: CC0-1.0 */

#include <inttypes.h>
#include <time.h>
#include "bsp/esp32_s31_korvo_1.h"
#include "esp_log.h"
#include "lvgl.h"
#include "camera_preview.h"
#include "ui_background.h"
#include "ui_fonts.h"
#include "wifi_clock.h"
#include "wifi_setup.h"

static const char *TAG = "s31_display";
static lv_obj_t *count_label;
static lv_obj_t *date_label;
static uint32_t tap_count;

/* LVGL calls this on its own task; an extra display lock is not needed here. */
static void on_button_clicked(lv_event_t *event)
{
    (void)event;
    tap_count += 10;
    lv_label_set_text_fmt(count_label, "计数：%" PRIu32, tap_count);
    ESP_LOGI(TAG, "Touch works! Count: %" PRIu32, tap_count);
}

static void on_reset_clicked(lv_event_t *event)
{
    (void)event;
    tap_count = 0;
    lv_label_set_text(count_label, "计数：0");
}

static void on_timer(lv_timer_t *timer)
{
    wifi_setup_refresh();
    lv_obj_t *clock_label = lv_timer_get_user_data(timer);
    time_t now = time(NULL);
    struct tm current_time;
    localtime_r(&now, &current_time);

    /* Before the first successful time sync, the clock normally starts at 1970. */
    if (current_time.tm_year < (2024 - 1900)) {
        wifi_clock_info_t info;
        wifi_clock_get_info(&info);
        const char *message = "正在连接 Wi-Fi…";
        if (info.connected) {
            message = "Wi-Fi 已连接，正在校时…";
        } else if (info.state == WIFI_CLOCK_SETUP_REQUIRED) {
            message = "点击“网络”设置 Wi-Fi";
        } else if (info.state == WIFI_CLOCK_CONNECT_FAILED) {
            message = "连接失败，请重新设置网络";
        }
        lv_label_set_text(clock_label, "--:--:--");
        lv_label_set_text(date_label, message);
        return;
    }

    char text[32];
    strftime(text, sizeof(text), "%H:%M:%S", &current_time);
    lv_label_set_text(clock_label, text);
    strftime(text, sizeof(text), "%Y-%m-%d", &current_time);
    lv_label_set_text(date_label, text);
}

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x071E27), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xD9C29A), 0);
    lv_obj_set_style_border_opa(panel, LV_OPA_30, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void make_count_button(lv_obj_t *parent, const char *text, int x,
                              lv_event_cb_t callback, uint32_t colour)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 244);
    lv_obj_set_size(button, 168, 64);
    lv_obj_set_style_bg_color(button, lv_color_hex(colour), 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, ui_font_large(), 0);
    lv_obj_center(label);
}

static void create_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x071E27), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_font(screen, ui_font_normal(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_background_create(screen);

    lv_obj_t *header = make_panel(screen, 16, 16, 768, 72);
    make_label(header, "My first screen", &lv_font_montserrat_32, 18, 17);
    lv_obj_t *wifi_button = lv_button_create(header);
    lv_obj_set_size(wifi_button, 112, 44);
    lv_obj_align(wifi_button, LV_ALIGN_TOP_RIGHT, -14, 13);
    lv_obj_set_style_bg_color(wifi_button, lv_color_hex(0x137B8E), 0);
    lv_obj_set_style_shadow_width(wifi_button, 0, 0);
    lv_obj_add_event_cb(wifi_button, wifi_setup_open, LV_EVENT_CLICKED, NULL);
    lv_obj_t *wifi_label = lv_label_create(wifi_button);
    lv_label_set_text(wifi_label, "网络");
    lv_obj_center(wifi_label);

    lv_obj_t *controls = make_panel(screen, 16, 104, 400, 360);
    lv_obj_t *caption = make_label(controls, "网络时钟", ui_font_small(), 24, 20);
    lv_obj_set_style_text_color(caption, lv_color_hex(0xD9C29A), 0);
    lv_obj_t *clock_label = make_label(controls, "--:--:--", &lv_font_montserrat_48, 24, 46);
    date_label = make_label(controls, "等待网络校时…", ui_font_small(), 24, 114);
    lv_obj_set_width(date_label, 352);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xD5E5E9), 0);
    lv_label_set_long_mode(date_label, LV_LABEL_LONG_WRAP);
    lv_timer_create(on_timer, 1000, clock_label);

    lv_obj_t *divider = lv_obj_create(controls);
    lv_obj_remove_style_all(divider);
    lv_obj_set_pos(divider, 24, 166);
    lv_obj_set_size(divider, 352, 1);
    lv_obj_set_style_bg_opa(divider, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xD9C29A), 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    count_label = make_label(controls, "计数：0", ui_font_large(), 24, 184);
    lv_obj_set_width(count_label, 352);
    make_count_button(controls, "加 10", 24, on_button_clicked, 0x137B8E);
    make_count_button(controls, "清零", 208, on_reset_clicked, 0x294550);
    caption = make_label(controls, "点击加数，随时清零", ui_font_small(), 24, 324);
    lv_obj_set_style_text_color(caption, lv_color_hex(0xD5E5E9), 0);

    lv_obj_t *camera_panel = make_panel(screen, 432, 104, 352, 360);
    camera_preview_create(camera_panel);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting the 800x480 screen and touch controller");
    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "Display or touch initialization failed; check the LCD connection");
        return;
    }

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Could not lock the display");
        return;
    }
    ui_fonts_init();
    create_screen();
    bsp_display_unlock();

    wifi_clock_start();
    ESP_LOGI(TAG, "Home screen ready; camera preview starts in its own task.");
}
