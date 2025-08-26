#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sd.h"
#include "sdkconfig.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#ifdef CONFIG_UPLOAD_AUTH_TOKEN_PRESENT
#include "mbedtls/sha256.h"
#include "auth_token_hash.h"
#endif

static const char *TAG = "http_srv";
static httpd_handle_t s_server = NULL;

static const char upload_html[] =
"<!DOCTYPE html><html><body><h2>Upload BMP</h2>"
"<input id=\"file\" type=\"file\" accept=\".bmp\"><button onclick=\"u()\">Upload</button>"
"<script>function u(){var f=document.getElementById('file').files[0];" 
"if(!f){alert('No file');return;}var x=new XMLHttpRequest();"
"x.open('POST','/upload/'+f.name,true);x.send(f);}</script></body></html>";

static void sanitize_filename(char *out, const char *in, size_t len)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j < len - 1; ++i) {
        char c = in[i];
        if (c == '/' || c == '\\') {
            continue;
        }
        if (c == '.' && in[i + 1] == '.') {
            ++i;
            continue;
        }
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, upload_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[128] = "";
    FILE *f = NULL;
    const char *filename = req->uri + sizeof("/upload/") - 1;
    if (strlen(filename) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename required");
        return ESP_FAIL;
    }

    size_t auth_len;
#ifdef CONFIG_UPLOAD_AUTH_TOKEN_PRESENT
    auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    char auth[64];
    if (auth_len == 0 || auth_len >= sizeof(auth) ||
        httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) != ESP_OK) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    int rc = mbedtls_sha256_starts(&ctx, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_starts failed: %d", rc);
        mbedtls_sha256_free(&ctx);
        if (f) {
            fclose(f);
            unlink(filepath);
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "hash fail");
        return ESP_FAIL;
    }
    rc = mbedtls_sha256_update(&ctx, (const unsigned char *)auth, strlen(auth));
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_update failed: %d", rc);
        mbedtls_sha256_free(&ctx);
        if (f) {
            fclose(f);
            unlink(filepath);
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "hash fail");
        return ESP_FAIL;
    }
    rc = mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    if (rc != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256_finish failed: %d", rc);
        if (f) {
            fclose(f);
            unlink(filepath);
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "hash fail");
        return ESP_FAIL;
    }

    if (memcmp(hash, upload_token_hash, sizeof(hash)) != 0) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, "Unauthorized", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
#endif

    auth_len = httpd_req_get_hdr_value_len(req, "Content-Type");
    char ctype[32];
    if (auth_len == 0 || auth_len >= sizeof(ctype) ||
        httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype)) != ESP_OK ||
        strcasecmp(ctype, "image/bmp") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Type must be image/bmp");
        return ESP_FAIL;
    }

    if (req->content_len > CONFIG_UPLOAD_MAX_BYTES) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_send(req, "Payload too large", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    char clean_name[64];
    sanitize_filename(clean_name, filename, sizeof(clean_name));
    if (strlen(clean_name) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }

    size_t n = strlen(clean_name);
    if (n < 4 || strcasecmp(&clean_name[n-4], ".bmp") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Only .bmp allowed");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(MOUNT_POINT "/upload", &st) != 0) {
        if (mkdir(MOUNT_POINT "/upload", 0775) != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "mkdir failed: %d", errno);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "mkdir fail");
            return ESP_FAIL;
        }
    }

    int len = snprintf(filepath, sizeof(filepath), "%s/upload/%s", MOUNT_POINT, clean_name);
    if (len < 0 || len >= sizeof(filepath)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "path too long");
        return ESP_FAIL;
    }

    f = fopen(filepath, "w");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open fail");
        return ESP_FAIL;
    }

    char buf[1024];
    size_t remaining = req->content_len;
    while (remaining > 0) {
        ssize_t received = httpd_req_recv(req, buf, remaining > sizeof(buf) ? sizeof(buf) : remaining);
        if (received < 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(f);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv fail");
            return ESP_FAIL;
        }
        if (fwrite(buf, 1, (size_t)received, f) != (size_t)received) {
            fclose(f);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write fail");
            return ESP_FAIL;
        }
        long pos = ftell(f);
        if (pos < 0 || (size_t)pos > CONFIG_UPLOAD_MAX_BYTES) {
            fclose(f);
            unlink(filepath);
            httpd_resp_set_status(req, "413 Payload Too Large");
            httpd_resp_send(req, "Payload too large", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        remaining -= (size_t)received;
    }
    fclose(f);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t start_file_server(void)
{
    if (s_server) {
        return ESP_OK;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_server = NULL;
        return ESP_FAIL;
    }
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &root);

    httpd_uri_t upload = {
        .uri = "/upload/*",
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &upload);
    ESP_LOGI(TAG, "server started");
    return ESP_OK;
}

void stop_file_server(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}

