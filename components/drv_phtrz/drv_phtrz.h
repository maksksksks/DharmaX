#pragma once
#include <stdbool.h>

// Инициализация (если нужна, пока можно оставить пустой)
void drv_phtrz_init(void);

// Функция получения данных. Возвращает false, если прочитать не удалось (например Wi-Fi мешает)
bool drv_phtrz_get_data(int *value);