#include "svc_motion.h"
#include "app_events.h"
#include "drv_gyro.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static char *TAG = "SVC_MOTION";
static TaskHandle_t motion_task_handle = NULL;
static i2c_master_bus_handle_t global_i2c_handle = NULL;

// ===== НАСТРОЙКИ =====
#define HP_FILTER_ALPHA 0.80f
#define ADAPTIVE_THRESHOLD_MIN 0.10f
#define ADAPTIVE_THRESHOLD_MAX 0.16f
#define ADAPTIVE_THRESHOLD_MULT 0.65f
#define STEP_PERIOD_MIN_MS 250
#define PEAK_HISTORY_SIZE 4

#define WRIST_GYRO_THRESHOLD 150.0f
#define WRIST_ACCEL_Z_UP 0.5f
#define WRIST_GESTURE_COOLDOWN 2000

// Порог отсева кручения запястья (ось Y) - оставляем, оно не мешает ходьбе
#define GYRO_STEP_REJECT_THRESHOLD 120.0f

// НОВОЕ! Максимально допустимый пик шага.
// Обычная ходьба дает пик 0.15 - 0.40. Тряска руки дает 1.0 - 2.0.
// Если пик > 0.80, мы считаем, что это тряска/удар, и сбрасываем шаг.
#define PEAK_MAX_REJECT 0.80f

typedef enum { STEP_IDLE, STEP_PEAKING, STEP_COOLDOWN } step_state_t;

static float hp_filtered = 0.0f;
static float prev_mag = 0.0f;
static float peak_history[PEAK_HISTORY_SIZE] = {0};
static int peak_idx = 0;
static float adaptive_threshold = ADAPTIVE_THRESHOLD_MIN;
static step_state_t step_state = STEP_IDLE;
static float current_peak = 0.0f;
static int64_t peaking_start_ms = 0;
static int64_t last_step_time_ms = 0;
static int64_t last_wrist_time_ms = 0;
static uint32_t total_steps = 0;

static int64_t last_log_time = 0;
static float debug_max_hp = 0.0f;
static float debug_max_gyro_y = 0.0f;

