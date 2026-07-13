idf_component_register(
    SRCS "svc_auto_brightness.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos 
        log 
        drv_phtrz
        app_core
        bsp_display
)