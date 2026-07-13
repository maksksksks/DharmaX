#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Достраивает вкладку с лентой карточек внутри ui_TabView1 и подписывается
// на EVT_BLE_NOTIFY_RX / EVT_BLE_NOTIFY_DEL.
// Вызывать один раз из main, после ui_init().
void screen_notify_logic_init(void);

void ui_ScreenNotify_full_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
