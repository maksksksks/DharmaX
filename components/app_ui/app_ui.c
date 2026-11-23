#include "app_ui.h"
#include "ui.h"

#include "screen_home_logic.h"
#include "screen_notify_logic.h"
#include "screen_phone_logic.h"
#include "screen_timeutils_logic.h"
#include "screen_calc_logic.h"
#include "screen_weather_logic.h"
#include "screen_sd_logic.h"
#include "screen_calendar_logic.h"
#include "screen_settings_logic.h"
#include "screen_system_logic.h"
#include "screen_weatherdetails_logic.h"
#include "screen_news_logic.h"
#include "screen_events_logic.h"
#include "screen_sys_logic.h"
#include "screen_ir_logic.h"
#include "screen_aod_logic.h"

#include "esp_log.h"

static const char *TAG = "APP_UI";

void app_ui_init(void)
{
    // 1. Строим все экраны (SquareLine-сгенерированное дерево виджетов).
    ui_init();

    // 2. Довешиваем логику каждого экрана
    screen_home_logic_init();
    // screen_notify_logic_init();
    // screen_phone_logic_init();
    // screen_timeutils_logic_init();
    // screen_calc_logic_init();
    // screen_weather_logic_init();
    // screen_sd_logic_init();
    // screen_calendar_logic_init();
    // screen_settings_logic_init();
    // screen_system_logic_init();
    // screen_weatherdetails_logic_init();
    screen_news_logic_init();
    // screen_events_logic_init();
    // screen_sys_logic_init();
    // screen_ir_logic_init();
    screen_aod_logic_init();

    ESP_LOGI(TAG, "App UI fully initialized");
}