static void motion_task(void *pvParameters) {
    ESP_LOGI(TAG, "Motion monitor task started");

    drv_mpu6050_init(global_i2c_handle);
    vTaskDelay(pdMS_TO_TICKS(500));

    drv_mpu6050_data_t init_data;
    uint16_t init_cnt = 0;
    if (drv_mpu6050_read_batch(1, &init_data, &init_cnt) && init_cnt > 0) {
        hp_filtered = prev_mag = sqrtf(init_data.accel_x * init_data.accel_x + init_data.accel_y * init_data.accel_y +
                                       init_data.accel_z * init_data.accel_z);
    }

    for (int j = 0; j < PEAK_HISTORY_SIZE; j++)
        peak_history[j] = 0.2f;

#define MAX_BATCH_SIZE 32
    drv_mpu6050_data_t data_buf[MAX_BATCH_SIZE];

    while (1) {
        uint16_t packets_read = 0;

        if (drv_mpu6050_read_batch(MAX_BATCH_SIZE, data_buf, &packets_read) && packets_read > 0) {

            for (int i = 0; i < packets_read; i++) {
                int64_t now_ms = esp_timer_get_time() / 1000;
                drv_mpu6050_data_t *d = &data_buf[i];

                float mag = sqrtf(d->accel_x * d->accel_x + d->accel_y * d->accel_y + d->accel_z * d->accel_z);

                float prev_hp_filtered = hp_filtered;
                hp_filtered = HP_FILTER_ALPHA * (hp_filtered + mag - prev_mag);
                prev_mag = mag;

                if (hp_filtered > debug_max_hp)
                    debug_max_hp = hp_filtered;
                if (fabsf(d->gyro_y) > debug_max_gyro_y)
                    debug_max_gyro_y = fabsf(d->gyro_y);

                // 3. Детекция шагов
                switch (step_state) {
                case STEP_IDLE:
                    // Убрали проверку Gyro Total, оставили только кручение запястья (ось Y)
                    if (hp_filtered > adaptive_threshold && fabsf(d->gyro_y) < GYRO_STEP_REJECT_THRESHOLD) {
                        step_state = STEP_PEAKING;
                        current_peak = hp_filtered;
                        peaking_start_ms = now_ms;
                    }
                    break;

                case STEP_PEAKING:
                    // Отмена при кручении запястья
                    if (fabsf(d->gyro_y) > GYRO_STEP_REJECT_THRESHOLD) {
                        step_state = STEP_IDLE;
                        break;
                    }

                    if (hp_filtered > current_peak) {
                        current_peak = hp_filtered;

                        // НОВОЕ: Отмена при аномальном пике (тряска руки)
                        if (current_peak > PEAK_MAX_REJECT) {
                            step_state = STEP_IDLE;
                            break;
                        }
                    } else if (hp_filtered < current_peak * 0.4f) {
                        total_steps++;
                        last_step_time_ms = now_ms;

                        if (current_peak > 0.15f) {
                            peak_history[peak_idx] = fmin(current_peak, 0.6f);
                            peak_idx = (peak_idx + 1) % PEAK_HISTORY_SIZE;

                            float avg_peak = 0;
                            for (int j = 0; j < PEAK_HISTORY_SIZE; j++)
                                avg_peak += peak_history[j];
                            avg_peak /= PEAK_HISTORY_SIZE;

                            adaptive_threshold = fmax(ADAPTIVE_THRESHOLD_MIN, avg_peak * ADAPTIVE_THRESHOLD_MULT);
                            adaptive_threshold = fmin(adaptive_threshold, ADAPTIVE_THRESHOLD_MAX);
                        }

                        ESP_LOGI(TAG, "🦶 STEP DETECTED! Total: %lu, Peak: %.3f, Thr: %.3f", total_steps, current_peak,
                                 adaptive_threshold);

                        motion_step_data_t step_data = {.steps = total_steps, .intensity = current_peak};
                        esp_event_post(APP_EVENTS, EVT_MOTION_STEP_UPDATED, &step_data, sizeof(step_data),
                                       pdMS_TO_TICKS(50));

                        step_state = STEP_COOLDOWN;
                    } else if (now_ms - peaking_start_ms > 500) {
                        step_state = STEP_IDLE;
                    }
                    break;

                case STEP_COOLDOWN:
                    if (now_ms - last_step_time_ms >= STEP_PERIOD_MIN_MS) {
                        step_state = STEP_IDLE;
                    }
                    break;
                }

                // 4. Детекция жеста
                bool gyro_triggered = fabsf(d->gyro_y) > WRIST_GYRO_THRESHOLD;
                bool accel_orientation_ok = (d->accel_z > WRIST_ACCEL_Z_UP) || (d->accel_x > WRIST_ACCEL_Z_UP) ||
                                            (d->accel_y > WRIST_ACCEL_Z_UP);

                if (gyro_triggered && accel_orientation_ok) {
                    if ((now_ms - last_wrist_time_ms) > WRIST_GESTURE_COOLDOWN) {
                        last_wrist_time_ms = now_ms;
                        ESP_LOGI(TAG, "👉 WRIST RAISE DETECTED! (GyroY: %.1f)", d->gyro_y);
                        esp_event_post(APP_EVENTS, EVT_MOTION_WRIST_RAISE, NULL, 0, pdMS_TO_TICKS(50));
                    }
                }
            }

            if (esp_timer_get_time() / 1000 - last_log_time > 2000) {
                ESP_LOGI(TAG, "DATA -> HP: %.3f (max: %.3f) | Thr: %.3f | GyroY: %.1f (max: %.1f)", hp_filtered,
                         debug_max_hp, adaptive_threshold, data_buf[packets_read - 1].gyro_y, debug_max_gyro_y);

                last_log_time = esp_timer_get_time() / 1000;
                debug_max_hp = 0.0f;
                debug_max_gyro_y = 0.0f;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void svc_motion_monitor_start(i2c_master_bus_handle_t i2c_bus_handle) {
    if (motion_task_handle != NULL)
        return;
    global_i2c_handle = i2c_bus_handle;
    xTaskCreatePinnedToCore(motion_task, "motion_mon", 4096, NULL, 5, &motion_task_handle, 1);
}