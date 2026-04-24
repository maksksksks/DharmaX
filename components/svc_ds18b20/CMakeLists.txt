idf_component_register(
    SRCS "svc_ds18b20.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos 
        log 
        drv_ds18b20
        app_core
)