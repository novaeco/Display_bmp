#include "touch_task.h"
#include "gt911.h"
#include "esp_log.h"
#include "esp_err.h"
#include "pm.h"

#define TOUCH_QUEUE_LENGTH       10
#define TOUCH_TASK_STACK         4096
#define TOUCH_TASK_PRIORITY      5

TaskHandle_t s_touch_task_handle;
QueueHandle_t s_touch_queue;
esp_lcd_touch_handle_t s_touch_handle;

static const char *TAG = "TOUCH_TASK";

static void IRAM_ATTR touch_int_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t hp_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_touch_task_handle, &hp_task_woken);
    portYIELD_FROM_ISR(hp_task_woken);
}

static void touch_task(void *arg)
{
    touch_gt911_point_t data;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        data = touch_gt911_read_point(5);
        if (data.cnt > 0) {
            pm_update_activity();
        }
        xQueueSend(s_touch_queue, &data, portMAX_DELAY);
    }
}

bool touch_task_init(void)
{
    s_touch_handle = touch_gt911_init();
    if (s_touch_handle == NULL) {
        ESP_LOGE(TAG, "Échec d'initialisation du contrôleur tactile");
        return false;
    }
    s_touch_queue = xQueueCreate(TOUCH_QUEUE_LENGTH, sizeof(touch_gt911_point_t));
    if (s_touch_queue == NULL) {
        ESP_LOGE(TAG, "Échec de création de la file tactile");
        return false;
    }
    if (xTaskCreate(touch_task, "touch_task", TOUCH_TASK_STACK, NULL, TOUCH_TASK_PRIORITY, &s_touch_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Échec de création de la tâche tactile");
        vQueueDelete(s_touch_queue);
        s_touch_queue = NULL;
        return false;
    }
    esp_err_t err = esp_lcd_touch_register_interrupt_callback(s_touch_handle, touch_int_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement du callback tactile échoué : %s", esp_err_to_name(err));
        vTaskDelete(s_touch_task_handle);
        s_touch_task_handle = NULL;
        vQueueDelete(s_touch_queue);
        s_touch_queue = NULL;
        return false;
    }
    return true;
}

void touch_task_deinit(void)
{
    if (s_touch_task_handle) {
        vTaskDelete(s_touch_task_handle);
        s_touch_task_handle = NULL;
    }
    if (s_touch_queue) {
        vQueueDelete(s_touch_queue);
        s_touch_queue = NULL;
    }
    if (s_touch_handle) {
        esp_lcd_touch_register_interrupt_callback(s_touch_handle, NULL);
        esp_lcd_touch_del(s_touch_handle);
        s_touch_handle = NULL;
    }
}
