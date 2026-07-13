idf_component_register(
    SRCS "drv_vibro.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_common 
        log 
        #esp_err 
        esp_adc
        driver
)