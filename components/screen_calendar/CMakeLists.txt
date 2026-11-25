idf_component_register(
    SRCS
        "ui_ScreenCalendar.c"
        "screen_calendar_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        app_nav
)
