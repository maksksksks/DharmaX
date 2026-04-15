#include <stdio.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "nvs_flash.h"

#include "esp_lvgl_port.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_phtrz.h"
#include "bsp_sdcard.h"
#include "bsp_touch.h"
#include "bsp_wifi.h"

#include "button_gpio.h"
#include "iot_button.h"

#include "lvgl_ui.h"

#include "app_events.h"

#include "drv_battery.h"
#include "svc_battery_monitor.h"

#include "svc_ble_sync.h"

#include "drv_phtrz.h"
#include "svc_lighting_monitor.h"

#include "svc_auto_brightness.h"

//#include "drv_vibro.h"
#include "svc_vibro.h"

#include "svc_motion.h"

#include "svc_screen_sleep.h"
#include "home_tile.h"

#define EXAMPLE_DISPLAY_ROTATION 0

#define EXAMPLE_LCD_H_RES (172)
#define EXAMPLE_LCD_V_RES (320)

static char *TAG = "WristWatch";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

void lv_fs_fatfs_init(void);
static esp_err_t app_lvgl_init(void);
static void button_init(void);

void app_main(void) {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,     // Начинаем с максимальной
        .light_sleep_enable = true,  // ★ Разрешаем light sleep когда CPU idle!
    };
    esp_pm_configure(&pm_config);
    ESP_LOGI("PWR", "PM initialized: light_sleep=ON");

    i2c_master_bus_handle_t i2c_bus_handle;

    // ==========================================
    // 1. Initialize NVS
    // ==========================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ==========================================
    // 1.5 ★★★ СОЗДАЕМ EVENT LOOP ★★★
    //     (раньше это делал Wi-Fi, но он закомментирован!)
    // ==========================================
    esp_err_t event_ret = esp_event_loop_create_default();
    if (event_ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Event loop already exists (OK)");
    } else {
        ESP_ERROR_CHECK(event_ret); // Если другая ошибка - крашимся с логом
    }
    ESP_LOGI(TAG, "✅ Event Loop ready");

    // ==========================================
    // 2. Базовая периферия (I2C, Датчики)
    // ==========================================
    i2c_bus_handle = bsp_i2c_init();
    bsp_battery_init();
    bsp_phtrz_init();
    

    // ==========================================
    // 3. Экран и Тач
    // ==========================================
    bsp_display_init(&io_handle, &panel_handle, EXAMPLE_LCD_H_RES * 100);
    bsp_touch_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);

    // ==========================================
    // 4. LVGL
    // ==========================================
    ESP_ERROR_CHECK(app_lvgl_init());
    bsp_display_brightness_init();
    bsp_display_set_brightness(80);

    // ==========================================
    // 5. SD Карта
    // ==========================================
    bsp_sdcard_init();
    lv_fs_fatfs_init();

    // ==========================================
    // 6. BLE
    // ==========================================
    svc_ble_sync_start();

    // ==========================================
    // 7. Инициализация драйверов (без запуска задач!)
    // ==========================================
    drv_battery_init();
    //drv_vibro_init();

    // ==========================================
    // 8. ★★ ЗАПУСК UI (здесь регистрируются обработчики событий!) ★★
    // ==========================================
    if (lvgl_port_lock(0)) {
        lvgl_ui_init();
        lvgl_port_unlock();
    }

    svc_screen_sleep_register_wake_callback(on_screen_wake_update_time);

    ESP_LOGI(TAG, "✅ UI created, event handlers registered");

    // ==========================================
    // 9. ★★ ЗАПУСК ФОНОВЫХ ЗАДАЧ (только после UI!) ★★
    // ==========================================
    svc_battery_monitor_start();
    svc_lighting_monitor_start();
    svc_auto_brightness_start();

    svc_vibro_start();
    vTaskDelay(pdMS_TO_TICKS(100));
    svc_vibro_send_event(VIBRATION_EVENT_OFF, LEVEL_ZERO, 0);

    svc_motion_monitor_start(i2c_bus_handle);

    ESP_LOGI(TAG, "✅ Background tasks started (now event handlers are ready)");

    // ★ 10. ЗАПУСК АВТОСНА (после всего!) ★
    svc_screen_sleep_start();           // ← Запуск сервиса
    svc_screen_sleep_set_timeout(20);   // ← 20 секунд таймаут

    // ==========================================
    // 11. Кнопки
    // ==========================================
    button_init();

    ESP_LOGI(TAG, "🚀 System fully initialized!");
}

static esp_err_t app_lvgl_init(void) {
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {.task_priority = 4,
                                      .task_stack = 1024 * 10,
                                      .task_affinity = -1,
                                      .task_max_sleep_ms = 500,
                                      .timer_period_ms = 5};
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * 100, // Безопасный размер
        .double_buffer = true,                  // Двойная буферизация
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = false,
                .mirror_y = false,
            },
        .flags = {
            .buff_spiram = true, // ВКЛЮЧАЕМ PSRAM (Wi-Fi оставил нам мало внутренней памяти)
            .buff_dma = false,   // ВЫКЛЮЧАЕМ DMA (DMA не работает с PSRAM напрямую)
        }};

    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x36 /* MADCTL */, (uint8_t[]){0x08}, 1));

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

static void button_event_cb(void *arg, void *data) {
    button_event_t event = iot_button_get_event((button_handle_t)arg);
    ESP_LOGI(TAG, "%s", iot_button_get_event_str(event));
}

static void button_init(void) {
    button_config_t btn_cfg = {};
    button_gpio_config_t btn_gpio_cfg = {};
    btn_gpio_cfg.gpio_num = GPIO_NUM_0;
    btn_gpio_cfg.active_level = 0;
    static button_handle_t btn = NULL;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn));
    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, NULL);
}