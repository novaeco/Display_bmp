#include "wifi.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#ifdef CONFIG_WIFI_PROV_TRANSPORT_BLE
#include "wifi_provisioning/scheme_ble.h"
#else
#include "wifi_provisioning/scheme_softap.h"
#endif
#include "esp_system.h"
#include <stdio.h>

static const char *TAG = "wifi";

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_GOT_IP_BIT    BIT1
#define WIFI_FAIL_BIT      BIT2

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_GOT_IP_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

static bool s_wifi_initialized = false;

static void get_service_name(char *name, size_t max_len)
{
    uint8_t eth_mac[6];
    esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
    snprintf(name, max_len, "PROV_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
}

esp_err_t wifi_init_sta(uint32_t timeout_ms)
{
    if (!s_wifi_initialized) {
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

        s_wifi_initialized = true;
    }

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
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0,
                                                        NULL, service_name, NULL));
        while (!provisioned) {
            vTaskDelay(pdMS_TO_TICKS(500));
            wifi_prov_mgr_is_provisioned(&provisioned);
        }
        ESP_LOGI(TAG, "Provisioning successful");
        wifi_prov_mgr_stop_provisioning();
    }

    wifi_prov_mgr_deinit();

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        return ret;
    }

    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    vEventGroupDelete(s_wifi_event_group);

    if (bits & (WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT)) {
        ESP_LOGI(TAG, "WiFi STA connected");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi STA connection failed");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi STA connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}
