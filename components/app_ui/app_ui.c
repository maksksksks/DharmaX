#include "app_ui.h"
#include "ui.h"
#include "app_nav.h"

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

// Реестр экранов. Порядок не важен, id - ключ для nav_push/nav_replace.
// home резидентный (создаётся один раз ниже), остальные живут от nav_push/
// nav_replace до nav_pop/nav_replace/nav_back_to_home.
static const screen_desc_t k_screens[] = {
    { NAV_HOME_ID,      screen_home_create,           screen_home_destroy           },
    { "notify",         screen_notify_create,         screen_notify_destroy         },
    { "phone",          screen_phone_create,          screen_phone_destroy          },
    { "timeutils",      screen_timeutils_create,      screen_timeutils_destroy      },
    { "calc",           screen_calc_create,           screen_calc_destroy           },
    { "weather",        screen_weather_create,        screen_weather_destroy        },
    { "sd",             screen_sd_create,             screen_sd_destroy             },
    { "calendar",       screen_calendar_create,       screen_calendar_destroy       },
    { "settings",       screen_settings_create,       screen_settings_destroy       },
    { "system",         screen_system_create,         screen_system_destroy         },
    { "weatherdetails", screen_weatherdetails_create, screen_weatherdetails_destroy },
    { "news",           screen_news_create,           screen_news_destroy           },
    { "events",         screen_events_create,         screen_events_destroy         },
    { "sys",            screen_sys_create,            screen_sys_destroy            },
    { "ir",             screen_ir_create,             screen_ir_destroy             },
    { "aod",            screen_aod_create,            screen_aod_destroy            },
};

void app_ui_init(void)
{
    // 1. Тема дисплея (единственное, что осталось от старого массового ui_init()).
    ui_init();

    // 2. Слой данных, который обязан работать, пока экранов нет:
    //    - ленты уведомлений / событий / новостей (карточки не должны теряться,
    //      когда экран уничтожен);
    //    - триггеры AOD по EVT_MOTION_WRIST_SLEEP/RAISE.
    screen_notify_service_init();
    screen_events_service_init();
    screen_news_service_init();
    screen_aod_logic_init();

    // 3. Навигатор: регистрируем дескрипторы и поднимаем резидентный home.
    //    Остальные экраны создаются лениво, по nav_push/nav_replace.
    nav_init(k_screens, sizeof(k_screens) / sizeof(k_screens[0]));
    nav_push(NAV_HOME_ID);

    ESP_LOGI(TAG, "App UI initialized, home is up");
}
