#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#include <stdio.h>
#include "lvgl.h"


#ifdef __cplusplus
extern "C" {
#endif

void lvgl_ui_init(void);
void show_pairing_popup(const char * code);
void close_pairing_popup_cb(lv_event_t * e);
bool get_popup_status();

LV_IMG_DECLARE(bg_home);

#ifdef __cplusplus
}
#endif

#endif

