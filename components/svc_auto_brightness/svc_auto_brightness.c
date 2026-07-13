#include "svc_auto_brightness.h"
#include "app_events.h"
#include "bsp_display.h"
#include "drv_phtrz.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "AUTO_BRI";

// ==========================================
// КОНФИГУРАЦИЯ КАЛИБРОВКИ
// ==========================================
#define BRIGHTNESS_LOW_VALUE 300
#define BRIGHTNESS_LOW_PERCENT 2
#define BRIGHTNESS_HIGH_VALUE 1700
#define BRIGHTNESS_HIGH_PERCENT 50
#define BRIGHTNESS_MAX_LIMIT 95
#define BRIGHTNESS_MIN_LIMIT 1
#define BRIGHTNESS_UPDATE_MS 2000

// ==========================================
// ГЛОБАЛЬНОЕ СОСТОЯНИЕ
// ==========================================
static TaskHandle_t auto_brightness_task_handle = NULL;
static SemaphoreHandle_t state_mutex = NULL;
static int current_brightness = 20;
static bool auto_enabled = true;

// ★★★ НОВАЯ ПЕРЕМЕННАЯ: ФЛАГ ПРИОСТАНОВКИ ★★★
static volatile bool is_suspended = false; // true = экран спит, не трогать яркость!

// ==========================================
// МАТЕМАТИКА
// ==========================================

static int calculate_brightness(int light_value) {
    int brightness;

    if (light_value <= BRIGHTNESS_LOW_VALUE) {
        brightness = BRIGHTNESS_MIN_LIMIT;
    } else if (light_value >= BRIGHTNESS_HIGH_VALUE) {
        brightness = BRIGHTNESS_HIGH_PERCENT;

        if (light_value > BRIGHTNESS_HIGH_VALUE) {
            int extra = light_value - BRIGHTNESS_HIGH_VALUE;
            int extra_brightness = extra / 100;
            brightness = BRIGHTNESS_HIGH_PERCENT + extra_brightness;

            if (brightness > BRIGHTNESS_MAX_LIMIT) {
                brightness = BRIGHTNESS_MAX_LIMIT;
            }
        }
    } else {
        float range_x = BRIGHTNESS_HIGH_VALUE - BRIGHTNESS_LOW_VALUE;
        float range_y = BRIGHTNESS_HIGH_PERCENT - BRIGHTNESS_LOW_PERCENT;
        float position = (float)(light_value - BRIGHTNESS_LOW_VALUE) / range_x;

        brightness = BRIGHTNESS_LOW_PERCENT + (int)(position * range_y);
    }

    return brightness;
}

// ==========================================
// ОБРАБОТЧИК СОБЫТИЙ ОСВЕЩЕННОСТИ
// ==========================================

static void auto_brightness_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    if (id != EVT_LIGHT_UPDATED)
        return;
    if (!auto_enabled)
        return;

    lighting_data_t *light = (lighting_data_t *)event_data;
    if (light == NULL)
        return;

    // ★★★ ЗАЩИТА: Если экран спит - НЕ МЕНЯТЬ ЯРКОСТЬ! ★★★
    if (is_suspended) {
        ESP_LOGD(TAG, "⏸️ Suspended: ignoring light event (screen is sleeping)");
        return; // ← ПРОСТО ВЫХОДИМ, НИЧЕГО НЕ ДЕЛАЕМ!
    }

    int new_brightness = calculate_brightness(light->value);

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_brightness = new_brightness;
        xSemaphoreGive(state_mutex);
    }

    //ESP_LOGI(TAG, "🔆 Setting brightness: %d%% (light=%d)", new_brightness, light->value);
    bsp_display_set_brightness(new_brightness);
}

// ==========================================
// ФОНОВАЯ ЗАДАЧА
// ==========================================

