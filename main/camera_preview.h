/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "lvgl.h"

/* Mount in a 352x360 home-page panel and start preview automatically.
 * Call once on the LVGL task or with the display locked. */
void camera_preview_create(lv_obj_t *parent);
