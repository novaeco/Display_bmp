
idf_component_register(
    SRCS "main.c" "file_manager.c" "touch_task.c" "http_server.c"
    INCLUDE_DIRS "."
    REQUIRES config rgb_lcd_port gui_paint touch ui_navigation sd battery wifi image_fetcher esp_http_server
    WHOLE_ARCHIVE
    )
