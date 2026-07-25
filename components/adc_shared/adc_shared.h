#pragma once
#include "esp_adc/adc_oneshot.h"
#include <stdbool.h>

// Инициализировать ADC1 один раз (повторные вызовы безопасны).
// Возвращает handle или NULL при ошибке.
adc_oneshot_unit_handle_t adc_shared_get_handle(void);