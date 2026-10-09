#include <stdio.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "esp_lcd_sh8601.h"
#include "i2c_bsp.h"
#include "touch_bsp.h"
#include "ui.h"
#include "nvs_flash.h"

#include "app_events.h"
#include "svc_ble_sync.h"

// ★ Новые подключения (см. docs/display_smearing_fix.md и общий разбор сервисов)
#include "app_ui.h"             // единая точка входа вместо ui_init()+home_tile_time_init()
#include "bsp_display.h"        // абстракция яркости вместо main.c::setBrightnes
#include "svc_screen_sleep.h"
#include "svc_vibro.h"
#include "ui_pairing_popup.h"   // show_pairing_popup/close_pairing_popup_cb теперь тут
#include "drv_battery.h"
#include "svc_battery_monitor.h"
#include "drv_phtrz.h"
#include "svc_auto_brightness.h"

static const char *TAG = "main";
static SemaphoreHandle_t lvgl_mux = NULL;
static lv_display_t *s_disp = NULL; // ★ нужен коллбэку завершения QSPI-передачи

#define LCD_HOST  SPI2_HOST

#define EXAMPLE_PIN_NUM_LCD_CS      (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_PCLK    (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA0   (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA1   (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA2   (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA3   (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST     (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_BK_LIGHT    (-1)

#define EXAMPLE_LCD_H_RES           280
#define EXAMPLE_LCD_V_RES           456
#define EXAMPLE_USE_TOUCH           1

#define EXAMPLE_LVGL_BUF_HEIGHT     40
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS  500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS  1
#define EXAMPLE_LVGL_TASK_STACK_SIZE    (12 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY      5

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 80},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 1},
    {0x63, (uint8_t[]){0xFF}, 1, 1},
    {0x51, (uint8_t[]){0x00}, 1, 1},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

// ═══════════════════════════════════════════════════════
// CALLBACK-И ДЛЯ LVGL 9
// ═══════════════════════════════════════════════════════
static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1 + 0x14;  // Аппаратный сдвиг SH8601
    const int offsetx2 = area->x2 + 0x14;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    // ★ lv_display_flush_ready() ЗДЕСЬ БОЛЬШЕ НЕ ВЫЗЫВАЕТСЯ - см. example_notify_lvgl_flush_ready().
    // esp_lcd_panel_draw_bitmap() для QSPI лишь ставит DMA-транзакцию в очередь и
    // возвращается немедленно - вызов flush_ready() тут означал "буфер свободен"
    // ДО того, как данные реально долетели до панели. Это и было причиной
    // смазывания/артефактов именно в обновляемых областях.
}

// ★ КРИТИЧНО для SH8601: контроллер требует, чтобы область частичного
// обновления была выровнена по чётным границам (данные шлются блоками по 2
// пикселя из-за QSPI/внутренней адресации). Без этого LVGL9 присылает в
// flush_cb произвольные (нечётные) прямоугольники при точечных обновлениях
// (лейблы часов, шагомера и т.д.) - именно это и вызывает смазывание/
// артефакты, которых нет в demo-примере производителя (там rounder был,
// просто через LVGL8-style API).
static void example_lvgl_rounder_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);

    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

// ★ Реальное завершение QSPI-передачи (вызывается из ISR SPI-драйвера)
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                            esp_lcd_panel_io_event_data_t *edata,
                                            void *user_ctx)
{
    if (s_disp) {
        lv_display_flush_ready(s_disp);
    }
    return false;
}

