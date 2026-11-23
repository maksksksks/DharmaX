#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
