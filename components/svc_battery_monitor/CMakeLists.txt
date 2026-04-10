idf_component_register(
    SRCS "svc_battery_monitor.c"
    INCLUDE_DIRS "."
    REQUIRES freertos log drv_battery app_core
)