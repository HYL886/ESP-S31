/* SPDX-License-Identifier: CC0-1.0 */

#include <inttypes.h>
#include <time.h>
#include "bsp/esp32_s31_korvo_1.h"
#include "esp_log.h"
#include "lvgl.h"
#include "wifi_clock.h"
#include "wifi_setup.h"
#include "ui_fonts.h"
#include "camera_preview.h"

static const char *TAG = "s31_display";
static lv_obj_t *count_label;
static uint32_t tap_count;

/* LVGL calls this on its own task; an extra display lock is not needed here. */
static void on_button_clicked(lv_event_t *event)
{
    (void)event;
    tap_count +=10;
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
    lv_obj_t *label = lv_timer_get_user_data(timer);
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
        lv_label_set_text(label, message);
        return;
    }

    char text[32];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &current_time);
    lv_label_set_text(label, text);
}

static void create_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x142030), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_font(screen, ui_font_normal(), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "My first screen");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 64);

    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "等待网络校时…");
    lv_obj_set_style_text_font(subtitle, ui_font_normal(), 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA8BED2), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 128);
    lv_timer_create(on_timer, 1000, subtitle);

    count_label = lv_label_create(screen);
    lv_label_set_text(count_label, "计数：0");
    lv_obj_set_style_text_font(count_label, ui_font_large(), 0);
    lv_obj_set_width(count_label, 700);
    lv_obj_set_style_text_align(count_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(count_label, LV_ALIGN_TOP_MID, 0, 214);

    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 280, 82);
    lv_obj_align(button, LV_ALIGN_TOP_MID, -160, 290);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x168AAD), 0);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_add_event_cb(button, on_button_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "加 10");
    lv_obj_set_style_text_font(button_label, ui_font_large(), 0);
    lv_obj_center(button_label);

    lv_obj_t *reset_button = lv_button_create(screen);
    lv_obj_set_size(reset_button, 280, 82);
    lv_obj_align(reset_button, LV_ALIGN_TOP_MID, 160, 290);
    lv_obj_add_event_cb(reset_button, on_reset_clicked,
                        LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "清零");
    lv_obj_set_style_text_font(reset_label,
                            ui_font_large(), 0);
    lv_obj_center(reset_label);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "右上角可设置网络或打开摄像头");
    lv_obj_set_style_text_font(footer, ui_font_normal(), 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0xA8BED2), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t *wifi_button = lv_button_create(screen);
    lv_obj_set_size(wifi_button, 120, 44);
    lv_obj_align(wifi_button, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_add_event_cb(wifi_button, wifi_setup_open, LV_EVENT_CLICKED, NULL);
    lv_obj_t *wifi_label = lv_label_create(wifi_button);
    lv_label_set_text(wifi_label, "网络");
    lv_obj_set_style_text_font(wifi_label, ui_font_normal(), 0);
    lv_obj_center(wifi_label);

    lv_obj_t *camera_button = lv_button_create(screen);
    lv_obj_set_size(camera_button, 136, 44);
    lv_obj_align(camera_button, LV_ALIGN_TOP_RIGHT, -150, 14);
    lv_obj_add_event_cb(camera_button, camera_preview_open, LV_EVENT_CLICKED, NULL);
    lv_obj_t *camera_label = lv_label_create(camera_button);
    lv_label_set_text(camera_label, "摄像头");
    lv_obj_set_style_text_font(camera_label, ui_font_normal(), 0);
    lv_obj_center(camera_label);
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

    /* Networking runs through events; the screen stays responsive. */
    wifi_clock_start();

    /* This board has no software backlight brightness control. */
    ESP_LOGI(TAG, "Screen ready. Tap the button to test touch.");
}
