#include "image_fetcher.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include <stdio.h>

static const char *TAG = "image_fetcher";

#if !CONFIG_IMAGE_FETCH_INSECURE
extern const char cert_pem_start[] asm("_binary_cert_pem_start");
#endif

#ifndef ESP_ERR_HTTP_STATUS
#define ESP_ERR_HTTP_STATUS (ESP_ERR_HTTP_BASE + 0x0F)
#endif

esp_err_t image_fetch_http_to_sd(const char *url, const char *dest_path)
{
    esp_http_client_config_t cfg = {
        .url = url,
#if CONFIG_IMAGE_FETCH_INSECURE
        .cert_pem = NULL,
        .skip_cert_common_name_check = true,
#else
        .cert_pem = cert_pem_start,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        err = content_length;
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_STATUS;
    }

    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    uint8_t buf[512];
    int data_read;
    int total_read = 0;
    while (total_read < content_length) {
        data_read = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (data_read < 0) {
            err = data_read;
            break;
        } else if (data_read == 0) {
            err = ESP_ERR_HTTP_EAGAIN;
            break;
        }
        fwrite(buf, 1, data_read, f);
        total_read += data_read;
    }
    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || total_read != content_length) {
        remove(dest_path);
        return (err != ESP_OK) ? err : ESP_ERR_HTTP_WRITE_DATA;
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
#if CONFIG_IMAGE_FETCH_INSECURE
        .cert_pem = NULL,
        .skip_cert_common_name_check = true,
#else
        .cert_pem = cert_pem_start,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        err = content_length;
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_STATUS;
    }
    uint8_t *buf = heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    int read = 0;
    err = ESP_OK;
    while (read < content_length) {
        int r = esp_http_client_read(client, (char *)buf + read, content_length - read);
        if (r < 0) {
            err = r;
            break;
        } else if (r == 0) {
            err = ESP_ERR_HTTP_EAGAIN;
            break;
        }
        read += r;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || read != content_length) {
        free(buf);
        return (err != ESP_OK) ? err : ESP_ERR_HTTP_WRITE_DATA;
    }
    *data = buf;
    *len = content_length;
    ESP_LOGI(TAG, "Downloaded %d bytes from %s", content_length, url);
    return ESP_OK;
}
