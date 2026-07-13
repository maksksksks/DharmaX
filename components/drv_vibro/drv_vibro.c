/**
 * @file drv_vibro.c
 * @brief Драйвер вибромотора - исправленная версия
 *
 * ГЛАВОЕ ПРАВИЛО: Никогда не вызывать gpio_config() на PWM-пине!
 * Это разрушает связь GPIO ↔ LEDC module.
 */

#include "drv_vibro.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "DRV_VIB";

/* ============================================================
 *  СОСТОЯНИЕ
 * ============================================================ */

static bool s_initialized = false;
static uint8_t s_current_percent = 0;

/* ============================================================
 *  ИНИЦИАЛИЗАЦИЯ
 * ============================================================ */

void drv_vibro_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized, skipping");
        return;
    }

    // ESP_LOGI(TAG, "Initializing vibration motor...");
    // ESP_LOGI(TAG, "  GPIO Pin    : %d", PIN_VIBRO);
    // ESP_LOGI(TAG, "  Frequency   : %d Hz", VIB_LEDC_FREQUENCY);
    // ESP_LOGI(TAG, "  Resolution  : %d bits (%d levels)", VIB_LEDC_DUTY_RES, (1 << VIB_LEDC_DUTY_RES));

    esp_err_t ret;

    // ─── 1. Таймер LEDC ───
    ledc_timer_config_t timer_cfg = {.speed_mode = VIB_LEDC_MODE,
                                     .timer_num = VIB_LEDC_TIMER,
                                     .duty_resolution = VIB_LEDC_DUTY_RES,
                                     .freq_hz = VIB_LEDC_FREQUENCY,
                                     .clk_cfg = LEDC_AUTO_CLK};

    ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer config failed: %s", esp_err_to_name(ret));
        return;
    }
    // ESP_LOGI(TAG, "Timer ✓");

    // ─── 2. Канал LEDC (привязка к GPIO!) ───
    ledc_channel_config_t channel_cfg = {.speed_mode = VIB_LEDC_MODE,
                                         .channel = VIB_LEDC_CHANNEL,
                                         .timer_sel = VIB_LEDC_TIMER,
                                         .intr_type = LEDC_INTR_DISABLE,
                                         .gpio_num = PIN_VIBRO, // ← Здесь GPIO привязывается к LEDC
                                         .duty = 0,             // ← Старт с 0%
                                         .hpoint = 0};

    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Channel config failed: %s (GPIO %d busy?)", esp_err_to_name(ret), PIN_VIBRO);
        return;
    }
    // ESP_LOGI(TAG, "Channel on GPIO %d ✓", PIN_VIBRO);

    // ─── 3. Применяем начальный duty = 0 ───
    ret = ledc_set_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL, 0);
    ESP_ERROR_CHECK(ret);

    ret = ledc_update_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL);
    ESP_ERROR_CHECK(ret);

    s_current_percent = 0;
    s_initialized = true;

    ESP_LOGI(TAG, "Driver initialized ✓ (MOTOR OFF)");
}

/* ============================================================
 *  УРОВЕНЬ ВИБРАЦИИ
 * ============================================================ */

void drv_vibro_set_level(uint8_t percent) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized!");
        return;
    }

    // Clamp
    if (percent > 100) {
        percent = 100;
    }

    // Особый случай: 0 = off
    if (percent == 0) {
        drv_vibro_off();
        return;
    }

    // Расчёт duty: percent=100 → max_duty
    uint32_t max_duty = (1 << VIB_LEDC_DUTY_RES) - 1; // 1023 for 10-bit
    uint32_t duty = (percent * max_duty) / 100;

    // ESP_LOGI(TAG, "SET %d%% → duty=%lu/%d", (unsigned long)percent, (unsigned long)duty, max_duty);

    // Устанавливаем duty
    esp_err_t ret = ledc_set_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_duty failed: %s", esp_err_to_name(ret));
        return;
    }

    // Применяем (это заставляет LEDC обновить выход!)
    ret = ledc_update_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "update_duty failed: %s", esp_err_to_name(ret));
        return;
    }

    s_current_percent = percent;
}

/* ============================================================
 *  ВЫКЛЮЧЕНИЕ (без gpio_config!!!)
 * ============================================================ */

void drv_vibro_off(void) {
    if (!s_initialized) {
        return;
    }

    // ✅ ПРАВИЛЬНЫЙ СПОСОБ: duty = 0 через LEDC API!
    // НЕ используем gpio_config() - это убьёт PWM!

    esp_err_t ret = ledc_set_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "off:set_duty failed: %s", esp_err_to_name(ret));
    }

    ret = ledc_update_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "off:update_duty failed: %s", esp_err_to_name(ret));
    }

    s_current_percent = 0;

    // ESP_LOGD(TAG, "OFF (duty=0 via LEDC, GPIO still bound to PWM)");
}

/* ============================================================
 *  GETTERS
 * ============================================================ */

bool drv_vibro_is_on(void) { return s_current_percent > 0; }

bool drv_vibro_is_initialized(void) { return s_initialized; }


/**
 * @brief Прочитать фактический duty из LEDC регистра
 * 
 * Для отладки - показывает ЧТО НА САМОМ ДЕЛЕ в регистре!
 */
uint32_t drv_vibro_get_actual_duty(void) {
    if (!s_initialized) return 0;
    
    uint32_t actual_duty = ledc_get_duty(VIB_LEDC_MODE, VIB_LEDC_CHANNEL);
    
    return actual_duty;
}