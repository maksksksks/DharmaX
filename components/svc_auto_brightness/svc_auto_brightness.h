#pragma once

#include <stdbool.h>

/**
 * @file svc_auto_brightness.h
 * @brief Сервис автоматической регулировки яркости экрана
 * 
 * Калибровка:
 *   - 300 lux  → 2%  (темнота)
 *   - 1700 lux → 50% (яркий свет)
 *   - Ограничение: max 80%
 */

// Запуск сервиса авто-яркости
void svc_auto_brightness_start(void);

// Остановить авто-яркость (вернуть ручной режим)
void svc_auto_brightness_stop(void);

// Получить текущую авто-яркость (0-100)
int svc_auto_brightness_get_current(void);

// Включить/выключить авто-режим
void svc_auto_brightness_set_enabled(bool enabled);
bool svc_auto_brightness_is_enabled(void);