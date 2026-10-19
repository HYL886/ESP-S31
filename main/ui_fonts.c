#include "ui_fonts.h"

LV_FONT_DECLARE(s31_cn_16);
LV_FONT_DECLARE(s31_cn_24);
LV_FONT_DECLARE(s31_cn_32);

static lv_font_t s_normal;
static lv_font_t s_large;

void ui_fonts_init(void)
{
    /* Latin characters and symbols keep their existing Montserrat appearance. */
    s_normal = lv_font_montserrat_24;
    s_normal.fallback = &s31_cn_24;
    s_large = lv_font_montserrat_32;
    s_large.fallback = &s31_cn_32;
}

const lv_font_t *ui_font_small(void) { return &s31_cn_16; }
const lv_font_t *ui_font_normal(void) { return &s_normal; }
const lv_font_t *ui_font_large(void) { return &s_large; }
