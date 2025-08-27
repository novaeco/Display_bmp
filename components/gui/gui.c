#include "gui.h"
#include "lvgl.h"
#include "gt911.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"

static esp_lcd_panel_handle_t s_panel;
static lv_draw_buf_t *s_draw_buf;
static uint8_t *s_buf1;
static lv_display_t *s_disp;
static TaskHandle_t s_lvgl_task;
static esp_timer_handle_t s_lvgl_tick_timer;

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    touch_gt911_point_t p = touch_gt911_read_point(1);
    if (p.cnt > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = p.x[0];
        data->point.y = p.y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static const char *TAG = "lvgl";

static void lvgl_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void lvgl_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        UBaseType_t stack_words = uxTaskGetStackHighWaterMark(NULL);
        if (stack_words < 512) {
            ESP_LOGW(TAG, "Low stack: %u words remaining", stack_words);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gui_init(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
    lv_init();

    s_draw_buf = lv_draw_buf_create(g_display.width, 10, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    s_buf1 = s_draw_buf->data;

    s_disp = lv_display_create(g_display.width, g_display.height);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp, s_buf1, NULL, g_display.width * 10 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_read_cb(indev, lvgl_touch_read);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_timer_cb,
        .name = "lvgl_tick"
    };
    esp_timer_create(&lvgl_tick_timer_args, &s_lvgl_tick_timer);
    esp_timer_start_periodic(s_lvgl_tick_timer, 1000);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 5, &s_lvgl_task, 1);
}

void gui_deinit(void)
{
    if (s_lvgl_task) {
        vTaskDelete(s_lvgl_task);
        s_lvgl_task = NULL;
    }
    if (s_lvgl_tick_timer) {
        esp_timer_stop(s_lvgl_tick_timer);
        esp_timer_delete(s_lvgl_tick_timer);
        s_lvgl_tick_timer = NULL;
    }
    if (s_disp) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
    if (s_draw_buf) {
        lv_draw_buf_destroy(s_draw_buf);
        s_draw_buf = NULL;
        s_buf1 = NULL;
    }
    lv_deinit();
}
