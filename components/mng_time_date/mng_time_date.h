#pragma once

// Инициализация таймера обновления времени (вызывать после ui_init)
void home_tile_time_init(void);

// Функция для мгновенного обновления времени при пробуждении экрана
void on_screen_wake_update_time(void);