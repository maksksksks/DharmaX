#pragma once

#include <stddef.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * app_nav - навигатор экранов с ленивым созданием/уничтожением.
 *
 * Правила:
 *  - Экран "home" (NAV_HOME_ID) резидентный: создаётся один раз первым
 *    nav_push("home") и никогда не уничтожается навигатором.
 *  - Все остальные экраны создаются в nav_push/nav_replace и уничтожаются в
 *    nav_pop/nav_replace/nav_back_to_home - ПОСЛЕ окончания анимации, когда
 *    экран уже выгружен из дисплея.
 *  - Перед уничтожением навигатор вызывает desc->destroy(scr). Именно destroy()
 *    обязан: снять ВСЕ esp_event подписки экрана, удалить ВСЕ его lv_timer,
 *    обнулить статические/глобальные указатели на его виджеты и удалить сам
 *    экран (lv_obj_del).
 *  - Одновременно существует не больше одного экземпляра каждого экрана
 *    (описания экранов - синглтоны со статическим состоянием). Запрос на
 *    экран, который уже есть в стеке, игнорируется.
 *  - Пока идёт переход (анимация + отложенное уничтожение), новые запросы не
 *    выполняются сразу, а встают в очередь из одного слота (побеждает
 *    последний). Так два перехода никогда не пересекаются.
 *
 * Потоки: все nav_* вызываются только из LVGL-задачи либо под LVGL-мьютексом.
 */

typedef struct {
    const char *id;                  // "home", "notify", "weather", ...
    lv_obj_t *(*create)(void);       // строит дерево LVGL + подписывается на события
    void (*destroy)(lv_obj_t *scr);  // отписывается, удаляет таймеры, lv_obj_del
} screen_desc_t;

#define NAV_HOME_ID          "home"
#define NAV_STACK_MAX        4       // home + максимум 3 экрана поверх
#define NAV_DEFAULT_ANIM_MS  500

// Регистрирует таблицу экранов. registry должен жить всё время работы
// (обычно static const массив). Экраны не создаёт.
void nav_init(const screen_desc_t *registry, size_t count);

// Создаёт целевой экран и кладёт его поверх стека. Текущий экран прячется и
// остаётся жить (его вернёт nav_pop). Первый вызов nav_push("home")
// загружает резидентный home без анимации.
void nav_push(const char *screen_id);

// Уничтожает верхний экран и показывает предыдущий. На home ничего не делает.
void nav_pop(void);

// Уничтожает текущий экран и на его место создаёт новый. Если текущий - home,
// home остаётся жить под новым экраном (как при nav_push).
// nav_replace("home") равнозначен nav_back_to_home().
void nav_replace(const char *screen_id);

// Уничтожает всё, что лежит над home, и показывает home.
void nav_back_to_home(void);

// Те же операции с явной анимацией перехода. anim = LV_SCR_LOAD_ANIM_NONE или
// time_ms = 0 - мгновенная смена. Короткие версии выше используют
// LV_SCR_LOAD_ANIM_NONE.
void nav_push_ex(const char *screen_id, lv_screen_load_anim_t anim, uint32_t time_ms);
void nav_pop_ex(lv_screen_load_anim_t anim, uint32_t time_ms);
void nav_replace_ex(const char *screen_id, lv_screen_load_anim_t anim, uint32_t time_ms);
void nav_back_to_home_ex(lv_screen_load_anim_t anim, uint32_t time_ms);

// id верхнего экрана стека (NULL, если стек пуст). Во время перехода уже
// показывает ЦЕЛЕВОЙ экран.
const char *nav_current_id(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
