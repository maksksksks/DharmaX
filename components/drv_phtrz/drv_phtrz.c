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

// drv_phtrz.c
bool drv_phtrz_get_data(int *value) {
    int data = 0;
    bsp_phtrz_get_voltage(&data);

    // Если ADC отвалился по таймауту, data будет -1
    if (data < 0) {
        ESP_LOGW(TAG, "ADC read failed (timeout?)");
        return false;
    }

    *value = data;
    return true;
}