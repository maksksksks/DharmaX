#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "timeutils", screen_timeutils_create, screen_timeutils_destroy }.
//
// create():  строит дерево ui_ScreenTimeUtils, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_timeutils_create(void);
void screen_timeutils_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
