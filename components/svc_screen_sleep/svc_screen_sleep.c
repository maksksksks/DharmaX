#include "svc_screen_sleep.h"
#include "bsp_display.h"
#include "driver/gpio.h" // ★ Для отключения микрофона
#include "esp_log.h"
#include "esp_pm.h" // ★ CPU частота
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lvgl_ui.h"
#include "svc_auto_brightness.h"

static const char *TAG = "SCR_SLEEP";

// ==========================================
// КОНФИГУРАЦИЯ
// ==========================================
#define DEFAULT_TIMEOUT_SEC 20
#define TASK_CHECK_INTERVAL_MS 500
#define BRIGHTNESS_AWAKE 80
#define BRIGHTNESS_SLEEP 0

#define CPU_FREQ_AWAKE_MHZ 240
#define CPU_FREQ_SLEEP_MHZ 80

// ★ Пины INMP441 микрофона (УЗНАЙТЕ ВАШИ!)
// Обычно на ESP32-S3 платах Waveshare:
#define MIC_I2S_SCK_GPIO 12 // ★ ПОМЕНЯЙТЕ НА ВАШИ!
#define MIC_I2S_WS_GPIO 13  // ★ ПОМЕНЯЙТЕ НА ВАШИ!
#define MIC_I2S_SD_GPIO 9   // ★ ПОМЕНЯЙТЕ НА ВАШИ!

// ==========================================
// СОСТОЯНИЕ
// ==========================================
static TaskHandle_t sleep_task_handle = NULL;
static SemaphoreHandle_t state_mutex = NULL;
static uint32_t timeout_seconds = DEFAULT_TIMEOUT_SEC;

static volatile bool is_sleeping = false;
static bool service_running = false;
static bool auto_enabled = true;

static screen_wake_callback_t wake_callback = NULL;
static int saved_brightness = BRIGHTNESS_AWAKE;
static uint32_t idle_counter_ms = 0;

// ★ Сохранённый указатель на таймер LVGL из home_tile ★
extern lv_timer_t *home_tile_timer; // Из home_tile.c/h

// ==========================================
// ★★★ ФУНКЦИИ ЭКОНОМИИ ЭНЕРГИИ ★★★
// ==========================================

/**
 * @brief Снизить частоту CPU
 */
static void pm_reduce_cpu(void) {
    esp_pm_config_t cfg = {
        .max_freq_mhz = CPU_FREQ_SLEEP_MHZ,
        .min_freq_mhz = CPU_FREQ_SLEEP_MHZ,
    };
    esp_pm_configure(&cfg);
    ESP_LOGI(TAG, "CPU → %d MHz", CPU_FREQ_SLEEP_MHZ);
}

/**
 * @brief Восстановить частоту CPU
 */
static void pm_restore_cpu(void) {
    esp_pm_config_t cfg = {
        .max_freq_mhz = CPU_FREQ_AWAKE_MHZ,
        .min_freq_mhz = CPU_FREQ_AWAKE_MHZ,
    };
    esp_pm_configure(&cfg);
    ESP_LOGI(TAG, "CPU → %d MHz", CPU_FREQ_AWAKE_MHZ);
}

/**
 * @brief ★ Приостановить LVGL отрисовку! ★
 *
 * Это ОГРОМНАЯ экономия! LVGL таск постоянно:
 * - Проверяет dirty флаги
 * - Делает flush буфера (SPI транзакции к дисплею)
 * - Активирует PSRAM
 */
static void lvgl_pause(void) {
    if (home_tile_timer != NULL) {
        lv_timer_pause(home_tile_timer); // ★ Стоп таймер обновления времени!
        ESP_LOGI(TAG, "LVGL timer PAUSED");
    }

    // Также можно приостановить весь LVGL если нужно:
    // lv_display_set_activity(NULL);  // Зависит от версии LVGL
}

/**
 * @brief ★ Возобновить LVGL отрисовку ★
 */
