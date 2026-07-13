idf_component_register(
    SRCS
        "ui_ScreenWeather.c"
        "screen_weather_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
)
