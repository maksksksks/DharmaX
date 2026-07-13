idf_component_register(
    SRCS "svc_vibro.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        log
        drv_vibro
        app_core
)
