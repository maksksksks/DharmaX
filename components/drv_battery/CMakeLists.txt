idf_component_register(
    SRCS "drv_battery.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_adc
        log
        adc_shared
)
