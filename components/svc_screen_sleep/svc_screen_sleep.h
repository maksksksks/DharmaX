#pragma once

#include <stdio.h>
#include <stdbool.h>

/**
 * @file svc_screen_sleep.h
 * @brief Сервис автовыключения экрана (как в smartwatch)
 *
 * Функционал:
 *   - Экран гаснет через N секунд после последнего касания
 *   - Любое касание включает экран и обновляет время
 *   - Настраиваемое время таймаута
 */

// Запуск сервиса автосна
void svc_screen_sleep_start(void);

// Остановить сервис (экран всегда включен)
void svc_screen_sleep_stop(void);

// Установить таймаут в секундах (по умолчанию: 20)
void svc_screen_sleep_set_timeout(uint32_t timeout_sec);

// Получить текущий таймаут
uint32_t svc_screen_sleep_get_timeout(void);

// Принудительно включить экран (сброс таймера)
void svc_screen_sleep_wake_up(void);

// Принудительно выключить экран
void svc_screen_sleep_force_sleep(void);

// Проверить, спит ли экран сейчас
bool svc_screen_sleep_is_sleeping(void);

// Включить/выключить авто-режим
void svc_screen_sleep_set_enabled(bool enabled);

// Проверить включен ли авто-режим
bool svc_screen_sleep_is_enabled(void);

// Зарегистрировать callback на пробуждение (для обновления UI)
typedef void (*screen_wake_callback_t)(void);
void svc_screen_sleep_register_wake_callback(screen_wake_callback_t cb);