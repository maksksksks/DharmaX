idf_component_register(
    SRCS
        "ui_ScreenSettings.c"
        "screen_settings_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        svc_battery_monitor
        svc_ble_sync
        svc_lighting_monitor
)
