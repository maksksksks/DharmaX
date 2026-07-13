#pragma once
#include "app_events.h" // vibro_event_t / vibro_level_t / vibro_duration_t уже объявлены тут

#ifdef __cplusplus
extern "C" {
#endif

// Вызвать один раз из main (поднимает drv_vibro + фоновую задачу-плеер паттернов).
void svc_vibro_init(void);

// Неблокирующий вызов - ставит паттерн в очередь, вибро-задача проигрывает
// его в фоне (импульсы вида SINGLE/DOUBLE/TRIPLE x SHORT/LONG).
void svc_vibro_send_event(vibro_event_t event, vibro_level_t level, vibro_duration_t duration);

#ifdef __cplusplus
} /*extern "C"*/
#endif
