#pragma once

#include <stdbool.h>

void drv_ds18b20_init();

void drv_ds18b20_start_conversion();
bool drv_ds18b20_read_temp(float *temp);