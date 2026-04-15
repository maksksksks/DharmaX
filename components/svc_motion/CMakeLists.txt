idf_component_register(
    SRCS "svc_motion.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos 
        log 
        drv_gyro
        app_core
        esp_timer
)