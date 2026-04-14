idf_component_register(
    SRCS "svc_screen_sleep.c"
    INCLUDE_DIRS "."
    REQUIRES 
        lvgl 
        freertos 
        log 
        esp_bsp 
        app_core 
        svc_auto_brightness
)