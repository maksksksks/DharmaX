idf_component_register(
    SRCS
        "ui.c"
        "ui_helpers.c"
        "ui_comp_hook.c"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        log
        app_core
        ui_resources
        # Заголовки всех экранов теперь лежат в их собственных папках
        # (ui_ScreenX.h рядом с ui_ScreenX.c), а не здесь. ui.c дергает
        # *_screen_init()/*_screen_destroy() каждого экрана, поэтому ему
        # нужны их заголовки - отсюда REQUIRES на каждый screen_*.
        # Экраны, в свою очередь, требуют "ui" обратно (им нужны ui.h/
        # ui_helpers.h/ui_theme.h и _ui_screen_change для навигации).
        # Это циклическая зависимость ui <-> screen_*, и это НЕ ошибка -
        # ESP-IDF официально поддерживает такие циклы (компоновщик
        # дважды подставляет библиотеки на этапе линковки), см.
        # https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html
        # (раздел "Component Circular Dependencies").
        screen_home
        screen_notify
        screen_phone
        screen_timeutils
        screen_calc
        screen_weather
        screen_sd
        screen_calendar
        screen_settings
        screen_system
        screen_weatherdetails
        screen_news
        screen_events
        screen_sys
        screen_ir
        screen_aod
)
