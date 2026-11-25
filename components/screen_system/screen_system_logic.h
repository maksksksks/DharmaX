#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "system", screen_system_create, screen_system_destroy }.
//
// create():  строит дерево ui_ScreenSystem, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_system_create(void);
void screen_system_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
