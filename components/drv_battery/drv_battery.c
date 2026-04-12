#include "drv_battery.h"
#include "bsp_battery.h"
#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DRV_BAT";

static const float voltage_table[] = {
    4.14f, 4.13f, 4.12f, 4.11f, 4.10f, 4.09f, 4.08f, 4.07f, 4.06f, 4.05f, 4.04f, 4.03f, 4.02f, 4.01f, 4.00f,
    3.99f, 3.98f, 3.97f, 3.96f, 3.95f, 3.94f, 3.93f, 3.92f, 3.91f, 3.90f, 3.89f, 3.88f, 3.87f, 3.86f, 3.85f,
    3.84f, 3.83f, 3.82f, 3.81f, 3.80f, 3.79f, 3.78f, 3.77f, 3.76f, 3.75f, 3.74f, 3.73f, 3.72f, 3.71f, 3.70f,
    3.69f, 3.68f, 3.67f, 3.66f, 3.65f, 3.64f, 3.63f, 3.62f, 3.61f, 3.60f, 3.59f, 3.58f, 3.57f, 3.56f, 3.55f,
    3.54f, 3.53f, 3.52f, 3.51f, 3.50f, 3.49f, 3.48f, 3.47f, 3.46f, 3.45f, 3.44f, 3.43f, 3.42f, 3.41f, 3.40f,
    3.39f, 3.38f, 3.37f, 3.36f, 3.35f, 3.34f, 3.33f, 3.32f, 3.31f, 3.30f, 3.28f, 3.26f, 3.24f, 3.22f, 3.20f,
    3.18f, 3.16f, 3.14f, 3.12f, 3.10f, 3.05f, 3.00f, 2.95f, 2.90f, 2.80f, 2.70f, 2.60f, 2.50f};

static const float percent_table[] = {
    100.0f, 99.0f, 98.0f, 97.0f, 96.0f, 95.0f, 94.0f, 93.0f, 92.0f, 91.0f, 90.0f, 89.0f, 88.0f, 87.0f, 86.0f,
    85.0f,  84.0f, 83.0f, 82.0f, 81.0f, 80.0f, 79.0f, 78.0f, 77.0f, 76.0f, 75.0f, 74.0f, 73.0f, 72.0f, 71.0f,
    70.0f,  69.0f, 68.0f, 67.0f, 66.0f, 65.0f, 64.0f, 63.0f, 62.0f, 61.0f, 60.0f, 59.0f, 58.0f, 57.0f, 56.0f,
    55.0f,  54.0f, 53.0f, 52.0f, 51.0f, 50.0f, 49.0f, 48.0f, 47.0f, 46.0f, 45.0f, 44.0f, 43.0f, 42.0f, 41.0f,
    40.0f,  39.0f, 38.0f, 37.0f, 36.0f, 35.0f, 34.0f, 33.0f, 32.0f, 31.0f, 30.0f, 29.0f, 28.0f, 27.0f, 26.0f,
    25.0f,  24.0f, 23.0f, 22.0f, 21.0f, 20.0f, 19.0f, 18.0f, 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f,
    10.0f,  9.0f,  8.0f,  7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f,  1.0f,  0.5f,  0.2f,  0.0f};

#define TABLE_SIZE (sizeof(voltage_table) / sizeof(voltage_table[0]))

void drv_battery_init(void) {
    // Если bsp требует инициализации, она уже вызывается в main.
    // Здесь можно оставить пусто.
}

bool drv_battery_get_data(int *percent, float *voltage) {
    float analogVolts = 0.0f;

    // Вызываем без присвоения к esp_err_t, так как в bsp_battery.h она объявлена как void
    bsp_battery_get_voltage(&analogVolts, NULL);

    // ВАЖНО: В коде Waveshare есть баг. Если do_calibration1_chan0 == false,
    // то переменная analogVolts останется равна 0.0f. Проверяем это:
    if (analogVolts <= 0.0f) {
        ESP_LOGW(TAG, "Battery voltage is 0 (calibration failed?)");
        return false;
    }

    // Ваша логика интерполяции (оставил без изменений)
    float v = analogVolts; // В функции BSP уже есть умножение на 3.0, берем готовое
    int p = 0;

    if (v >= voltage_table[0]) {
        p = 100;
    } else if (v <= voltage_table[TABLE_SIZE - 1]) {
        p = 0;
    } else {
        for (int i = 0; i < TABLE_SIZE - 1; i++) {
            if (v <= voltage_table[i] && v >= voltage_table[i + 1]) {
                p = (int)(percent_table[i + 1] +
                          (v - voltage_table[i + 1]) * (percent_table[i] - percent_table[i + 1]) /
                              (voltage_table[i] - voltage_table[i + 1]) +
                          0.5f);
                break;
            }
        }
    }

    if (percent)
        *percent = p;
    if (voltage)
        *voltage = v;

    return true;
}