#pragma once
#include "esp_err.h"
#include <stddef.h>

esp_err_t image_fetch_http_to_sd(const char *url, const char *dest_path);

/**
 * @brief Download an image over HTTP into PSRAM.
 *
 * The function streams the response and grows the PSRAM buffer as needed
 * until `esp_http_client_read()` returns 0. A SHA-256 checksum supplied by
 * the server via the `X-File-SHA256` header is verified to ensure data integrity.
 *
 * @param url  HTTP URL of the image to download
 * @param data Output pointer receiving the allocated buffer
 * @param len  Output length of the downloaded data in bytes
 *
 * @return ESP_OK on success, or an error code on failure.
 *
 * @note The caller is responsible for calling free() on *data when done.
 */
esp_err_t image_fetch_http_to_psram(const char *url, uint8_t **data, size_t *len);
