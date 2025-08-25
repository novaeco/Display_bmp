#include "gui.h"
#include "lvgl.h"
#include "gt911.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static esp_lcd_panel_handle_t s_panel;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_buf1;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
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

    s_buf1 = heap_caps_malloc(LCD_H_RES * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, NULL, LCD_H_RES * 10);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = g_display.width;
    disp_drv.ver_res = g_display.height;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);

    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
}
