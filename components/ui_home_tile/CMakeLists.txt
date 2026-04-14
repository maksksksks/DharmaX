idf_component_register(
    SRCS "home_tile.c" "assets/bg.c"
    INCLUDE_DIRS "."
    REQUIRES 
        lvgl 
        log 
        app_core
)