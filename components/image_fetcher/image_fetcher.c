#include "image_fetcher.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>

static const char *TAG = "image_fetcher";

esp_err_t image_fetch_http_to_sd(const char *url, const char *dest_path)
{
    esp_http_client_config_t cfg = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }
    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    uint8_t buf[512];
    int data_read;
    while ((data_read = esp_http_client_read(client, (char *)buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, data_read, f);
    }
    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "Downloaded %s to %s", url, dest_path);
    return ESP_OK;
}

esp_err_t image_fetch_http_to_psram(const char *url, uint8_t **data, size_t *len)
{
    *data = NULL;
    *len = 0;
    esp_http_client_config_t cfg = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    uint8_t *buf = heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    int read = 0;
    while (read < content_length) {
        int r = esp_http_client_read(client, (char *)buf + read, content_length - read);
        if (r <= 0) break;
        read += r;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (read != content_length) {
        free(buf);
        return ESP_FAIL;
    }
    *data = buf;
    *len = content_length;
    ESP_LOGI(TAG, "Downloaded %d bytes from %s", content_length, url);
    return ESP_OK;
}
