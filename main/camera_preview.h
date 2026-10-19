/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "lvgl.h"

/* LVGL event callback. Call only on the LVGL task or with the display locked. */
void camera_preview_open(lv_event_t *event);
