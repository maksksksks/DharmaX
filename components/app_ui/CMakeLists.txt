idf_component_register(
    SRCS "app_ui.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        screen_home
        screen_notify
        screen_phone
        screen_timeutils
        screen_calc
        screen_weather
        screen_sd
        screen_calendar
        screen_settings
        screen_system
        screen_weatherdetails
        screen_news
        screen_events
        screen_sys
        screen_ir
        screen_aod
)
