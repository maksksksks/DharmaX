#include "svc_ds18b20.h"
#include "app_events.h"
#include "drv_ds18b20.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MEASUREMENT_INTERVAL_MS 5000  // Как часто мы хотим получать данные (3 секунды)
#define CONVERSION_TIME_MS      200   // Время конвертации для 10 бит (~187мс + запас)


static const char *TAG = "SVC_TEMP";

static bool flag = false;

static TaskHandle_t temp_task_handle = NULL;


static void temp_task(void *pvParameters) {
    ESP_LOGI(TAG, "DS18B20 monitor task started");

    float last_value = 0;
    flag = true;

    while (1) {
        // 1. Даем команду датчику начать измерение
        drv_ds18b20_start_conversion();

        // 2. СПИМ пока датчик физически измеряет температуру
        // FreeRTOS переведет задачу в Blocked state, MCU будет спать или выполнять другие задачи
        vTaskDelay(pdMS_TO_TICKS(CONVERSION_TIME_MS));

        // 3. Проснулись и мгновенно читаем готовые данные
        float v = 0;
        if (drv_ds18b20_read_temp(&v)) {
            last_value = v;
        } else {
            ESP_LOGW(TAG, "Using cached temp data");
        }

        // 4. Кричим в эфир
        temperature_data_t data = {
            .temp = last_value,
        };
        esp_event_post(APP_EVENTS, EVT_TEMP_UPDATED, &data, sizeof(data), 100);

        // 5. СПИМ остаток времени до следующего цикла
        // Если интервал 3000мс, а на конвертацию ушло 200мс, то поспим еще 2800мс
        int remaining_sleep = MEASUREMENT_INTERVAL_MS - CONVERSION_TIME_MS;
        if (remaining_sleep > 0) {
            vTaskDelay(pdMS_TO_TICKS(remaining_sleep));
        }
    }
}

void svc_ds18b20_start() { xTaskCreatePinnedToCore(temp_task, "temp_mon", 4096, NULL, 2, &temp_task_handle, 0); }

void svc_ds18b20_stop() {
    if (temp_task_handle != NULL) {
        vTaskDelete(temp_task_handle);
        temp_task_handle = NULL;
        ESP_LOGI(TAG, "DS18B20 monitor stopped");
        flag = false;
    }
}

bool svc_ds18b20_is_running() { return flag; }