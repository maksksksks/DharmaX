#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "app_events.h"

/**
 * @brief Запуск сервиса вибромотора
 * 
 * Создаёт очередь, мьютекс и задачу FreeRTOS.
 * Должен вызываться один раз при старте системы.
 */
void svc_vibro_start(void);

/**
 * @brief Остановка сервиса
 * 
 * Удаляет задачу, очищает ресурсы.
 */
void svc_vibro_stop(void);

/**
 * @brief Установить уровень вибрации напрямую
 * @param level Уровень 0-100%
 */
void svc_vibro_set_level(uint8_t level);

/**
 * @brief Получить текущий уровень
 * @return Уровень 0-100%
 */
uint8_t svc_vibro_get_level(void);

/**
 * @brief Отправить событие в очередь обработки
 * 
 * Используется другими модулями для управления вибромотором.
 * 
 * @param event Тип события (из vibro_event_t)
 * @param level Уровень вибрации для этого события
 * @param duration Длительность (используется некоторыми паттернами)
 * @return true если событие успешно поставлено в очередь
 */
bool svc_vibro_send_event(vibro_event_t event, 
                           vibro_level_t level, 
                           vibro_duration_t duration);