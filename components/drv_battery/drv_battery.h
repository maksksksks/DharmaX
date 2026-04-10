#pragma once
#include <stdbool.h>

// Инициализация (если нужна, пока можно оставить пустой)
void drv_battery_init(void);

// Функция получения данных. Возвращает false, если прочитать не удалось (например Wi-Fi мешает)
bool drv_battery_get_data(int *percent, float *voltage);