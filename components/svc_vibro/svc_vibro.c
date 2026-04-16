/**
 * @file svc_vibro.c
 * @brief Сервис вибромотора (исправленная версия v3)
 */

#include "svc_vibro.h"
#include "app_events.h"
#include "drv_vibro.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "SVC_VIB";

/* ============================================================
 *  CONFIG
 * ============================================================ */

#define VIBRO_STACK 4096
#define VIBRO_PRIORITY 2
#define VIBRO_CORE 0
#define VIBRO_QUEUE_SIZE 10
#define QUEUE_TIMEOUT_MS 100

/* ============================================================
 *  STATE
 * ============================================================ */

static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_event_queue = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static volatile bool s_enabled = true;
static uint8_t s_current_level = 0;

/* ============================================================
 *  INNER: Pattern execution
 * ============================================================ */

static void execute_pattern(vibro_event_t pattern, vibro_level_t level, vibro_duration_t duration) {
    uint32_t short_ms = (duration != 0) ? duration : DURATION_SHORT;
    uint32_t long_ms = (duration != 0) ? duration : DURATION_LONG;
    uint32_t gap_ms = 80;

    switch (pattern) {
    case VIBRATION_EVENT_SINGLE_SHORT:
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(short_ms));
        break;

    case VIBRATION_EVENT_DOUBLE_SHORT:
        for (int i = 0; i < 2; i++) {
            if (i > 0)
                vTaskDelay(pdMS_TO_TICKS(gap_ms));
            drv_vibro_set_level(level);
            vTaskDelay(pdMS_TO_TICKS(short_ms));
            drv_vibro_off();
            vTaskDelay(pdMS_TO_TICKS(gap_ms));
        }
        break;

    case VIBRATION_EVENT_SINGLE_LONG:
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(long_ms));
        break;

    case VIBRATION_EVENT_DOUBLE_LONG:
        for (int i = 0; i < 2; i++) {
            if (i > 0)
                vTaskDelay(pdMS_TO_TICKS(150));
            drv_vibro_set_level(level);
            vTaskDelay(pdMS_TO_TICKS(long_ms));
            drv_vibro_off();
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        break;

    case VIBRATION_EVENT_TRIPLE_SHORT:
        for (int i = 0; i < 3; i++) {
            if (i > 0)
                vTaskDelay(pdMS_TO_TICKS(gap_ms));
            drv_vibro_set_level(level);
            vTaskDelay(pdMS_TO_TICKS(short_ms));
            drv_vibro_off();
            vTaskDelay(pdMS_TO_TICKS(gap_ms));
        }
        break;

    case VIBRATION_EVENT_TRIPLE_LONG:
        for (int i = 0; i < 3; i++) {
            if (i > 0)
                vTaskDelay(pdMS_TO_TICKS(150));
            drv_vibro_set_level(level);
            vTaskDelay(pdMS_TO_TICKS(long_ms));
            drv_vibro_off();
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        break;

    case VIBRATION_EVENT_SHORT_AND_LONG:
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(short_ms));
        drv_vibro_off();
        vTaskDelay(pdMS_TO_TICKS(gap_ms));
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(long_ms));
        break;

    case VIBRATION_EVENT_LONG_AND_SHORT:
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(long_ms));
        drv_vibro_off();
        vTaskDelay(pdMS_TO_TICKS(gap_ms));
        drv_vibro_set_level(level);
        vTaskDelay(pdMS_TO_TICKS(short_ms));
        break;

    default:
        ESP_LOGW(TAG, "Pattern %d not implemented", pattern);
        return;
    }

    // ✅ ВСЕГДА гасим после паттерна
    drv_vibro_off();
    s_current_level = 0;
}

/* ============================================================
 *  TASK
 * ============================================================ */


static void vibro_task(void *pvParameters) {
    vibro_data_t msg;

    s_current_level = 0;

    ESP_LOGI(TAG, "Task started [core=%d]", xPortGetCoreID());

    while (1) {
        // Check enabled
        bool can_run = false;
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            can_run = s_enabled;
            xSemaphoreGive(s_mutex);
        }

        if (!can_run) {
            drv_vibro_off();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Wait for event
        if (xQueueReceive(s_event_queue, &msg, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) == pdPASS) {

            ESP_LOGI(TAG, "[RX] evt=%d lvl=%d dur=%d", msg.event, msg.level, msg.duration);

            switch (msg.event) {
            case VIBRATION_EVENT_ON:
                s_current_level = (msg.level > 0) ? msg.level : LEVEL_MEDIUM;
                drv_vibro_set_level(s_current_level);
                break;

            case VIBRATION_EVENT_OFF:
                s_current_level = 0;
                drv_vibro_off();
                break;

            case VIBRATION_EVENT_SINGLE_SHORT:
            case VIBRATION_EVENT_DOUBLE_SHORT:
            case VIBRATION_EVENT_TRIPLE_SHORT:
            case VIBRATION_EVENT_SINGLE_LONG:
            case VIBRATION_EVENT_DOUBLE_LONG:
            case VIBRATION_EVENT_TRIPLE_LONG:
            case VIBRATION_EVENT_SHORT_AND_LONG:
            case VIBRATION_EVENT_LONG_AND_SHORT:
                execute_pattern(msg.event, msg.level, msg.duration);
                break;

            case VIBRATION_EVENT_ENABLE:
                if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
                    s_enabled = true;
                    xSemaphoreGive(s_mutex);
                }
                ESP_LOGI(TAG, "Module ENABLED");
                break;

            case VIBRATION_EVENT_DISABLE:
                if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
                    s_enabled = false;
                    xSemaphoreGive(s_mutex);
                }
                drv_vibro_off();
                s_current_level = 0;
                ESP_LOGI(TAG, "Module DISABLED");
                break;

            default:
                ESP_LOGW(TAG, "Unknown: %d", msg.event);
                break;
            }
        }
    }
}

/* ============================================================
 *  PUBLIC API
 * ============================================================ */

void svc_vibro_start(void) {
    ESP_LOGI(TAG, "=== Starting Vibro Service ===");

    // ✅ 1. Init driver (с защитой от double-init!)
    drv_vibro_init(); // Теперь безопасно вызвать несколько раз

    // ✅ 2. Create queue
    if (s_event_queue == NULL) {
        s_event_queue = xQueueCreate(VIBRO_QUEUE_SIZE, sizeof(vibro_data_t));
        assert(s_event_queue != NULL);
        ESP_LOGI(TAG, "Queue created (%d items)", VIBRO_QUEUE_SIZE);
    }

    // ✅ 3. Create mutex
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        assert(s_mutex != NULL);
    }

    // ✅ 4. Reset state
    s_enabled = true;
    s_current_level = 0;

    // ✅ 5. Force motor off BEFORE task start
    drv_vibro_off();

    // ✅ 6. Create task
    BaseType_t ret =
        xTaskCreatePinnedToCore(vibro_task, "vibro", VIBRO_STACK, NULL, VIBRO_PRIORITY, &s_task_handle, VIBRO_CORE);
    assert(ret == pdPASS);

    // ✅ 7. Wait for task to initialize
    vTaskDelay(pdMS_TO_TICKS(100));

    // ✅ 8. Final force-off ( belts and suspenders!)
    drv_vibro_off();

    ESP_LOGI(TAG, "=== Vibro Service Started ✓ ===");
}

void svc_vibro_stop(void) {
    ESP_LOGI(TAG, "Stopping...");

    drv_vibro_off();

    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_event_queue != NULL) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_current_level = 0;
    ESP_LOGI(TAG, "Stopped ✓");
}

void svc_vibro_set_level(uint8_t level) {
    if (level > 100)
        level = 100;
    s_current_level = level;
    drv_vibro_set_level(level);
}

uint8_t svc_vibro_get_level(void) { return s_current_level; }

bool svc_vibro_send_event(vibro_event_t event, vibro_level_t level, vibro_duration_t duration) {
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Queue NULL!");
        return false;
    }

    vibro_data_t msg = {.event = event, .level = level, .duration = duration};

    // ✅ Логирование (оставляем для отладки)
    //ESP_LOGW(TAG, "[SEND] evt=%d lvl=%d dur=%d | caller: 0x%p", event, level, duration, __builtin_return_address(0));

    BaseType_t ret = xQueueSend(s_event_queue, &msg, pdMS_TO_TICKS(10));

    if (ret != pdPASS) {
        ESP_LOGW(TAG, "QUEUE FULL! Event %d dropped", event);
        return false;
    }

    return true;
}