#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "home", screen_home_create, screen_home_destroy }.
//
// create():  строит дерево ui_ScreenHome, достраивает виджет шагомера, запускает
//            часы и подписывается на события батареи/шагов/запястья.
// destroy(): снимает ВСЕ подписки, удаляет таймер, обнуляет указатели, удаляет экран.
//
// Home резидентный: создаётся один раз через nav_push("home") и навигатором
// никогда не уничтожается.
lv_obj_t *screen_home_create(void);
void screen_home_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
