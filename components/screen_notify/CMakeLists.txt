idf_component_register(
    SRCS
        "ui_ScreenNotify.c"
        "screen_notify_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        app_nav
        ui_widgets
)
