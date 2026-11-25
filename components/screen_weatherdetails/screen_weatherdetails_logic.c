#include "screen_weatherdetails_logic.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "SCREEN_WEATHERDETAILS";

static void logic_init(void)
{
    // TODO: наполнить логикой экрана ScreenWeatherDetails
    // Пример: esp_event_handler_instance_register(APP_EVENTS, EVT_..., handler, NULL, NULL);
    ESP_LOGI(TAG, "ScreenWeatherDetails logic stub initialized");
}

static void logic_deinit(void)
{
    // ничего своего: ни подписок, ни таймеров; ui_ScreenWeatherDetails_screen_destroy() обнулит ui_*
}

lv_obj_t *screen_weatherdetails_create(void)
{
    ui_ScreenWeatherDetails_screen_init();
    logic_init();
    return ui_ScreenWeatherDetails;
}

void screen_weatherdetails_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenWeatherDetails
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenWeatherDetails_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
