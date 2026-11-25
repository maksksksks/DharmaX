#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Запускает приём события календаря в ленту. Вызвать один раз при старте (app_ui_init).
// Подписки сервиса постоянные и не относятся к жизненному циклу экрана.
void screen_events_service_init(void);

// Дескриптор для app_nav: { "events", screen_events_create, screen_events_destroy }.
//
// create():  строит дерево ui_ScreenEvents, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_events_create(void);
void screen_events_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
