idf_component_register(
    SRCS
        "ui_ScreenIR.c"
        "screen_ir_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        app_nav
        svc_ir_transmit
)
