idf_component_register(
    SRCS
        "ui_ScreenTimeUtils.c"
        "screen_timeutils_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        esp_timer
        svc_vibro
)
