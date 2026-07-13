#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Регистрирует обработчики EVT_MOTION_WRIST_SLEEP / EVT_MOTION_WRIST_RAISE
// и подключает автообновление стрелок AOD (раз в секунду, только пока
// экран AOD активен - в остальное время таймер остановлен и не тратит CPU).
void screen_aod_logic_init(void);

// Можно вызывать и напрямую (например, из svc_screen_sleep по таймеру
// бездействия, если он у вас не завязан на события wrist raise/sleep).
void screen_aod_enter(void);   // войти в режим AOD (запомнит текущий экран)
void screen_aod_exit(void);    // выйти из AOD, вернуться на запомненный экран

#ifdef __cplusplus
} /*extern "C"*/
#endif
