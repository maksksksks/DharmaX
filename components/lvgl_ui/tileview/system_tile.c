#include "system_tile.h"

#include "bsp_sdcard.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_photores.h"

#include "esp_flash.h"
#include "esp_psram.h"
#include "driver/temperature_sensor.h"
#include "esp_private/esp_clk.h"

#include "onewire_bus.h"
#include "ds18b20.h"

static lv_obj_t *label_brightness;
static lv_obj_t *label_chip_temp;
static lv_obj_t *label_ds18_temp;
static lv_obj_t *label_battery;
static lv_obj_t *label_photores;

temperature_sensor_handle_t temp_sensor = NULL;
static float ds18_last_temp = 0;

#define DS18B20_GPIO    6
onewire_bus_handle_t bus;
ds18b20_device_handle_t ds18b20;

static void ds18_task(void *arg)
{
    float temp;

    while (1) {
        ds18b20_trigger_temperature_conversion(ds18b20);
        vTaskDelay(pdMS_TO_TICKS(800));

        if (ds18b20_get_temperature(ds18b20, &temp) == ESP_OK) {
            ds18_last_temp = temp;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void slider_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        // printf("Slider value: %d\n", value);

        lv_label_set_text_fmt(label_brightness, "%d%%", value);
        bsp_display_set_brightness(value);
        // bsp_display_handle_t display = bsp_display_get_handle();
        // display->set_brightness(display, value);
        lv_event_stop_bubbling(e);
    }
}

static void system_time_cb(lv_timer_t *timer) {
    char str[20];
    float tsens_out;
    float voltage;
    int photo;
    bsp_battery_get_voltage(&voltage, NULL);
    sprintf(str, "%.2f V", voltage);
    lv_label_set_text(label_battery, str);

    temperature_sensor_get_celsius(temp_sensor, &tsens_out);
    sprintf(str, "%d C", (int)tsens_out);
    lv_label_set_text(label_chip_temp, str);

    // if (!isnan(ds18_last_temp)) {
    sprintf(str, "%d C", (int)ds18_last_temp);
    lv_label_set_text(label_ds18_temp, str);

    lv_label_set_text(label_ds18_temp, str);
    bsp_photores_get_voltage(&photo);
    sprintf(str, "%d", photo);
    lv_label_set_text(label_photores, str);
}

void ds18_init() {
    /* ---------- 1-Wire bus ---------- */
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = DS18B20_GPIO
    };
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,   // достаточно для DS18B20
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    /* ---------- DS18B20 ---------- */
    ds18b20_config_t ds18b20_cfg = {};
    ESP_ERROR_CHECK(ds18b20_new_device_from_bus(bus, &ds18b20_cfg, &ds18b20));
}


void system_tile_init(lv_obj_t *parent) {
    uint32_t flash_size;
    uint32_t psram_size;
    uint64_t sdcard_size;
    uint32_t cpu_freq;

    ds18_init();
    
    /*Create a list*/
    lv_obj_t *list = lv_list_create(parent);
    lv_obj_t *lable =  lv_label_create(parent);
    lv_obj_set_style_text_font(lable, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_label_set_text(lable, "System");
    lv_obj_align(lable, LV_ALIGN_TOP_MID, 0, 3);


    lv_obj_set_size(list, lv_pct(100), lv_pct(75));
    // lv_obj_center(list);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);

    // lv_obj_t *button = lv_btn_create(parent);
    // lable = lv_label_create(parent);
    // lv_label_set_text(lable, "ES8311\nTest");
    // lv_obj_align(lable, LV_ALIGN_BOTTOM_MID, -125, -15);

    // lv_obj_t *sw = lv_switch_create(parent);
    // lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, -75, -15);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 1, 100);    
    lv_slider_set_value(slider, 80, LV_ANIM_OFF); 

    lv_obj_set_size(slider, lv_pct(70), lv_pct(5));     
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -18); 
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *list_item;
    list_item = lv_list_add_btn(list, NULL, "Chip");
    lv_obj_t *label_chip = lv_label_create(list_item);
    lv_label_set_text(label_chip, "ESP32-S3");

    list_item = lv_list_add_btn(list, NULL, "CPUtemp");
    label_chip_temp = lv_label_create(list_item);
    lv_label_set_text(label_chip_temp, "--- C");

    list_item = lv_list_add_btn(list, NULL, "DS18temp");
    label_ds18_temp = lv_label_create(list_item);
    lv_label_set_text(label_ds18_temp, "--- C");

    list_item = lv_list_add_btn(list, NULL, "Freq");
    lv_obj_t *label_chip_freq = lv_label_create(list_item);
    cpu_freq = esp_clk_cpu_freq();
    lv_label_set_text_fmt(label_chip_freq, "%d MHz", (int)(cpu_freq / 1000 / 1000));

    list_item = lv_list_add_btn(list, NULL, "Brightness");
    label_brightness = lv_label_create(list_item);
    uint8_t brightness = bsp_display_get_brightness();
    lv_label_set_text_fmt(label_brightness, "%d%%", brightness);
    // lv_label_set_text(label_brightness, "100%");

    list_item = lv_list_add_btn(list, NULL, "RAM");
    lv_obj_t *label_ram = lv_label_create(list_item);
    lv_label_set_text(label_ram, "512 KB");

    list_item = lv_list_add_btn(list, NULL, "PSRAM");
    lv_obj_t *label_psram = lv_label_create(list_item);
    psram_size = (uint32_t)esp_psram_get_size();
    lv_label_set_text_fmt(label_psram, "%d MB", (int)(psram_size / 1024 / 1024));

    list_item = lv_list_add_btn(list, NULL, "Flash");
    lv_obj_t *label_flash = lv_label_create(list_item);
    esp_flash_get_size(NULL, &flash_size);
    lv_label_set_text_fmt(label_flash, "%d MB", (int)(flash_size / 1024 / 1024));

    list_item = lv_list_add_btn(list, NULL, "SDCard");
    lv_obj_t *label_sd = lv_label_create(list_item);
    sdcard_size = bsp_sdcard_get_size();
    lv_label_set_text_fmt(label_sd, "%d MB", (int)(sdcard_size / 1024 / 1024));
    
    list_item = lv_list_add_btn(list, NULL, "Battery");
    label_battery = lv_label_create(list_item);
    lv_label_set_text(label_battery, "--- V");

    list_item = lv_list_add_btn(list, NULL, "Photores");
    label_photores = lv_label_create(list_item);
    lv_label_set_text(label_photores, "----");

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    temperature_sensor_install(&temp_sensor_config, &temp_sensor);
    temperature_sensor_enable(temp_sensor);
    
    lv_timer_create(system_time_cb, 1000, NULL);

    xTaskCreatePinnedToCore(
        ds18_task,
        "ds18_task",
        2048,
        NULL,
        5,
        NULL,
        1
    );
}
