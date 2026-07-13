idf_component_register(
    SRCS "svc_screen_sleep.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        bsp_display
        svc_auto_brightness
        screen_aod
)
