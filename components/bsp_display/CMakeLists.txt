idf_component_register(
    SRCS "bsp_display.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_lcd
        log
)
