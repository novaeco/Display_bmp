#include "gui.h"
#include "lvgl.h"
#include "gt911.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static esp_lcd_panel_handle_t s_panel;
static lv_draw_buf_t *s_draw_buf;
static uint8_t *s_buf1;

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

static void lvgl_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

void gui_init(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
    lv_init();

    s_draw_buf = lv_draw_buf_create(LCD_H_RES, 10, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    s_buf1 = s_draw_buf->data;

    lv_display_t *disp = lv_display_create(g_display.width, g_display.height);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, s_buf1, NULL, LCD_H_RES * 10, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_read_cb(indev, lvgl_touch_read);

    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
}