#if EXAMPLE_USE_TOUCH
static void example_lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t tp_x, tp_y;
    uint8_t win = getTouch(&tp_x, &tp_y);

    if (win) {
        data->point.x = tp_y;
        data->point.y = tp_x;
        if (data->point.x >= EXAMPLE_LCD_H_RES) data->point.x = EXAMPLE_LCD_H_RES - 1;
        if (data->point.y >= EXAMPLE_LCD_V_RES) data->point.y = EXAMPLE_LCD_V_RES - 1;
        data->state = LV_INDEV_STATE_PRESSED;

        // ★ Любое касание считается активностью - будим экран / сбрасываем таймер сна.
        svc_screen_sleep_wake_up();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL 9 task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    while (1) {
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static void app_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    if (base != APP_EVENTS) return;

    switch (id) {
        case EVT_BLE_SHOW_PIN: {
            ble_pin_event_data_t *pin_data = (ble_pin_event_data_t *)event_data;
            if (pin_data) {
                if (example_lvgl_lock(-1)) {
                    show_pairing_popup(pin_data->pin);
                    example_lvgl_unlock();
                }
            }
            break;
        }

        case EVT_BLE_CLOSE_PIN: {
             if (example_lvgl_lock(-1)) {
                close_pairing_popup_cb(NULL);
                example_lvgl_unlock();
            }
            break;
        }

        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════
// ГЛАВНАЯ ФУНКЦИЯ
// ═══════════════════════════════════════════════════════
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "⚠️ NVS ПОВРЕЖДЁН ИЛИ НОВАЯ ВЕРСИЯ! ret=%s — СТИРАЮ ВСЁ!", esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(err));
        return;
    }

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        EXAMPLE_PIN_NUM_LCD_PCLK,
        EXAMPLE_PIN_NUM_LCD_DATA0,
        EXAMPLE_PIN_NUM_LCD_DATA1,
        EXAMPLE_PIN_NUM_LCD_DATA2,
        EXAMPLE_PIN_NUM_LCD_DATA3,
        EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 16 / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    // ★ ГЛАВНЫЙ ФИКС: раньше тут был NULL вместо коллбэка завершения передачи.
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        EXAMPLE_PIN_NUM_LCD_CS,
        example_notify_lvgl_flush_ready,   // было NULL
        NULL);
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    bsp_display_backlight_attach(io_handle); // ★ было: amoled_bl_handle = io_handle;

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_LOGI(TAG, "Install SH8601 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    I2C_master_Init();
#if EXAMPLE_USE_TOUCH
    touch_Init();
#endif

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, 1);
#endif

    ESP_LOGI(TAG, "Initialize LVGL 9");
    lv_init();

    size_t buf_size = EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(uint16_t);
    uint16_t *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    assert(buf1);
    uint16_t *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    assert(buf2);

    lv_display_t *disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    s_disp = disp; // ★ нужен example_notify_lvgl_flush_ready()
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);
    lv_display_add_event_cb(disp, example_lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);   // ← добавить эту строку
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, panel_handle);

#if EXAMPLE_USE_TOUCH
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, example_lvgl_touch_cb);
    lv_indev_set_display(indev, disp);
#endif

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    // ═══════════════════════════════════════════════════════
    // Сервисы, не зависящие от LVGL (можно поднимать до/после экрана)
    // ═══════════════════════════════════════════════════════
    svc_vibro_init();
    //drv_phtrz_init();               // ← фотодатчик (нужен авто-яркости)
    //svc_auto_brightness_start();    // ← ДОБАВИТЬ — создаёт state_mutex
    svc_screen_sleep_init();        // ← этот может дёрнуть resume при первом касании
    drv_battery_init();          // ADC для батареи
    svc_battery_monitor_start();
    bsp_display_set_brightness(80); 
    // ═══════════════════════════════════════════════════════
    // Запуск UI (под мьютексом!)
    // ═══════════════════════════════════════════════════════
    ESP_LOGI(TAG, "Display UI");
    if (example_lvgl_lock(-1)) {
        app_ui_init(); // ★ было: ui_init(); home_tile_time_init();
                       // app_ui_init() строит все экраны И запускает логику
                       // каждого из них (часы/шагомер/лента уведомлений/
                       // пульт/таймер/калькулятор и т.д.) - раньше логика
                       // экранов (кроме времени) вообще не вызывалась.
        example_lvgl_unlock();
    }
    ESP_ERROR_CHECK(esp_event_handler_register(APP_EVENTS, EVT_BLE_SHOW_PIN, app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_EVENTS, EVT_BLE_CLOSE_PIN, app_event_handler, NULL));

    svc_ble_sync_start();
}
