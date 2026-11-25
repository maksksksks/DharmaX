idf_component_register(
    SRCS
        "ui_ScreenSystem.c"
        "screen_system_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        app_nav
        spi_flash
        esp_timer
)
