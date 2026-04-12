#include "drv_phtrz.h"
#include "bsp_phtrz.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DRV_PHT";

void drv_phtrz_init(void) {
    // Если bsp требует инициализации, она уже вызывается в main.
    // Здесь можно оставить пусто.
}

bool drv_phtrz_get_data(int *value) {
    int data = 0;

    // Вызываем без присвоения к esp_err_t, так как в bsp_battery.h она объявлена как void
    bsp_phtrz_get_voltage(&data);

    if (data <= 0) {
        ESP_LOGW(TAG, "Battery voltage is 0 (calibration failed?)");
        return false;
    }

    *value = data;

    return true;
}