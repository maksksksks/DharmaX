#ifndef __HOME_TILE_H__
#define __HOME_TILE_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void home_tile_init(lv_obj_t *parent);
void on_screen_wake_update_time(void);

#ifdef __cplusplus
}
#endif

#endif