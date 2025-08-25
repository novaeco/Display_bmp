#include "can_display.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "ui_navigation.h"
#include "touch/gt911.h"
#include "config.h"
#include "esp_log.h"

extern QueueHandle_t s_touch_queue;
extern display_geometry_t g_display;

#define CAN_DISPLAY_TAG "CAN_DISP"
#define CAN_TX_PIN GPIO_NUM_20
#define CAN_RX_PIN GPIO_NUM_19

static void send_nav_touch(bool next)
{
    touch_gt911_point_t ev = { .cnt = 1 };
    if (next) {
        ev.x[0] = g_display.width - NAV_MARGIN - ARROW_WIDTH / 2;
        ev.y[0] = NAV_MARGIN + ARROW_HEIGHT / 2;
    } else {
        ev.x[0] = NAV_MARGIN + ARROW_WIDTH / 2;
        ev.y[0] = NAV_MARGIN + ARROW_HEIGHT / 2;
    }
    xQueueSend(s_touch_queue, &ev, portMAX_DELAY);
}

static void can_display_task(void *arg)
{
    twai_message_t msg;
    while (1) {
        if (twai_receive(&msg, portMAX_DELAY) == ESP_OK) {
            if (msg.data_length_code >= 4) {
                if (memcmp(msg.data, "NEXT", 4) == 0) {
                    send_nav_touch(true);
                } else if (memcmp(msg.data, "PREV", 4) == 0) {
                    send_nav_touch(false);
                }
            }
        }
    }
}

esp_err_t can_display_init(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(CAN_DISPLAY_TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(CAN_DISPLAY_TAG, "Failed to start TWAI: %s", esp_err_to_name(ret));
        return ret;
    }

    if (xTaskCreate(can_display_task, "can_display_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(CAN_DISPLAY_TAG, "Failed to create CAN task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

