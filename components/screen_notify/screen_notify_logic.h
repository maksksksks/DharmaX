#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Запускает приём уведомления в ленту. Вызвать один раз при старте (app_ui_init).
// Подписки сервиса постоянные и не относятся к жизненному циклу экрана.
void screen_notify_service_init(void);

// Дескриптор для app_nav: { "notify", screen_notify_create, screen_notify_destroy }.
//
// create():  строит дерево ui_ScreenNotify, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_notify_create(void);
void screen_notify_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
