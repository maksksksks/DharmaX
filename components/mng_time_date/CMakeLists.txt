idf_component_register(
    SRCS "mng_time_date.c"
    INCLUDE_DIRS "."
    REQUIRES 
        log 
        ui
        lvgl
)