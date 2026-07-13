idf_component_register(
    SRCS
        "ui_ScreenNews.c"
        "screen_news_logic.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        ui
        ui_widgets
)
