#include "svc_motion.h"
#include "drv_gyro.h"
#include "app_events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static char *TAG = "SVC_MOTION";
static TaskHandle_t motion_task_handle = NULL;
static i2c_master_bus_handle_t global_i2c_handle = NULL;

// ===== НАСТРОЙКИ (ПОДПРАВЛЕННЫЕ) =====
#define HP_FILTER_ALPHA         0.80f  // Было 0.95. Снизили, чтобы пропускать шаги (1-2 Гц)
#define ADAPTIVE_THRESHOLD_MIN  0.06f  // Было 0.15. Снизили минимальный порог
#define ADAPTIVE_THRESHOLD_MULT 0.55f  
#define STEP_PERIOD_MIN_MS      250    
#define STEP_PERIOD_MAX_MS      2000   
#define PEAK_HISTORY_SIZE       4      

#define WRIST_GYRO_THRESHOLD    150.0f 
#define WRIST_ACCEL_Z_UP        0.5f   
#define WRIST_GESTURE_COOLDOWN  2000   

typedef enum {
    STEP_IDLE,     
    STEP_PEAKING,  
    STEP_FALLING   
} step_state_t;

static float hp_filtered = 0.0f;       
static float prev_hp_filtered = 0.0f;
static float prev_mag = 0.0f;
static float peak_history[PEAK_HISTORY_SIZE] = {0};
static int peak_idx = 0;
static float adaptive_threshold = ADAPTIVE_THRESHOLD_MIN;
static step_state_t step_state = STEP_IDLE;
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
        hp_filtered = prev_mag = sqrtf(init_data.accel_x*init_data.accel_x + 
                                       init_data.accel_y*init_data.accel_y + 
                                       init_data.accel_z*init_data.accel_z);
    }

    #define MAX_BATCH_SIZE 32
    drv_mpu6050_data_t data_buf[MAX_BATCH_SIZE];

    while (1) {
        uint16_t packets_read = 0;
        
        if (drv_mpu6050_read_batch(MAX_BATCH_SIZE, data_buf, &packets_read) && packets_read > 0) {
            
            for (int i = 0; i < packets_read; i++) {
                int64_t now_ms = esp_timer_get_time() / 1000;
                drv_mpu6050_data_t *d = &data_buf[i];

                float mag = sqrtf(d->accel_x*d->accel_x + d->accel_y*d->accel_y + d->accel_z*d->accel_z);

                prev_hp_filtered = hp_filtered;
                hp_filtered = HP_FILTER_ALPHA * (hp_filtered + mag - prev_mag);
                prev_mag = mag;

                if (hp_filtered > debug_max_hp) debug_max_hp = hp_filtered;
                if (fabsf(d->gyro_y) > debug_max_gyro_y) debug_max_gyro_y = fabsf(d->gyro_y);

                // 3. Детекция шагов
                switch (step_state) {
                    case STEP_IDLE:
                        if (hp_filtered > adaptive_threshold) {
                            step_state = STEP_PEAKING;
                        }
                        break;
                        
                    case STEP_PEAKING:
                        if (hp_filtered < prev_hp_filtered) { 
                            step_state = STEP_FALLING;
                        }
                        // Защита: если шаг идет дольше 1 сек - это не шаг, а поворот руки
                        if (now_ms - last_step_time_ms > 1000) {
                            step_state = STEP_IDLE;
                        }
                        break;
                        
                    case STEP_FALLING:
                        if (hp_filtered < 0.0f) { 
                            int64_t step_period = now_ms - last_step_time_ms;
                            
                            if (step_period > STEP_PERIOD_MIN_MS && step_period < STEP_PERIOD_MAX_MS) {
                                total_steps++;
                                last_step_time_ms = now_ms;
                                
                                peak_history[peak_idx] = prev_hp_filtered; 
                                peak_idx = (peak_idx + 1) % PEAK_HISTORY_SIZE;
                                
                                float avg_peak = 0;
                                for(int j=0; j<PEAK_HISTORY_SIZE; j++) avg_peak += peak_history[j];
                                avg_peak /= PEAK_HISTORY_SIZE;
                                
                                adaptive_threshold = fmax(ADAPTIVE_THRESHOLD_MIN, avg_peak * ADAPTIVE_THRESHOLD_MULT);

                                ESP_LOGI(TAG, "🦶 STEP DETECTED! Total: %lu, Peak: %.3f, Thr: %.3f", total_steps, prev_hp_filtered, adaptive_threshold);
                                
                                motion_step_data_t step_data = {
                                    .steps = total_steps,
                                    .intensity = prev_hp_filtered
                                };
                                esp_event_post(APP_EVENTS, EVT_MOTION_STEP_UPDATED, &step_data, sizeof(step_data), pdMS_TO_TICKS(50));
                            }
                            step_state = STEP_IDLE;
                        }
                        // Защита: если ждем нуля дольше 1 сек - сброс
                        else if (now_ms - last_step_time_ms > 1000) {
                            step_state = STEP_IDLE;
                        }
                        break;
                }

                // 4. Жест
                bool gyro_triggered = fabsf(d->gyro_y) > WRIST_GYRO_THRESHOLD;
                bool accel_orientation_ok = (d->accel_z > WRIST_ACCEL_Z_UP) || (d->accel_x > WRIST_ACCEL_Z_UP) || (d->accel_y > WRIST_ACCEL_Z_UP);
                
                if (gyro_triggered && accel_orientation_ok) {
                    if ((now_ms - last_wrist_time_ms) > WRIST_GESTURE_COOLDOWN && d->gyro_y > 0) {
                        last_wrist_time_ms = now_ms;
                        ESP_LOGI(TAG, "👉 WRIST RAISE DETECTED! (GyroY: %.1f)", d->gyro_y);
                        esp_event_post(APP_EVENTS, EVT_MOTION_WRIST_RAISE, NULL, 0, pdMS_TO_TICKS(50));
                    }
                }
            }

            if (esp_timer_get_time() / 1000 - last_log_time > 2000) {
                ESP_LOGI(TAG, "DATA -> HP: %.3f (max: %.3f) | Thr: %.3f | GyroY: %.1f (max: %.1f)", 
                         hp_filtered, debug_max_hp, adaptive_threshold, 
                         data_buf[packets_read-1].gyro_y, debug_max_gyro_y);
                
                last_log_time = esp_timer_get_time() / 1000;
                debug_max_hp = 0.0f;
                debug_max_gyro_y = 0.0f;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void svc_motion_monitor_start(i2c_master_bus_handle_t i2c_bus_handle) {
    if (motion_task_handle != NULL) return;
    global_i2c_handle = i2c_bus_handle;
    xTaskCreatePinnedToCore(motion_task, "motion_mon", 4096, NULL, 5, &motion_task_handle, 1);
}