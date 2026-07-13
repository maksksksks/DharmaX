#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Строит UI плеера поверх ui_ScreenPhone (заголовок трека, исполнитель,
// play/pause + prev/next, прогресс-бар) и подписывается на
// EVT_BLE_MEDIA_UPDATED (см. docs/ble_protocol_additions.md).
void screen_phone_logic_init(void);

// Обёртка для ленивой инициализации: создаёт экран + запускает логику.
void ui_ScreenPhone_full_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif