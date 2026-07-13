#include "svc_vibro.h"
#include "drv_vibro.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "SVC_VIBRO";

typedef struct {
    int pulses;
    uint32_t on_ms;
    uint32_t off_ms;
    uint8_t level_percent;
} vibro_job_t;

static QueueHandle_t s_queue = NULL;

static void job_from_event(vibro_event_t event, vibro_level_t level, vibro_duration_t duration, vibro_job_t *out)
{
    out->level_percent = (uint8_t)level; // vibro_level_t уже хранит проценты (LEVEL_LOW=40 и т.д.)
    out->on_ms = (uint32_t)duration;     // DURATION_SHORT=400 / DURATION_LONG=600 (мс)
    out->off_ms = 150;

    switch (event) {
    case VIBRATION_EVENT_SINGLE_SHORT:
    case VIBRATION_EVENT_SINGLE_LONG:
        out->pulses = 1;
        break;
    case VIBRATION_EVENT_DOUBLE_SHORT:
    case VIBRATION_EVENT_DOUBLE_LONG:
        out->pulses = 2;
        break;
    case VIBRATION_EVENT_TRIPLE_SHORT:
    case VIBRATION_EVENT_TRIPLE_LONG:
        out->pulses = 3;
        break;
    case VIBRATION_EVENT_SHORT_AND_LONG:
        out->pulses = 2;
        out->on_ms = DURATION_SHORT; // первый импульс короткий, второй - см. ниже (упрощённо: оба = duration)
        break;
    case VIBRATION_EVENT_LONG_AND_SHORT:
        out->pulses = 2;
        break;
    default:
        out->pulses = 1;
        break;
    }
}

static void vibro_task(void *arg)
{
    (void)arg;
    vibro_job_t job;
    for (;;) {
        if (xQueueReceive(s_queue, &job, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < job.pulses; i++) {
                drv_vibro_set_level(job.level_percent);
                vTaskDelay(pdMS_TO_TICKS(job.on_ms));
                drv_vibro_off();
                if (i != job.pulses - 1) {
                    vTaskDelay(pdMS_TO_TICKS(job.off_ms));
                }
            }
        }
    }
}

void svc_vibro_init(void)
{
    drv_vibro_init();
    s_queue = xQueueCreate(6, sizeof(vibro_job_t));
    xTaskCreate(vibro_task, "vibro_player", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Vibro service initialized");
}

void svc_vibro_send_event(vibro_event_t event, vibro_level_t level, vibro_duration_t duration)
{
    if (!s_queue) return;
    if (event == VIBRATION_EVENT_NONE || event == VIBRATION_EVENT_OFF) return;

    vibro_job_t job;
    job_from_event(event, level, duration, &job);
    xQueueSend(s_queue, &job, 0); // не блокируем вызывающего - если очередь полна, просто пропускаем
}
