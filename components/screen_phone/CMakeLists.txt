idf_component_register(
    SRCS
        "ui_ScreenPhone.c"
        "screen_phone_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        app_nav
)
