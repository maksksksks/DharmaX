idf_component_register(
    SRCS "svc_store.c"
    INCLUDE_DIRS "."
    REQUIRES 
        app_core 
        esp_event 
        freertos 
        log
)