static void lvgl_resume(void) {
    if (home_tile_timer != NULL) {
        lv_timer_resume(home_tile_timer); // ★ Возобновляем таймер!
        ESP_LOGI(TAG, "LVGL timer RESUMED");
    }

    // Принудительно обновляем сразу (чтобы не ждать следующий тик)
    if (wake_callback != NULL) {
        wake_callback(); // Обновит время мгновенно
    }
}

/**
 * @brief ★ Максимальное выключение дисплея ★
 */
static void display_full_off(void) {
    // 1. Яркость в 0%
    bsp_display_set_brightness(0);

    // 2. Если есть возможность - отправляем команду SLEEP IN на дисплей
    // (зависит от вашего BSP, может не работать)
    // bsp_display_sleep(true);

    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "Display FULL OFF");
}

/**
 * @brief ★ Включение дисплея ★
 */
static void display_full_on(int brightness) {
    // 1. Включаем дисплей
    // bsp_display_sleep(false);

    // 2. Устанавливаем яркость
    bsp_display_set_brightness(brightness);

    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "Display ON (%d%%)", brightness);
}

// ==========================================
// ВКЛЮЧЕНИЕ ЭКРАНА (полная мощность)
// ==========================================

static void screen_turn_on(void) {
    if (!is_sleeping)
        return;

    ESP_LOGI(TAG, "🌅 Screen WAKE UP (full power)");

    // 1. ★ Сначала CPU! (нужно для быстрой отрисовки)
    pm_restore_cpu();

    // 2. ★ Включаем дисплей!
    display_full_on(saved_brightness);

    // 3. ★ Возобновляем LVGL!
    lvgl_resume();

    // 4. Возобновляем авто-яркость
    svc_auto_brightness_resume(true);

    is_sleeping = false;

    // 5. Callback обновления времени (уже вызван в lvgl_resume, но на всякий случай)
    if (wake_callback != NULL) {
        wake_callback();
    }
}

// ==========================================
// ВЫКЛЮЧЕНИЕ ЭКРАНА (максимальная экономия!)
// ==========================================

static void screen_turn_off(void) {
    if (is_sleeping)
        return;

    ESP_LOGI(TAG, "😴 Screen SLEEP (power save)");

    // 1. Приостанавливаем авто-яркость
    svc_auto_brightness_suspend();

    // 2. Сохраняем яркость
    saved_brightness = svc_auto_brightness_get_current();
    if (saved_brightness <= 0) {
        saved_brightness = BRIGHTNESS_AWAKE;
    }

    // 3. ★ ПРИОСТАНАВЛИВАЕМ LVGL! (до гашения дисплея!)
    lvgl_pause();

    // 4. ★ Гасим дисплей полностью!
    display_full_off();

    // 5. ★ Снижаем частоту CPU!
    pm_reduce_cpu();

    is_sleeping = true;
}

// ==========================================
// ВСЁ ОСТАЛЬНОЕ БЕЗ ИЗМЕНЕНИЙ
// ==========================================

static void reset_idle_timer(void) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        idle_counter_ms = 0;

        bool was_sleeping = is_sleeping;
        xSemaphoreGive(state_mutex);

        if (was_sleeping) {
            screen_turn_on();
        } else {
            ESP_LOGD(TAG, "👆 Touch detected, timer reset");
        }
    }
}

static void global_touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_CLICKED) {
        reset_idle_timer();
    }
}

static void add_touch_handler_to_all(lv_obj_t *obj) {
    if (obj == NULL)
        return;
    lv_obj_add_event_cb(obj, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, global_touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, global_touch_event_cb, LV_EVENT_CLICKED, NULL);

    uint32_t child_cnt = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        add_touch_handler_to_all(child);
    }
}

