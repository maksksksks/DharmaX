#include "drv_battery.h"
#include "adc_shared.h"  
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// Раньше тут был отдельный bsp_battery.c/h (перенесённый со старой платы) -
// на новой плате в этом промежуточном слое смысла нет, читаем АЦП прямо
// здесь. Канал/атт/битность - как в вашем новом adc_bsp.c примере.
#define BATTERY_ADC_UNIT     ADC_UNIT_1
#define BATTERY_ADC_CHANNEL  ADC_CHANNEL_3
#define BATTERY_ADC_ATTEN    ADC_ATTEN_DB_12
#define BATTERY_ADC_BITWIDTH ADC_BITWIDTH_12

static const char *TAG = "DRV_BAT";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_calibrated = false;
static bool s_initialized = false;

// Ваша таблица интерполяции напряжение -> проценты, без изменений.
static const float voltage_table[] = {
    4.14f, 4.13f, 4.12f, 4.11f, 4.10f, 4.09f, 4.08f, 4.07f, 4.06f, 4.05f, 4.04f, 4.03f, 4.02f, 4.01f, 4.00f,
    3.99f, 3.98f, 3.97f, 3.96f, 3.95f, 3.94f, 3.93f, 3.92f, 3.91f, 3.90f, 3.89f, 3.88f, 3.87f, 3.86f, 3.85f,
    3.84f, 3.83f, 3.82f, 3.81f, 3.80f, 3.79f, 3.78f, 3.77f, 3.76f, 3.75f, 3.74f, 3.73f, 3.72f, 3.71f, 3.70f,
    3.69f, 3.68f, 3.67f, 3.66f, 3.65f, 3.64f, 3.63f, 3.62f, 3.61f, 3.60f, 3.59f, 3.58f, 3.57f, 3.56f, 3.55f,
    3.54f, 3.53f, 3.52f, 3.51f, 3.50f, 3.49f, 3.48f, 3.47f, 3.46f, 3.45f, 3.44f, 3.43f, 3.42f, 3.41f, 3.40f,
    3.39f, 3.38f, 3.37f, 3.36f, 3.35f, 3.34f, 3.33f, 3.32f, 3.31f, 3.30f, 3.28f, 3.26f, 3.24f, 3.22f, 3.20f,
    3.18f, 3.16f, 3.14f, 3.12f, 3.10f, 3.05f, 3.00f, 2.95f, 2.90f, 2.80f, 2.70f, 2.60f, 2.50f};

static const float percent_table[] = {
    100.0f, 99.0f, 98.0f, 97.0f, 96.0f, 95.0f, 94.0f, 93.0f, 92.0f, 91.0f, 90.0f, 89.0f, 88.0f, 87.0f, 86.0f,
    85.0f,  84.0f, 83.0f, 82.0f, 81.0f, 80.0f, 79.0f, 78.0f, 77.0f, 76.0f, 75.0f, 74.0f, 73.0f, 72.0f, 71.0f,
    70.0f,  69.0f, 68.0f, 67.0f, 66.0f, 65.0f, 64.0f, 63.0f, 62.0f, 61.0f, 60.0f, 59.0f, 58.0f, 57.0f, 56.0f,
    55.0f,  54.0f, 53.0f, 52.0f, 51.0f, 50.0f, 49.0f, 48.0f, 47.0f, 46.0f, 45.0f, 44.0f, 43.0f, 42.0f, 41.0f,
    40.0f,  39.0f, 38.0f, 37.0f, 36.0f, 35.0f, 34.0f, 33.0f, 32.0f, 31.0f, 30.0f, 29.0f, 28.0f, 27.0f, 26.0f,
    25.0f,  24.0f, 23.0f, 22.0f, 21.0f, 20.0f, 19.0f, 18.0f, 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f,
    10.0f,  9.0f,  8.0f,  7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f,  1.0f,  0.5f,  0.2f,  0.0f};

#define TABLE_SIZE (sizeof(voltage_table) / sizeof(voltage_table[0]))

void drv_battery_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "already initialized");
        return;
    }

    // ★ Берём общий handle вместо adc_oneshot_new_unit()
    s_adc_handle = adc_shared_get_handle();
    if (s_adc_handle == NULL) {
        ESP_LOGE(TAG, "ADC1 not available");
        return;
    }

    // Конфигурируем ТОЛЬКО свой канал
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = BATTERY_ADC_BITWIDTH,
        .atten = BATTERY_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_config));

    // Калибровка (без изменений)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    s_calibrated = (adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle) == ESP_OK);
    if (!s_calibrated) {
        ESP_LOGW(TAG, "ADC calibration unavailable");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Battery ADC initialized on channel %d", BATTERY_ADC_CHANNEL);
}

bool drv_battery_get_data(int *percent, float *voltage)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "not initialized - call drv_battery_init() first");
        return false;
    }

    int adc_raw = 0;
    if (adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &adc_raw) != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_read failed");
        return false;
    }

    float v;
    if (s_calibrated) {
        int mv = 0;
        adc_cali_raw_to_voltage(s_cali_handle, adc_raw, &mv);
        // Тот же коэффициент x3, что в вашем новом adc_bsp.c примере -
        // компенсация делителя напряжения на плате.
        v = (mv / 1000.0f) * 3.0f;
    } else {
        v = ((float)adc_raw * 3.3f / 4096.0f) * 3.0f;
    }

    if (v <= 0.0f) {
        ESP_LOGW(TAG, "Battery voltage is 0 (АЦП не откалиброван или обрыв цепи?)");
        return false;
    }

    int p = 0;
    if (v >= voltage_table[0]) {
        p = 100;
    } else if (v <= voltage_table[TABLE_SIZE - 1]) {
        p = 0;
    } else {
        for (size_t i = 0; i < TABLE_SIZE - 1; i++) {
            if (v <= voltage_table[i] && v >= voltage_table[i + 1]) {
                p = (int)(percent_table[i + 1] +
                          (v - voltage_table[i + 1]) * (percent_table[i] - percent_table[i + 1]) /
                              (voltage_table[i] - voltage_table[i + 1]) +
                          0.5f);
                break;
            }
        }
    }

    if (percent) *percent = p;
    if (voltage) *voltage = v;
    return true;
}
