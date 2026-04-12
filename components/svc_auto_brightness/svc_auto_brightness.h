#pragma once

#include <stdbool.h>

/**
 * @file svc_auto_brightness.h
 * @brief Сервис автоматической регулировки яркости экрана
 */

// Запуск сервиса авто-яркости
void svc_auto_brightness_start(void);

// Остановить сервис
void svc_auto_brightness_stop(void);

// Получить текущую авто-яркость (0-100)
int svc_auto_brightness_get_current(void);

// Включить/выключить авто-режим
void svc_auto_brightness_set_enabled(bool enabled);
bool svc_auto_brightness_is_enabled(void);

// ★★★ НОВЫЕ ФУНКЦИИ ДЛЯ КООРДИНАЦИИ СО СНОМ ★★★

/**
 * @brief Приостановить авто-яркость (экран спит)
 * 
 * Вызывается svc_screen_sleep когда экран гаснет.
 * Авто-яркость перестает слать команды на дисплей.
 */
void svc_auto_brightness_suspend(void);

/**
 * @brief Возобновить авто-яркость (экран проснулся)
 * 
 * Вызывается svc_screen_sleep когда экран включается.
 * Авто-яркость снова начинает работать.
 * @param force_update Если true - сразу применить текущую яркость
 */
void svc_auto_brightness_resume(bool force_update);

/**
 * @brief Проверить, приостановлен ли сервис
 */
bool svc_auto_brightness_is_suspended(void);