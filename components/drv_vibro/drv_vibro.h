/**
 * @file drv_vibro.h
 * @brief Драйвер вибромотора 
 */

#pragma once
#include <stdbool.h>
#include <stdio.h>


// ✅ ИСПРАВЛЕНО: Таймер 1 вместо 0!
#define VIB_LEDC_TIMER LEDC_TIMER_1 // Был TIMER_0 → теперь TIMER_1
#define VIB_LEDC_MODE LEDC_LOW_SPEED_MODE
#define VIB_LEDC_CHANNEL LEDC_CHANNEL_1

#define VIB_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define VIB_LEDC_MAX_DUTY ((1 << VIB_LEDC_DUTY_RES) - 1)

#define VIB_LEDC_FREQUENCY 200 // Hz (для ERM/LRA моторов)

#define PIN_VIBRO 18

/* ============================================================
 *  API
 * ============================================================ */

void drv_vibro_init(void);
void drv_vibro_set_level(uint8_t percent);
void drv_vibro_off(void);
bool drv_vibro_is_on(void);
bool drv_vibro_is_initialized(void);
