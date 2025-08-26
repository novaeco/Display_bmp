#include "image_fetcher.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mbedtls/sha256.h"

static const char *TAG = "image_fetcher";

extern const char cert_pem_start[] asm("_binary_cert_pem_start");

#define SHA256_HEADER "X-File-SHA256"

#ifndef ESP_ERR_HTTP_STATUS
#define ESP_ERR_HTTP_STATUS (ESP_ERR_HTTP_BASE + 0x0F)
#endif
#ifndef ESP_ERR_HTTP_FETCH_HEADER
#define ESP_ERR_HTTP_FETCH_HEADER (ESP_ERR_HTTP_BASE + 0x07)
#endif

esp_err_t image_fetch_http_to_sd(const char *url, const char *dest_path)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = cert_pem_start,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Connection", "keep-alive");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_http_client_fetch_headers(client);
    if (err != ESP_OK && err != ESP_ERR_HTTP_FETCH_HEADER) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }
    int content_length = esp_http_client_get_content_length(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_STATUS;
    }

    char *hash_hex = NULL;
    if (esp_http_client_get_header(client, SHA256_HEADER, &hash_hex) != ESP_OK || !hash_hex) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t expected_hash[32];
    if (strlen(hash_hex) != 64) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }
    for (int i = 0; i < 32; ++i) {
        char tmp[3] = { hash_hex[2*i], hash_hex[2*i+1], 0 };
        char *end;
        long v = strtol(tmp, &end, 16);
        if (*end != 0 || v < 0 || v > 0xFF) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_RESPONSE;
        }
        expected_hash[i] = (uint8_t)v;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    FILE *f = NULL;
    int rc = mbedtls_sha256_starts(&sha_ctx, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_starts failed: %d", rc);
        if (f) {
            fclose(f);
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        remove(dest_path);
        mbedtls_sha256_free(&sha_ctx);
        return ESP_FAIL;
    }

    f = fopen(dest_path, "wb");
    if (!f) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        mbedtls_sha256_free(&sha_ctx);
        return ESP_FAIL;
    }
    uint8_t buf[512];
    int data_read;
    int total_read = 0;
    err = ESP_OK;
    while (1) {
        data_read = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (data_read < 0) {
            err = data_read;
            break;
        } else if (data_read == 0) {
            break;
        }
        fwrite(buf, 1, data_read, f);
        total_read += data_read;
        rc = mbedtls_sha256_update(&sha_ctx, buf, data_read);
        if (rc != 0) {
            ESP_LOGE(TAG, "mbedtls_sha256_update failed: %d", rc);
            fclose(f);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            remove(dest_path);
            mbedtls_sha256_free(&sha_ctx);
            return ESP_FAIL;
        }
    }
    fclose(f);
    uint8_t actual_hash[32];
    rc = mbedtls_sha256_finish(&sha_ctx, actual_hash);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_finish failed: %d", rc);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        remove(dest_path);
        mbedtls_sha256_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha256_free(&sha_ctx);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || (content_length >= 0 && total_read != content_length)) {
        remove(dest_path);
        return (err != ESP_OK) ? err : ESP_ERR_HTTP_WRITE_DATA;
    }
    if (memcmp(actual_hash, expected_hash, sizeof(expected_hash)) != 0) {
        remove(dest_path);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Downloaded %s to %s", url, dest_path);
    return ESP_OK;
}

esp_err_t image_fetch_http_to_psram(const char *url, uint8_t **data, size_t *len)
{
    *data = NULL;
    *len = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = cert_pem_start,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Connection", "keep-alive");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    err = esp_http_client_fetch_headers(client);
    if (err != ESP_OK && err != ESP_ERR_HTTP_FETCH_HEADER) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }
    int content_length = esp_http_client_get_content_length(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_STATUS;
    }

    char *hash_hex = NULL;
    if (esp_http_client_get_header(client, SHA256_HEADER, &hash_hex) != ESP_OK || !hash_hex || strlen(hash_hex) != 64) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t expected_hash[32];
    for (int i = 0; i < 32; ++i) {
        char tmp[3] = { hash_hex[2*i], hash_hex[2*i+1], 0 };
        char *end;
        long v = strtol(tmp, &end, 16);
        if (*end != 0 || v < 0 || v > 0xFF) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_RESPONSE;
        }
        expected_hash[i] = (uint8_t)v;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    int rc = mbedtls_sha256_starts(&sha_ctx, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_starts failed: %d", rc);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        mbedtls_sha256_free(&sha_ctx);
        return ESP_FAIL;
    }

    uint8_t *buf = NULL;
    size_t cap = 0;
    size_t total = 0;
    uint8_t tmp_buf[512];
    err = ESP_OK;
    while (1) {
        int r = esp_http_client_read(client, (char *)tmp_buf, sizeof(tmp_buf));
        if (r < 0) {
            err = r;
            break;
        } else if (r == 0) {
            break;
        }
        if (total + r > cap) {
            size_t new_cap = cap + ((r > sizeof(tmp_buf)) ? r : sizeof(tmp_buf));
            uint8_t *new_buf = heap_caps_realloc(buf, new_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!new_buf) {
                err = ESP_ERR_NO_MEM;
                break;
            }
            buf = new_buf;
            cap = new_cap;
        }
        memcpy(buf + total, tmp_buf, r);
        total += r;
        rc = mbedtls_sha256_update(&sha_ctx, tmp_buf, r);
        if (rc != 0) {
            ESP_LOGE(TAG, "mbedtls_sha256_update failed: %d", rc);
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            mbedtls_sha256_free(&sha_ctx);
            return ESP_FAIL;
        }
    }

    uint8_t actual_hash[32];
    rc = mbedtls_sha256_finish(&sha_ctx, actual_hash);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_finish failed: %d", rc);
        free(buf);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        mbedtls_sha256_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha256_free(&sha_ctx);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || (content_length >= 0 && total != content_length)) {
        free(buf);
        return (err != ESP_OK) ? err : ESP_ERR_HTTP_WRITE_DATA;
    }
    if (memcmp(actual_hash, expected_hash, sizeof(expected_hash)) != 0) {
        free(buf);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *data = buf;
    *len = total;
    ESP_LOGI(TAG, "Downloaded %d bytes from %s", (int)total, url);
    return ESP_OK;
}
