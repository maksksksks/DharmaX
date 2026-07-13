#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/**
* Инициализация логики экрана "ScreenSystem".
* Вызывается один раз из main после ui_init() (когда ui_ScreenSystem уже создан).
* Здесь: регистрация esp_event хендлеров, lv_timer, привязка к сервисам app_core.
*/
void screen_system_logic_init(void);

// Обёртка для ленивой инициализации: создаёт экран + запускает логику.
void ui_ScreenSystem_full_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif