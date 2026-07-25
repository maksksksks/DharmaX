#include "adc_shared.h"
#include "esp_log.h"

static const char *TAG = "ADC_SHARED";
static adc_oneshot_unit_handle_t s_handle = NULL;

adc_oneshot_unit_handle_t adc_shared_get_handle(void)
{
    if (s_handle != NULL) return s_handle;

    adc_oneshot_unit_init_cfg_t cfg = { .unit_id = ADC_UNIT_1 };
    esp_err_t err = adc_oneshot_new_unit(&cfg, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return NULL;
    }
    ESP_LOGI(TAG, "ADC1 unit created");
    return s_handle;
}