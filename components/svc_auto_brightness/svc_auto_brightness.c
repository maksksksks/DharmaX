#include "svc_auto_brightness.h"
#include "drv_phtrz.h"
#include "app_events.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "AUTO_BRI";

// ==========================================
// КОНФИГУРАЦИЯ КАЛИБРОВКИ
// ==========================================
#define BRIGHTNESS_LOW_VALUE     300    // Низкое значение люкс
#define BRIGHTNESS_LOW_PERCENT   2      // % при низком значении
#define BRIGHTNESS_HIGH_VALUE    1700   // Высокое значение люкс
#define BRIGHTNESS_HIGH_PERCENT  50     // % при высоком значении
#define BRIGHTNESS_MAX_LIMIT     90     // Максимальная яркость (защита глаз)
#define BRIGHTNESS_MIN_LIMIT     1      // Минимальная яркость (чтобы не погас)
#define BRIGHTNESS_UPDATE_MS     3000   // Период обновления (мс)

// ==========================================
// ГЛОБАЛЬНОЕ СОСТОЯНИЕ
// ==========================================
static TaskHandle_t auto_brightness_task_handle = NULL;
static SemaphoreHandle_t state_mutex = NULL;
static int current_brightness = 20;       // Текущая яркость (начальное значение)
static bool auto_enabled = true;          // Включен ли авто-режим

// ==========================================
// МАТЕМАТИКА: Линейная интерполяция
// ==========================================

/**
 * @brief Конвертировать значение фоторезистора в процент яркости
 * 
 * Формула линейной интерполяции:
 *   y = y1 + (x - x1) × (y2 - y1) / (x2 - x1)
 */
static int calculate_brightness(int light_value)
{
    int brightness;
    
    if (light_value <= BRIGHTNESS_LOW_VALUE) {
        // Темно → минимальная яркость
        brightness = BRIGHTNESS_MIN_LIMIT;
        
    } else if (light_value >= BRIGHTNESS_HIGH_VALUE) {
        // Ярко → максимальная (с лимитом)
        brightness = BRIGHTNESS_HIGH_PERCENT;
        
        // Плавное нарастание выше 1700 (до 80% максимума)
        if (light_value > BRIGHTNESS_HIGH_VALUE) {
            int extra = light_value - BRIGHTNESS_HIGH_VALUE;
            int extra_brightness = extra / 100;  // +1% каждые 100 люкс
            brightness = BRIGHTNESS_HIGH_PERCENT + extra_brightness;
            
            if (brightness > BRIGHTNESS_MAX_LIMIT) {
                brightness = BRIGHTNESS_MAX_LIMIT;
            }
        }
        
    } else {
        // Линейная интерполяция между 300 и 1700
        float range_x = BRIGHTNESS_HIGH_VALUE - BRIGHTNESS_LOW_VALUE;  // 1400
        float range_y = BRIGHTNESS_HIGH_PERCENT - BRIGHTNESS_LOW_PERCENT; // 48
        float position = (float)(light_value - BRIGHTNESS_LOW_VALUE) / range_x;
        
        brightness = BRIGHTNESS_LOW_PERCENT + (int)(position * range_y);
    }
    
    ESP_LOGD(TAG, "Light=%d → Brightness=%d%%", light_value, brightness);
    return brightness;
}

// ==========================================
// ОБРАБОТЧИК СОБЫТИЙ ОСВЕЩЕННОСТИ
// ==========================================

/**
 * @brief Пришло новое значение освещенности → пересчитать яркость
 */
static void auto_brightness_event_handler(void* arg, esp_event_base_t base, 
                                         int32_t id, void* event_data)
{
    if (id != EVT_LIGHT_UPDATED) return;
    if (!auto_enabled) return;
    
    lighting_data_t *light = (lighting_data_t*) event_data;
    if (light == NULL) return;
    
    int new_brightness = calculate_brightness(light->value);
    
    // Обновляем глобальное состояние (с защитой мьютексом)
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_brightness = new_brightness;
        xSemaphoreGive(state_mutex);
    }
    
    // Применяем яркость к экрану!
    ESP_LOGI(TAG, "🔆 Setting brightness: %d%% (light=%d)", new_brightness, light->value);
    bsp_display_set_brightness(new_brightness);
}

// ==========================================
// ФОНОВАЯ ЗАДАЧА (резервный механизм)
// ==========================================

/**
 * @brief Задача, которая раз в N секунд опрашивает датчик напрямую
 * 
 * Используется как fallback, если события не работают.
 * Также может применять сглаживание (moving average).
 */
