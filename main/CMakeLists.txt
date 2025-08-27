set(EXTRA_INCLUDES ".")
if(NOT "${CONFIG_UPLOAD_AUTH_TOKEN}" STREQUAL "")
    string(SHA256 AUTH_TOKEN_HASH "${CONFIG_UPLOAD_AUTH_TOKEN}")
    string(REGEX REPLACE "(..)" "0x\\1," AUTH_TOKEN_HASH_BYTES "${AUTH_TOKEN_HASH}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/auth_token_hash.h "static const uint8_t upload_token_hash[32] = {${AUTH_TOKEN_HASH_BYTES}};\n")
    list(APPEND EXTRA_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
    set(HAVE_AUTH_TOKEN 1)
endif()

idf_component_register(
    SRCS "main.c" "file_manager.c" "touch_task.c" "http_server.c"
    INCLUDE_DIRS ${EXTRA_INCLUDES}
    REQUIRES
        config
        rgb_lcd_port
        gui
        lvgl
        lvgl_fs
        touch
        ui_navigation
        sd
        battery
        wifi
        image_fetcher
        esp_http_server
        can_display
        rs485_display
    PRIV_REQUIRES esp_psram
    WHOLE_ARCHIVE
    )

if(HAVE_AUTH_TOKEN)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_UPLOAD_AUTH_TOKEN_PRESENT=1)
endif()
