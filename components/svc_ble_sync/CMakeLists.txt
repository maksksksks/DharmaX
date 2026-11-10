idf_component_register(
    SRCS
        "svc_ble_sync.c"
        "include/ble_state.c"
        "include/ble_advertise.c"
        "include/ble_logs.c"

    INCLUDE_DIRS
        "."

    PRIV_INCLUDE_DIRS
        "include"

    REQUIRES
        freertos
        log
        esp_event
        bt
        app_core
        nvs_flash
        svc_vibro
        ui
        svc_screen_sleep
        json
)