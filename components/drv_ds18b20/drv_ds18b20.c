#include "drv_ds18b20.h"
//#include "driver/temperature_sensor.h"
#include "ds18b20.h"
#include "onewire_bus.h"
#include <stdbool.h>

#define DS18B20_GPIO 6
onewire_bus_handle_t bus;
ds18b20_device_handle_t ds18b20;
//static float ds18_last_temp = 0;

void drv_ds18b20_init() {
    /* ---------- 1-Wire bus ---------- */
    onewire_bus_config_t bus_config = {.bus_gpio_num = DS18B20_GPIO};
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, // достаточно для DS18B20
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    /* ---------- DS18B20 ---------- */
    ds18b20_config_t ds18b20_cfg = {};
    ESP_ERROR_CHECK(ds18b20_new_device_from_bus(bus, &ds18b20_cfg, &ds18b20));
    ds18b20_set_resolution(ds18b20, DS18B20_RESOLUTION_10B);
}

void drv_ds18b20_start_conversion() {
    ds18b20_trigger_temperature_conversion(ds18b20);
}

bool drv_ds18b20_read_temp(float *temp) {
    float cur_temp;
    if (ds18b20_get_temperature(ds18b20, &cur_temp) == ESP_OK) {
        *temp = cur_temp;
        return true;
    }
    return false;
}