idf_component_register(
    SRCS
        "ui_ScreenSD.c"
        "screen_sd_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
)
