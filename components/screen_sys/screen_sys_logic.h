#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "sys", screen_sys_create, screen_sys_destroy }.
//
// create():  строит дерево ui_ScreenSys, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_sys_create(void);
void screen_sys_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
