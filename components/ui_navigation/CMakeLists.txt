idf_component_register(
    SRCS "ui_navigation.c"
    INCLUDE_DIRS "."
    REQUIRES config gui_paint touch
    PRIV_REQUIRES battery main
)
