#pragma once
#include "esp_err.h"
#include <stddef.h>

esp_err_t image_fetch_http_to_sd(const char *url, const char *dest_path);
esp_err_t image_fetch_http_to_psram(const char *url, uint8_t **data, size_t *len);
