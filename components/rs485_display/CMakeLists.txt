idf_component_register(SRCS "rs485_display.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver freertos touch config ui_navigation)