static void auto_brightness_task(void *pvParameters) {
    ESP_LOGI(TAG, "Auto-brightness task started");

#define AVG_BUFFER_SIZE 5
    int avg_buffer[AVG_BUFFER_SIZE] = {0};
    int avg_index = 0;
    int avg_sum = 0;
    bool buffer_full = false;

    while (1) {
        if (!auto_enabled) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // ★★★ ЗАЩИТА В ЗАДАЧЕ ТОЖЕ ★★★
        if (is_suspended) {
            vTaskDelay(pdMS_TO_TICKS(BRIGHTNESS_UPDATE_MS));
            continue; // Спим, пока экран не проснется
        }

        int raw_value = 0;
        if (drv_phtrz_get_data(&raw_value)) {
            avg_sum -= avg_buffer[avg_index];
            avg_buffer[avg_index] = raw_value;
            avg_sum += raw_value;
            avg_index = (avg_index + 1) % AVG_BUFFER_SIZE;

            if (avg_index == 0)
                buffer_full = true;

            int avg_value = buffer_full ? (avg_sum / AVG_BUFFER_SIZE) : raw_value;
            int new_brightness = calculate_brightness(avg_value);

            int old_brightness = 0;
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                old_brightness = current_brightness;

                if (abs(new_brightness - old_brightness) >= 2) {
                    current_brightness = new_brightness;
                    xSemaphoreGive(state_mutex);

                    ESP_LOGI(TAG, "🔆 [Task] Brightness: %d%% (avg_light=%d)", new_brightness, avg_value);
                    bsp_display_set_brightness(new_brightness);
                } else {
                    xSemaphoreGive(state_mutex);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to read sensor in task");
        }

        vTaskDelay(pdMS_TO_TICKS(BRIGHTNESS_UPDATE_MS));
    }
}

// ==========================================
// ПУБЛИЧНЫЙ API
// ==========================================

void svc_auto_brightness_start(void) {
    if (auto_brightness_task_handle != NULL) {
        ESP_LOGW(TAG, "Auto-brightness already running");
        return;
    }

    if (state_mutex == NULL) {
        state_mutex = xSemaphoreCreateMutex();
    }

    esp_event_handler_instance_t handler;
    esp_err_t ret = esp_event_handler_instance_register(APP_EVENTS, EVT_LIGHT_UPDATED, auto_brightness_event_handler,
                                                        NULL, &handler);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ Event handler registered for EVT_LIGHT_UPDATED");
    }

    BaseType_t task_created =
        xTaskCreatePinnedToCore(auto_brightness_task, "auto_bri", 4096, NULL, 3, &auto_brightness_task_handle, 0);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create auto-brightness task!");
    } else {
        ESP_LOGI(TAG, "🚀 Auto-brightness service started");
        ESP_LOGI(TAG, "   Calibration: %d→%d%% , %d→%d%% , max=%d%%", BRIGHTNESS_LOW_VALUE, BRIGHTNESS_LOW_PERCENT,
                 BRIGHTNESS_HIGH_VALUE, BRIGHTNESS_HIGH_PERCENT, BRIGHTNESS_MAX_LIMIT);
    }
}

void svc_auto_brightness_stop(void) {
    auto_enabled = false;
    is_suspended = false; // Сбрасываем при остановке

    if (auto_brightness_task_handle != NULL) {
        vTaskDelete(auto_brightness_task_handle);
        auto_brightness_task_handle = NULL;
        ESP_LOGI(TAG, "Auto-brightness task stopped");
    }

    esp_event_handler_instance_unregister(APP_EVENTS, EVT_LIGHT_UPDATED, NULL);

    ESP_LOGI(TAG, "Auto-brightness disabled");
}

int svc_auto_brightness_get_current(void) {
    int val = 20;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        val = current_brightness;
        xSemaphoreGive(state_mutex);
    }
    return val;
}

void svc_auto_brightness_set_enabled(bool enabled) {
    auto_enabled = enabled;
    ESP_LOGI(TAG, "Auto-brightness %s", enabled ? "ENABLED" : "DISABLED");
}

bool svc_auto_brightness_is_enabled(void) { return auto_enabled; }

// ==========================================
// ★★ НОВЫЕ ФУНКЦИИ: SUSPEND / RESUME ★★
// ==========================================

void svc_auto_brightness_suspend(void) {
    is_suspended = true;
    ESP_LOGI(TAG, "⏸️ Auto-brightness SUSPENDED (screen sleeping)");
}

void svc_auto_brightness_resume(bool force_update) {
    is_suspended = false;
    ESP_LOGI(TAG, "▶️ Auto-brightness RESUMED (screen awake)");

    if (force_update && auto_enabled) {
        // Сразу применяем текущую рассчитанную яркость
        int brightness = 0;
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            brightness = current_brightness;
            xSemaphoreGive(state_mutex);
        }

        if (brightness > 0) {
            ESP_LOGI(TAG, "🔆 Force applying brightness: %d%%", brightness);
            bsp_display_set_brightness(brightness);
        }
    }
}

bool svc_auto_brightness_is_suspended(void) { return is_suspended; }