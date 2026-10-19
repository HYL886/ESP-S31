#pragma once
#include "lvgl.h"

/* Both functions run on the LVGL task, never on the network task. */
void wifi_setup_open(lv_event_t *event);
void wifi_setup_refresh(void);