static void screen_sleep_task(void *pvParameters) {
    ESP_LOGI(TAG, "Screen sleep task started on core %d", xPortGetCoreID());

    while (1) {
        if (service_running && auto_enabled) {
            bool should_sleep = false;

            if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                idle_counter_ms += TASK_CHECK_INTERVAL_MS;

                uint32_t timeout_ms = timeout_seconds * 1000;

                if (idle_counter_ms >= timeout_ms && !is_sleeping) {
                    should_sleep = true;
                    idle_counter_ms = 0;
                }

                xSemaphoreGive(state_mutex);
            }

            if (should_sleep && !get_popup_status()) {
                screen_turn_off();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CHECK_INTERVAL_MS));
    }
}

// Public API
void svc_screen_sleep_start(void) {
    if (service_running) {
        ESP_LOGW(TAG, "Screen sleep service already running");
        return;
    }

    ESP_LOGI(TAG, "🚀 Starting screen sleep service (timeout=%d sec)", timeout_seconds);

    if (state_mutex == NULL) {
        state_mutex = xSemaphoreCreateMutex();
    }

    lv_obj_t *screen = lv_scr_act();
    if (screen != NULL) {
        add_touch_handler_to_all(screen);
        ESP_LOGI(TAG, "✅ Touch handlers installed");
    } else {
        ESP_LOGE(TAG, "❌ No active screen found!");
    }

    BaseType_t ret = xTaskCreatePinnedToCore(screen_sleep_task, "scr_sleep", 4096, NULL, 3, &sleep_task_handle, 0);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sleep task!");
        return;
    }

    service_running = true;
    auto_enabled = true;
    is_sleeping = false;

    ESP_LOGI(TAG, "✅ Screen sleep service started");
    ESP_LOGI(TAG, "   Timeout: %d seconds", timeout_seconds);
}

void svc_screen_sleep_stop(void) {
    if (!service_running)
        return;

    if (sleep_task_handle != NULL) {
        vTaskDelete(sleep_task_handle);
        sleep_task_handle = NULL;
    }

    if (is_sleeping) {
        pm_restore_cpu();                  // Восстанавливаем CPU
        display_full_on(saved_brightness); // Включаем дисплей
        lvgl_resume();                     // Возобновляем LVGL
        is_sleeping = false;
    }

    service_running = false;
    auto_enabled = false;

    ESP_LOGI(TAG, "Screen sleep service stopped");
}

void svc_screen_sleep_set_timeout(uint32_t timeout_sec) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        timeout_seconds = timeout_sec;
        idle_counter_ms = 0;
        xSemaphoreGive(state_mutex);
    }
    ESP_LOGI(TAG, "Sleep timeout set to %d seconds", timeout_sec);
}

uint32_t svc_screen_sleep_get_timeout(void) { return timeout_seconds; }

void svc_screen_sleep_wake_up(void) { reset_idle_timer(); }

void svc_screen_sleep_force_sleep(void) {
    if (service_running && auto_enabled && !is_sleeping) {
        screen_turn_off();
    }
}

bool svc_screen_sleep_is_sleeping(void) { return is_sleeping; }

void svc_screen_sleep_register_wake_callback(screen_wake_callback_t cb) {
    wake_callback = cb;
    ESP_LOGI(TAG, "Wake callback registered");
}

void svc_screen_sleep_set_enabled(bool enabled) {
    auto_enabled = enabled;
    ESP_LOGI(TAG, "Screen sleep auto %s", enabled ? "ENABLED" : "DISABLED");

    if (!enabled && is_sleeping) {
        ESP_LOGI(TAG, "Auto disabled but screen was sleeping -> waking up");
        screen_turn_on();
    }

    if (enabled) {
        if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            idle_counter_ms = 0;
            xSemaphoreGive(state_mutex);
        }
    }
}

bool svc_screen_sleep_is_enabled(void) { return auto_enabled; }

int count_objects_recursive(lv_obj_t *obj) {
    if (obj == NULL)
        return 0;
    int count = 1;
    uint32_t child_cnt = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        count += count_objects_recursive(child);
    }
    return count;
}