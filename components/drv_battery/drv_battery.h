#pragma once
#include <stdbool.h>

void drv_battery_init(void);

// percent: 0-100, voltage: вольты (с учётом делителя на плате).
// Возвращает false, если чтение АЦП не удалось.
bool drv_battery_get_data(int *percent, float *voltage);
