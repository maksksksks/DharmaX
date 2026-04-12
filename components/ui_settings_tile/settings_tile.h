#pragma once

#include "lvgl.h"

// Глобальные объекты (для доступа извне если нужно)
extern lv_obj_t *sw_auto_brightness;
extern lv_obj_t *sw_screen_sleep;
extern lv_obj_t *sw_battery_monitor;
extern lv_obj_t *sw_lighting_monitor;
extern lv_obj_t *sw_ble_sync;

// Инициализация тайла настроек
void settings_tile_init(lv_obj_t *parent);