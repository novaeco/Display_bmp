#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sd.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static const char *TAG = "http_srv";
static httpd_handle_t s_server = NULL;

static const char upload_html[] =
"<!DOCTYPE html><html><body><h2>Upload BMP</h2>"
"<input id=\"file\" type=\"file\" accept=\".bmp\"><button onclick=\"u()\">Upload</button>"
"<script>function u(){var f=document.getElementById('file').files[0];"
"if(!f){alert('No file');return;}var x=new XMLHttpRequest();"
"x.open('POST','/upload/'+f.name,true);x.send(f);}</script></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, upload_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[128];
    const char *filename = req->uri + sizeof("/upload/") - 1;
    if (strlen(filename) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename required");
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

    snprintf(filepath, sizeof(filepath), "%s/upload/%s", MOUNT_POINT, filename);
    FILE *f = fopen(filepath, "w");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open fail");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, remaining > sizeof(buf) ? sizeof(buf) : remaining);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(f);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv fail");
            return ESP_FAIL;
        }
        if (fwrite(buf, 1, received, f) != received) {
            fclose(f);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write fail");
            return ESP_FAIL;
        }
        remaining -= received;
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

