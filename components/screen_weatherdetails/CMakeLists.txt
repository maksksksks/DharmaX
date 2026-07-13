idf_component_register(
    SRCS
        "ui_ScreenWeatherDetails.c"
        "screen_weatherdetails_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
)
