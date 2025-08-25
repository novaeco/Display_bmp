#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#ifdef CONFIG_WIFI_PROV_TRANSPORT_BLE
#include "wifi_provisioning/scheme_ble.h"
#else
#include "wifi_provisioning/scheme_softap.h"
#endif
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include <string.h>
#include <stdio.h>

#ifndef CONFIG_WIFI_PROV_TIMEOUT_MS
#define CONFIG_WIFI_PROV_TIMEOUT_MS 60000
#endif

#define WIFI_MANAGER_MAX_RETRY 5
#define WIFI_MANAGER_BASE_BACKOFF_MS 1000
#define WIFI_MANAGER_TASK_STACK 4096
#define WIFI_MANAGER_TASK_PRIO 5

static const char *TAG = "wifi_mgr";

typedef struct {
    esp_event_base_t base;
    int32_t id;
} wifi_internal_event_t;

static QueueHandle_t s_event_queue;
static wifi_event_cb_t s_event_cb;
static TaskHandle_t s_task_handle;
static int s_retry_num;
static bool s_fail_notified;

static void get_service_name(char *name, size_t max_len)
{
    uint8_t eth_mac[6];
    esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
    snprintf(name, max_len, "PROV_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
}

static esp_err_t do_provisioning(void)
{
    wifi_prov_mgr_config_t prov_cfg = {
#ifdef CONFIG_WIFI_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
#else
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
#endif
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    if (!provisioned) {
        char service_name[12];
        get_service_name(service_name, sizeof(service_name));
        ESP_LOGI(TAG, "Starting provisioning service: %s", service_name);
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0, NULL,
                                                         service_name, NULL));
        int64_t start_time = esp_timer_get_time();
        while (!provisioned &&
               (esp_timer_get_time() - start_time) <
                   (CONFIG_WIFI_PROV_TIMEOUT_MS * 1000LL)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            wifi_prov_mgr_is_provisioned(&provisioned);
        }
        wifi_prov_mgr_stop_provisioning();
        if (!provisioned) {
            ESP_LOGE(TAG, "Provisioning timeout");
            wifi_prov_mgr_deinit();
            return ESP_ERR_TIMEOUT;
        }
        ESP_LOGI(TAG, "Provisioning successful");
    }
    wifi_prov_mgr_deinit();
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data)
{
    wifi_internal_event_t evt = {.base = event_base, .id = event_id};
    if (s_event_queue) {
        xQueueSend(s_event_queue, &evt, 0);
    }
}

static void wifi_manager_task(void *pv)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (do_provisioning() != ESP_OK) {
        if (s_event_cb) {
            s_event_cb(WIFI_MANAGER_EVENT_FAIL);
        }
        s_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    s_event_queue = xQueueCreate(10, sizeof(wifi_internal_event_t));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

    wifi_internal_event_t evt;
    for (;;) {
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY)) {
            if (evt.base == IP_EVENT && evt.id == IP_EVENT_STA_GOT_IP) {
                s_retry_num = 0;
                s_fail_notified = false;
                if (s_event_cb) {
                    s_event_cb(WIFI_MANAGER_EVENT_CONNECTED);
                }
            } else if (evt.base == WIFI_EVENT && evt.id == WIFI_EVENT_STA_DISCONNECTED) {
                if (!s_fail_notified) {
                    if (s_retry_num < WIFI_MANAGER_MAX_RETRY) {
                        int delay_ms = WIFI_MANAGER_BASE_BACKOFF_MS << s_retry_num;
                        vTaskDelay(pdMS_TO_TICKS(delay_ms));
                        s_retry_num++;
                        esp_wifi_connect();
                        if (s_event_cb) {
                            s_event_cb(WIFI_MANAGER_EVENT_DISCONNECTED);
                        }
                    } else {
                        s_fail_notified = true;
                        if (s_event_cb) {
                            s_event_cb(WIFI_MANAGER_EVENT_FAIL);
                        }
                    }
                }
            }
        }
    }
}

void wifi_manager_register_callback(wifi_event_cb_t cb)
{
    s_event_cb = cb;
}

esp_err_t wifi_manager_start(void)
{
    if (s_task_handle) {
        return ESP_OK;
    }
    s_retry_num = 0;
    s_fail_notified = false;
    if (xTaskCreate(wifi_manager_task, "wifi_mgr", WIFI_MANAGER_TASK_STACK, NULL,
                    WIFI_MANAGER_TASK_PRIO, &s_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
