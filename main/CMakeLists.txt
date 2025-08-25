idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES config rgb_lcd_port gui_paint touch sd battery
    WHOLE_ARCHIVE
    )
