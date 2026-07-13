#include "drv_phtrz.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// На официальной плате Waveshare ESP32-S3-Touch-AMOLED-1.64 встроенного
// фоторезистора нет (канал 3 в их demo - это системное/аккумуляторное
// напряжение, см. drv_battery.c). Датчик освещённости - ваш собственный,
// внешний, так что канал ниже - ЗАГЛУШКА. Поменяйте на тот, к которому
// реально припаян фоторезистор на вашей плате (любой свободный ADC1-канал,
// кроме занятого батареей CHANNEL_3).
#define PHTRZ_ADC_UNIT     ADC_UNIT_1
#define PHTRZ_ADC_CHANNEL  ADC_CHANNEL_2  // TODO: проверьте под свою распайку
#define PHTRZ_ADC_ATTEN    ADC_ATTEN_DB_12
#define PHTRZ_ADC_BITWIDTH ADC_BITWIDTH_12

static const char *TAG = "DRV_PHT";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_calibrated = false;
static bool s_initialized = false;

void drv_phtrz_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "already initialized");
        return;
    }

    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = PHTRZ_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = PHTRZ_ADC_BITWIDTH,
        .atten = PHTRZ_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, PHTRZ_ADC_CHANNEL, &chan_config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = PHTRZ_ADC_UNIT,
        .chan = PHTRZ_ADC_CHANNEL,
        .atten = PHTRZ_ADC_ATTEN,
        .bitwidth = PHTRZ_ADC_BITWIDTH,
    };
    s_calibrated = (adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle) == ESP_OK);
    if (!s_calibrated) {
        ESP_LOGW(TAG, "ADC calibration недоступна, используем сырые отсчёты");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Photoresistor ADC initialized on channel %d", PHTRZ_ADC_CHANNEL);
}

bool drv_phtrz_get_data(int *value)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "not initialized - call drv_phtrz_init() first");
        return false;
    }

    int adc_raw = 0;
    if (adc_oneshot_read(s_adc_handle, PHTRZ_ADC_CHANNEL, &adc_raw) != ESP_OK) {
        ESP_LOGW(TAG, "adc_oneshot_read failed (timeout?)");
        return false;
    }

    int mv = adc_raw; // если калибровки нет - отдаём сырые отсчёты как есть
    if (s_calibrated) {
        adc_cali_raw_to_voltage(s_cali_handle, adc_raw, &mv);
    }

    // Без множителя x3 (в отличие от батареи) - svc_auto_brightness калиброван
    // именно под сырое напряжение в мВ (BRIGHTNESS_LOW_VALUE=300,
    // BRIGHTNESS_HIGH_VALUE=1700 - это диапазон мВ, не вольт).
    *value = mv;
    return true;
}
