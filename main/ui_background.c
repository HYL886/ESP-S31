/* SPDX-License-Identifier: CC0-1.0 */
#include "ui_background.h"
#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"

#define BACKGROUND_WIDTH 800
#define BACKGROUND_HEIGHT 480
#define BACKGROUND_BYTES (BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 2)

/* background_asset.S.in exports 4-byte-aligned, read-only pixel bounds. */
extern const uint8_t background_start[] asm("_binary_home_background_rgb565_start");
extern const uint8_t background_end[] asm("_binary_home_background_rgb565_end");
static lv_image_dsc_t s_background;

void ui_background_create(lv_obj_t *parent)
{
    size_t length = (size_t)(background_end - background_start);
    if (length != BACKGROUND_BYTES) {
        ESP_LOGE("ui_background", "Expected %u background bytes, got %u",
                 (unsigned)BACKGROUND_BYTES, (unsigned)length);
        return;
    }
    s_background = (lv_image_dsc_t){
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                   .w = BACKGROUND_WIDTH, .h = BACKGROUND_HEIGHT,
                   .stride = BACKGROUND_WIDTH * 2},
        .data_size = BACKGROUND_BYTES,
        .data = background_start,
    };
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, &s_background);
    lv_obj_set_pos(image, 0, 0);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(image, 0);
}