static void auto_brightness_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Auto-brightness task started");
    
    // Буфер для скользящего среднего (сглаживание резких скачков)
    #define AVG_BUFFER_SIZE 5
    int avg_buffer[AVG_BUFFER_SIZE] = {0};
    int avg_index = 0;
    int avg_sum = 0;
    bool buffer_full = false;
    
    while (1) {
        if (!auto_enabled) {
            // Авто-режим выключен → спим
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // Читаем датчик напрямую
        int raw_value = 0;
        if (drv_phtrz_get_data(&raw_value)) {
            // Добавляем в буфер скользящего среднего
            avg_sum -= avg_buffer[avg_index];
            avg_buffer[avg_index] = raw_value;
            avg_sum += raw_value;
            avg_index = (avg_index + 1) % AVG_BUFFER_SIZE;
            
            if (avg_index == 0) buffer_full = true;
            
            // Вычисляем среднее
            int avg_value = buffer_full ? (avg_sum / AVG_BUFFER_SIZE) : raw_value;
            
            // Конвертируем в яркость
            int new_brightness = calculate_brightness(avg_value);
            
            // Применяем (только если изменилось более чем на 2%, чтобы не мерцало)
            int old_brightness = 0;
            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                old_brightness = current_brightness;
                
                if (abs(new_brightness - old_brightness) >= 2) {
                    current_brightness = new_brightness;
                    xSemaphoreGive(state_mutex);
                    
                    ESP_LOGI(TAG, "🔆 [Task] Brightness: %d%% (avg_light=%d, raw=%d)", 
                             new_brightness, avg_value, raw_value);
                    bsp_display_set_brightness(new_brightness);
                } else {
                    xSemaphoreGive(state_mutex);
                    // Изменение слишком маленькое, пропускаем (экономим SPI команды)
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to read sensor in task");
        }
        
        // Проверяем каждые 2 секунды
        vTaskDelay(pdMS_TO_TICKS(BRIGHTNESS_UPDATE_MS));
    }
}

// ==========================================
// ПУБЛИЧНЫЙ API
// ==========================================

void svc_auto_brightness_start(void)
{
    if (auto_brightness_task_handle != NULL) {
        ESP_LOGW(TAG, "Auto-brightness already running");
        return;
    }
    
    // Создаем мьютекс для защиты состояния
    if (state_mutex == NULL) {
        state_mutex = xSemaphoreCreateMutex();
    }
    
    // Подписываемся на события освещенности (быстрая реакция!)
    esp_event_handler_instance_t handler;
    esp_err_t ret = esp_event_handler_instance_register(
        APP_EVENTS, 
        EVT_LIGHT_UPDATED,
        auto_brightness_event_handler,
        NULL,
        &handler
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ Event handler registered for EVT_LIGHT_UPDATED");
    }
    
    // Создаем фоновую задачу (для сглаживания и fallback'а)
    BaseType_t task_created = xTaskCreatePinnedToCore(
        auto_brightness_task,
        "auto_bri",
        4096,          // Стек (не мало как в прошлый раз!)
        NULL,
        3,             // Приоритет (выше чем light_mon, но ниже LVGL)
        &auto_brightness_task_handle,
        0              // Ядро 0 (фоновое)
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create auto-brightness task!");
    } else {
        ESP_LOGI(TAG, "🚀 Auto-brightness service started");
        ESP_LOGI(TAG, "   Calibration: %d→%d%% , %d→%d%% , max=%d%%", 
                 BRIGHTNESS_LOW_VALUE, BRIGHTNESS_LOW_PERCENT,
                 BRIGHTNESS_HIGH_VALUE, BRIGHTNESS_HIGH_PERCENT,
                 BRIGHTNESS_MAX_LIMIT);
    }
}

void svc_auto_brightness_stop(void)
{
    auto_enabled = false;
    
    if (auto_brightness_task_handle != NULL) {
        vTaskDelete(auto_brightness_task_handle);
        auto_brightness_task_handle = NULL;
        ESP_LOGI(TAG, "Auto-brightness task stopped");
    }
    
    // Отписываемся от событий
    esp_event_handler_instance_unregister(APP_EVENTS, EVT_LIGHT_UPDATED, NULL);
    
    ESP_LOGI(TAG, "Auto-brightness disabled (manual mode)");
}

int svc_auto_brightness_get_current(void)
{
    int val = 20; // default
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        val = current_brightness;
        xSemaphoreGive(state_mutex);
    }
    return val;
}

void svc_auto_brightness_set_enabled(bool enabled)
{
    auto_enabled = enabled;
    ESP_LOGI(TAG, "Auto-brightness %s", enabled ? "ENABLED" : "DISABLED");
}

bool svc_auto_brightness_is_enabled(void)
{
    return auto_enabled;
}