#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Запускает приём новости в ленту. Вызвать один раз при старте (app_ui_init).
// Подписки сервиса постоянные и не относятся к жизненному циклу экрана.
void screen_news_service_init(void);

// Дескриптор для app_nav: { "news", screen_news_create, screen_news_destroy }.
//
// create():  строит дерево ui_ScreenNews, достраивает логику, подписывается на события.
// destroy(): снимает ВСЕ подписки и lv_timer экрана, обнуляет указатели на его
//            виджеты и удаляет сам экран. Вызывается только навигатором.
lv_obj_t *screen_news_create(void);
void screen_news_destroy(lv_obj_t *scr);

#ifdef __cplusplus
} /*extern "C"*/
#endif
