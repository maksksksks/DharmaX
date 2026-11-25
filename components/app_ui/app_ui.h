#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Единая точка входа UI: тема дисплея, сервисы данных (ленты, триггеры AOD),
// регистрация экранов в app_nav и nav_push("home"). Остальные экраны создаются
// лениво навигатором. Вызвать один раз из app_main() под LVGL-мьютексом,
// после того как LVGL, дисплей и app_core уже проинициализированы.
void app_ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
