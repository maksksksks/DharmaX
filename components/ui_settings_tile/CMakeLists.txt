idf_component_register(
    SRCS "settings_tile.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl log app_core svc_auto_brightness svc_battery_monitor svc_ble_sync svc_lighting_monitor svc_screen_sleep
)