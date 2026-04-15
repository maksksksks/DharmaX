idf_component_register(
    SRCS "drv_ds18b20.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_common 
        log 
)