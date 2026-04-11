idf_component_register(
    SRCS "svc_lighting_monitor.c"
    INCLUDE_DIRS "."
    REQUIRES freertos log drv_phtrz app_core
)