idf_component_register(SRCS "rs485_display.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver freertos config ui_navigation
                       PRIV_REQUIRES touch)
