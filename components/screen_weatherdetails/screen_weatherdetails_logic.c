#include "screen_weatherdetails_logic.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_WEATHERDETAILS";

void screen_weatherdetails_logic_init(void)
{
    // TODO: наполнить логикой экрана ScreenWeatherDetails
    // Пример: esp_event_handler_instance_register(APP_EVENTS, EVT_..., handler, NULL, NULL);
    ESP_LOGI(TAG, "ScreenWeatherDetails logic stub initialized");
}

void ui_ScreenWeatherDetails_full_init(void)
{
    ui_ScreenWeatherDetails_screen_init();
    screen_weatherdetails_logic_init();
}