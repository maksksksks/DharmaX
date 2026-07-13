#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Сервис сна экрана. В новом проекте отсутствовал, хотя на него уже
 * ссылаются screen_home_logic / screen_aod_logic / screen_settings_logic
 * (перенесено из старой платы) - без этого компонента сборка не слинкуется.
 *
 * Логика: гасит подсветку (через bsp_display) при неактивности
 * (SCREEN_SLEEP_IDLE_TIMEOUT_SEC) или по явному вызову force_sleep(),
 * приостанавливает свc_auto_brightness на время сна, возвращает яркость
 * при wake_up(). AOD (screen_aod_logic) сам решает, показывать ли
 * циферблат вместо гашения - оба механизма подписаны на одни и те же
 * события запястья и работают независимо друг от друга.
 */

// Вызвать один раз из main, после bsp_display_backlight_attach()
// и (опционально) после svc_auto_brightness_start().
void svc_screen_sleep_init(void);

void svc_screen_sleep_wake_up(void);     // разбудить + сбросить таймер бездействия
void svc_screen_sleep_force_sleep(void); // усыпить немедленно

void svc_screen_sleep_set_enabled(bool enabled);
bool svc_screen_sleep_is_enabled(void);
bool svc_screen_sleep_is_sleeping(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
