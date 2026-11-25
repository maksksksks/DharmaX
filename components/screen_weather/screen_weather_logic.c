#include "screen_weather_logic.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_WEATHER";

static void logic_init(void)
{
    // TODO: наполнить логикой экрана ScreenWeather
    // Пример: esp_event_handler_instance_register(APP_EVENTS, EVT_..., handler, NULL, NULL);
    ESP_LOGI(TAG, "ScreenWeather logic stub initialized");
}

static void logic_deinit(void)
{
    // ничего своего: ни подписок, ни таймеров; ui_ScreenWeather_screen_destroy() обнулит ui_*
}

lv_obj_t *screen_weather_create(void)
{
    ui_ScreenWeather_screen_init();
    logic_init();
    return ui_ScreenWeather;
}

void screen_weather_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenWeather
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenWeather_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
