idf_component_register(
    SRCS "main.c" "file_manager.c" "ui_navigation.c" "touch_task.c" "http_server.c"
    INCLUDE_DIRS "."
    REQUIRES config rgb_lcd_port gui_paint touch sd battery wifi image_fetcher esp_http_server esp_lcd_touch
    WHOLE_ARCHIVE
    )
