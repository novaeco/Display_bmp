idf_component_register(SRCS "gui_bmp.c" "gui_paint.c" "gui_image.c" "gui_jpg.c" "gui_png.c"
                        INCLUDE_DIRS "."
                        REQUIRES fonts esp_timer jpeg png
                        )
