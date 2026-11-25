#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Дескриптор для app_nav: { "aod", screen_aod_create, screen_aod_destroy }.
// create() строит часы и запускает секундный таймер, destroy() его удаляет.
lv_obj_t *screen_aod_create(void);
void screen_aod_destroy(lv_obj_t *scr);

// Регистрирует постоянные триггеры EVT_MOTION_WRIST_SLEEP / EVT_MOTION_WRIST_RAISE.
// Они не принадлежат экрану и живут весь uptime; сами экран не создают, а
// просто зовут screen_aod_enter()/screen_aod_exit().
void screen_aod_logic_init(void);

// AOD - оверлей: enter = nav_push("aod"), exit = nav_pop() (вернёт тот экран,
// с которого уходили). Можно вызывать из svc_screen_sleep. Идемпотентны.
void screen_aod_enter(void);
void screen_aod_exit(void);

// Выйти из AOD сразу на home (касание по экрану AOD).
void screen_aod_exit_to_home(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
