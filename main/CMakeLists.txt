idf_component_register(
    SRCS "main.c" 
        "lv_fs_port.c"     # Ваш старый файл
    INCLUDE_DIRS "."
    REQUIRES 
        esp_bsp 
        esp_lvgl_port 
        #iot_button 
        lvgl_ui
        app_core           # ДОБАВИЛИ
        drv_battery        # ДОБАВИЛИ
        svc_battery_monitor # ДОБАВИЛИ
        svc_ble_sync
        drv_phtrz
        svc_lighting_monitor
        svc_auto_brightness
        svc_screen_sleep
        drv_vibro
        svc_vibro
        svc_motion
        #home_tile
)