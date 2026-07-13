#include "screen_weather_logic.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_WEATHER";

void screen_weather_logic_init(void)
{
    // TODO: наполнить логикой экрана ScreenWeather
    // Пример: esp_event_handler_instance_register(APP_EVENTS, EVT_..., handler, NULL, NULL);
    ESP_LOGI(TAG, "ScreenWeather logic stub initialized");
}

void ui_ScreenWeather_full_init(void)
{
    ui_ScreenWeather_screen_init();
    screen_weather_logic_init();
}