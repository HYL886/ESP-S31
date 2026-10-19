#pragma once

#include "lvgl.h"

/* Initialize once under the display lock, before creating any UI objects. */
void ui_fonts_init(void);
const lv_font_t *ui_font_small(void);
const lv_font_t *ui_font_normal(void);
const lv_font_t *ui_font_large(void);
