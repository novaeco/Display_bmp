idf_component_register(
    SRCS "main.c" "file_manager.c" "ui_navigation.c" "touch_task.c"
    INCLUDE_DIRS "."
    REQUIRES config rgb_lcd_port gui_paint touch sd battery wifi image_fetcher
    WHOLE_ARCHIVE
    )
