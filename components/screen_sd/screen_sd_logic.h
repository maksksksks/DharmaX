#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "sd", screen_sd_create, screen_sd_destroy }.
//
// create():  строит дерево ui_ScreenSD, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_sd_create(void);
void screen_sd_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
