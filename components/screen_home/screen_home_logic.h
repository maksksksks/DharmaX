#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Строит виджет шагомера внутри ui_Panel1, запускает тикающие часы
// и подписывается на события батареи/шагов/запястья.
// Вызывать один раз из main, после ui_ScreenHome_screen_init() (т.е. после ui_init()).
void screen_home_logic_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
