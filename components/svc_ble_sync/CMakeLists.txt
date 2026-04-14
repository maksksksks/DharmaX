idf_component_register(
    SRCS "svc_ble_sync.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos 
        log 
        esp_event 
        bt 
        app_core 
        nvs_flash
        svc_vibro
)