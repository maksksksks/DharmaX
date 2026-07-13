idf_component_register(
    SRCS
        "ui_ScreenCalc.c"
        "screen_calc_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
)
