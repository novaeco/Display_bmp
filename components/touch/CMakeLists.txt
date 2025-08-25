
idf_component_register(SRCS "gt911.c" "touch.c"
                        INCLUDE_DIRS "."
                        REQUIRES driver  esp_lcd i2c gpio io_extension rgb_lcd_port
                    )

set_target_properties(${COMPONENT_LIB} PROPERTIES PUBLIC_HEADER "esp_lcd_touch.h;touch.h")
