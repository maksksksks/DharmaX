#include "svc_battery_monitor.h"
#include "app_events.h"
#include "drv_battery.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SVC_BAT";
static TaskHandle_t battery_task_handle = NULL;

static void battery_task(void *pvParameters) {
    ESP_LOGI(TAG, "Battery monitor task started");

    // Запоминаем последние валидные данные (на случай если чтение сорвется)
    int last_percent = 0;
    float last_voltage = 0.0f;

    while (1) {
        int p = 0;
        float v = 0.0f;

        // Обращаемся к драйверу
        if (drv_battery_get_data(&p, &v)) {
            last_percent = p;
            last_voltage = v;
        } else {
            ESP_LOGW(TAG, "Using cached battery data");
        }

        // КРИЧИМ В ЭФИР (Отправляем событие)
        battery_data_t data = {.percent = last_percent, .voltage = last_voltage};

        // Отправляем данные. Таймаут 100мс (если очередь событий переполнена, просто пропускаем)
        esp_event_post(APP_EVENTS, EVT_BATTERY_UPDATED, &data, sizeof(data), pdMS_TO_TICKS(100));

        // Спим 5 секунд (В FreeRTOS НИКОГДА не используйте delay() или vTaskDelay без pdMS_TO_TICKS)
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void svc_battery_monitor_start(void) {
    // Создаем задачу на Ядре 0 (так как это фоновая логика, не UI)
    xTaskCreatePinnedToCore(battery_task, "bat_mon", 2048, NULL, 2, &battery_task_handle, 0);
}

void svc_battery_monitor_stop(void) {
    if (battery_task_handle != NULL) {
        vTaskDelete(battery_task_handle);
        battery_task_handle = NULL;
        ESP_LOGI("SVC_BAT", "Battery monitor stopped");
    }
}
