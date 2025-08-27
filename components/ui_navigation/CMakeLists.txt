idf_component_register(
    SRCS "ui_navigation.c"
    INCLUDE_DIRS "."
    REQUIRES config gui lvgl lvgl_fs touch
    PRIV_REQUIRES battery main
)
