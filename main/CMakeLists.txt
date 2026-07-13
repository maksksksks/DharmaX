idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
            ui 
            esp_lcd_sh8601 
            lvgl 
            esp_timer 
            driver
            i2c_bsp
            esp_touch
            svc_ble_sync
            mng_time_date
            app_core
            app_ui
            bsp_display
            svc_screen_sleep
            svc_vibro
            ui_pairing_popup
)