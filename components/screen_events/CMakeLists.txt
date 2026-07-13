idf_component_register(
    SRCS
        "ui_ScreenEvents.c"
        "screen_events_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        ui_widgets
)
