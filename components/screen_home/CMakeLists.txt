idf_component_register(
    SRCS
        "ui_ScreenHome.c"
        "screen_home_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        esp_event
        svc_screen_sleep
)
