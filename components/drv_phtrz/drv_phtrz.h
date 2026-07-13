#pragma once
#include <stdbool.h>

void drv_phtrz_init(void);

// Функция получения данных. Возвращает false, если прочитать не удалось.
bool drv_phtrz_get_data(int *value);
