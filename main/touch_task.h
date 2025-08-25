#ifndef TOUCH_TASK_H
#define TOUCH_TASK_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "touch.h"

extern TaskHandle_t s_touch_task_handle;
extern QueueHandle_t s_touch_queue;
extern esp_lcd_touch_handle_t s_touch_handle;

bool touch_task_init(void);
void touch_task_deinit(void);

#endif // TOUCH_TASK_H
