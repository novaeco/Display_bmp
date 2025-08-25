#include "rs485_display.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "ui_navigation.h"
#include "touch/gt911.h"
#include "config.h"
#include "esp_log.h"

extern QueueHandle_t s_touch_queue;
extern const display_geometry_t g_display;

#define RS485_DISPLAY_TAG "RS485_DISP"
#define RS485_UART UART_NUM_1
#define RS485_TXD GPIO_NUM_15
#define RS485_RXD GPIO_NUM_16

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

static void rs485_display_task(void *arg)
{
    uint8_t buf[8];
    while (1) {
        int len = uart_read_bytes(RS485_UART, buf, sizeof(buf), portMAX_DELAY);
        if (len >= 4) {
            if (memcmp(buf, "NEXT", 4) == 0) {
                send_nav_touch(true);
            } else if (memcmp(buf, "PREV", 4) == 0) {
                send_nav_touch(false);
            }
        }
    }
}

esp_err_t rs485_display_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(RS485_UART, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(RS485_DISPLAY_TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_set_pin(RS485_UART, RS485_TXD, RS485_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(RS485_DISPLAY_TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_driver_install(RS485_UART, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(RS485_DISPLAY_TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_set_mode(RS485_UART, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) {
        ESP_LOGE(RS485_DISPLAY_TAG, "uart_set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (xTaskCreate(rs485_display_task, "rs485_display_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(RS485_DISPLAY_TAG, "Failed to create RS485 task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

