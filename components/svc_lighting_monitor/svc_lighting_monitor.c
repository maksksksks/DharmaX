#include "svc_lighting_monitor.h"
#include "drv_phtrz.h"
#include "app_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SVC_LIG";

static void lighting_task(void *pvParameters) {
    ESP_LOGI(TAG, "Lighting monitor task started");
    
    // Запоминаем последние валидные данные (на случай если чтение сорвется)
    int last_value = 0;

    while (1) {
        int v = 0;

        // Обращаемся к драйверу
        if (drv_phtrz_get_data(&v)) {
            last_value = v;
        } else {
            ESP_LOGW(TAG, "Using cached ligth data");
        }

        // КРИЧИМ В ЭФИР (Отправляем событие)
        lighting_data_t data = {
            .value = last_value,
        };
        
        esp_event_post(APP_EVENTS, EVT_LIGHT_UPDATED, &data, sizeof(data), 100);

        // Спим 5 секунд (В FreeRTOS НИКОГДА не используйте delay() или vTaskDelay без pdMS_TO_TICKS)
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void svc_lighting_monitor_start(void) {
    // Создаем задачу на Ядре 0 (так как это фоновая логика, не UI)
    xTaskCreatePinnedToCore(lighting_task, "light_mon", 2048, NULL, 2, NULL, 0);
}