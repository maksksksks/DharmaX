#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Единая точка входа: строит все экраны (ui_init()) и запускает логику
// каждого из них (*_logic_init()). Вызвать один раз из app_main(),
// после того как LVGL, дисплей и app_core уже проинициализированы.
void app_ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
