#include "ui_navigation.h"
#include "gui_paint.h"
#include "gui_bmp.h"
#include "battery.h"
#include "file_manager.h"
#include "touch_task.h"
#include "config.h"
#include "rgb_lcd_port.h"
#include "sd.h"
#include "gt911.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "freertos/queue.h"
#include <string.h>
#include "esp_log.h"

extern UBYTE *BlackImage;
extern display_geometry_t g_display;
extern char g_base_path[];

static inline void orient_coords(uint16_t *x, uint16_t *y)
{
    if (g_is_portrait) {
        uint16_t tx = *x;
        *x = *y;
        *y = g_display.width - tx;
    }
}

static void draw_folder_button(UWORD x0, UWORD y0, UWORD x1, UWORD y1,
                               const char *label, UWORD offset_x, UWORD bg_color)
{
    Paint_DrawRectangle(x0, y0, x1, y1, bg_color, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(x0, y0, x1, y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(x1, y0, x1, y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(x1, y1, x0, y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(x0, y1, x0, y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawString_EN(x0 + offset_x, (y0 + y1) / 2 - BTN_LABEL_OFFSET_Y,
                       label, &Font24, BLACK, bg_color);
}
void draw_orientation_menu(void)
{
    touch_gt911_point_t point_data;

    UWORD text_x = g_display.width / TEXT_X_DIVISOR;
    UWORD text_y1 = g_display.height / TEXT_Y1_DIVISOR;
    UWORD text_y2 = text_y1 + TEXT_LINE_SPACING;

    Paint_DrawString_EN(text_x, text_y1, "Orientation :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, "Choisissez :", &Font24, BLACK, WHITE);

    UWORD btnL_x0 = g_display.margin_left;
    UWORD btnL_y0 = (g_display.height - BTN_HEIGHT) / 2;
    UWORD btnL_x1 = btnL_x0 + BTN_WIDTH;
    UWORD btnL_y1 = btnL_y0 + BTN_HEIGHT;

    UWORD btnR_x1 = g_display.width - g_display.margin_right;
    UWORD btnR_x0 = btnR_x1 - BTN_WIDTH;
    UWORD btnR_y0 = btnL_y0;
    UWORD btnR_y1 = btnL_y1;

    draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                       "Paysage", BTN_LABEL_L_OFFSET_X, WHITE);
    draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                       "Portrait", BTN_LABEL_R_OFFSET_X, WHITE);

    waveshare_rgb_lcd_display(BlackImage);

    while (1) {
        if (xQueueReceive(s_touch_queue, &point_data, portMAX_DELAY) == pdTRUE) {
            if (point_data.cnt == 0) {
                continue;
            }
            uint16_t tx = point_data.x[0];
            uint16_t ty = point_data.y[0];
            orient_coords(&tx, &ty);
            if (tx >= btnL_x0 && tx <= btnL_x1 && ty >= btnL_y0 && ty <= btnL_y1) {
                display_set_orientation(false);
            } else if (tx >= btnR_x0 && tx <= btnR_x1 && ty >= btnR_y0 && ty <= btnR_y1) {
                display_set_orientation(true);
            } else {
                continue;
            }
            Paint_SetRotate(g_is_portrait ? ROTATE_90 : ROTATE_0);
            Paint_Clear(WHITE);
            waveshare_rgb_lcd_display(BlackImage);
            display_save_orientation();
            return;
        }
    }
}

const char *draw_folder_selection(void)
{
    touch_gt911_point_t point_data;

    UWORD text_x = g_display.width / TEXT_X_DIVISOR;
    UWORD text_y1 = g_display.height / TEXT_Y1_DIVISOR;
    UWORD text_y2 = text_y1 + TEXT_LINE_SPACING;

    Paint_DrawString_EN(text_x, text_y1, "Carte SD OK !", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, "Choisissez un dossier :", &Font24, BLACK, WHITE);

    UWORD btnL_x0 = g_display.margin_left;
    UWORD btnL_y0 = (g_display.height - BTN_HEIGHT) / 2;
    UWORD btnL_x1 = btnL_x0 + BTN_WIDTH;
    UWORD btnL_y1 = btnL_y0 + BTN_HEIGHT;

    UWORD btnR_x1 = g_display.width - g_display.margin_right;
    UWORD btnR_x0 = btnR_x1 - BTN_WIDTH;
    UWORD btnR_y0 = btnL_y0;
    UWORD btnR_y1 = btnL_y1;

    draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                       "Reptiles", BTN_LABEL_L_OFFSET_X, WHITE);
    draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                       "Presentation", BTN_LABEL_R_OFFSET_X, WHITE);

    waveshare_rgb_lcd_display(BlackImage);

    const char *selected_dir = NULL;
    enum { HIGHLIGHT_NONE, HIGHLIGHT_LEFT, HIGHLIGHT_RIGHT } highlight = HIGHLIGHT_NONE;
    while (selected_dir == NULL) {
        if (xQueueReceive(s_touch_queue, &point_data, portMAX_DELAY) == pdTRUE) {
            if (point_data.cnt == 2) {
                return NULL;
            }
            if (point_data.cnt == 1) {
                uint16_t tx = point_data.x[0];
                uint16_t ty = point_data.y[0];
                orient_coords(&tx, &ty);
                if (tx >= btnL_x0 && tx <= btnL_x1 && ty >= btnL_y0 && ty <= btnL_y1) {
                    if (highlight != HIGHLIGHT_LEFT) {
                        if (highlight == HIGHLIGHT_RIGHT) {
                            draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                               "Presentation", BTN_LABEL_R_OFFSET_X, WHITE);
                        }
                        draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                           "Reptiles", BTN_LABEL_L_OFFSET_X, GRAY);
                        waveshare_rgb_lcd_display(BlackImage);
                        highlight = HIGHLIGHT_LEFT;
                    }
                } else if (tx >= btnR_x0 && tx <= btnR_x1 && ty >= btnR_y0 && ty <= btnR_y1) {
                    if (highlight != HIGHLIGHT_RIGHT) {
                        if (highlight == HIGHLIGHT_LEFT) {
                            draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                               "Reptiles", BTN_LABEL_L_OFFSET_X, WHITE);
                        }
                        draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                           "Presentation", BTN_LABEL_R_OFFSET_X, GRAY);
                        waveshare_rgb_lcd_display(BlackImage);
                        highlight = HIGHLIGHT_RIGHT;
                    }
                } else if (highlight != HIGHLIGHT_NONE) {
                    if (highlight == HIGHLIGHT_LEFT) {
                        draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                           "Reptiles", BTN_LABEL_L_OFFSET_X, WHITE);
                    } else {
                        draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                           "Presentation", BTN_LABEL_R_OFFSET_X, WHITE);
                    }
                    waveshare_rgb_lcd_display(BlackImage);
                    highlight = HIGHLIGHT_NONE;
                }
            } else if (point_data.cnt == 0 && highlight != HIGHLIGHT_NONE) {
                if (highlight == HIGHLIGHT_LEFT) {
                    draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                       "Reptiles", BTN_LABEL_L_OFFSET_X, WHITE);
                    selected_dir = "Reptiles";
                } else {
                    draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                       "Presentation", BTN_LABEL_R_OFFSET_X, WHITE);
                    selected_dir = "Presentation";
                }
                waveshare_rgb_lcd_display(BlackImage);
                highlight = HIGHLIGHT_NONE;
            }
        }
    }

    Paint_Clear(WHITE);
    Paint_DrawString_EN(text_x, text_y1, "Dossier choisi :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, (char *)selected_dir, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2 + TEXT_LINE_SPACING, "Touchez la fleche pour demarrer.", &Font24, BLACK, WHITE);
    waveshare_rgb_lcd_display(BlackImage);
    return selected_dir;
}

static QueueHandle_t s_nav_queue;
static volatile int s_src_choice = -1;
static lv_obj_t *s_fname_bar = NULL;
static lv_obj_t *s_fname_label = NULL;

static void source_btn_cb(lv_event_t *e)
{
    s_src_choice = (int)lv_event_get_user_data(e);
}

static void nav_btn_cb(lv_event_t *e)
{
    int8_t dir = (int8_t)(intptr_t)lv_event_get_user_data(e);
    if (s_nav_queue) {
        xQueueSend(s_nav_queue, &dir, 0);
    }
}

image_source_t draw_source_selection(void)
{
    s_src_choice = -1;
    lv_obj_t *scr = lv_obj_create(NULL);

    UWORD btnL_x0 = g_display.margin_left;
    UWORD btnL_y0 = (g_display.height - BTN_HEIGHT) / 2;
    UWORD btnR_x1 = g_display.width - g_display.margin_right;
    UWORD btnR_x0 = btnR_x1 - BTN_WIDTH;
    UWORD btnR_y0 = btnL_y0;
    UWORD btnN_x0 = (g_display.width - BTN_WIDTH) / 2;
    UWORD btnN_y0 = btnL_y0 + BTN_HEIGHT + NAV_MARGIN;

    lv_obj_t *btn_local = lv_btn_create(scr);
    lv_obj_set_size(btn_local, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(btn_local, btnL_x0, btnL_y0);
    lv_obj_add_event_cb(btn_local, source_btn_cb, LV_EVENT_CLICKED, (void*)IMAGE_SOURCE_LOCAL);
    lv_obj_t *lbl_local = lv_label_create(btn_local);
    lv_label_set_text(lbl_local, "Locales");

    lv_obj_t *btn_remote = lv_btn_create(scr);
    lv_obj_set_size(btn_remote, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(btn_remote, btnR_x0, btnR_y0);
    lv_obj_add_event_cb(btn_remote, source_btn_cb, LV_EVENT_CLICKED, (void*)IMAGE_SOURCE_REMOTE);
    lv_obj_t *lbl_remote = lv_label_create(btn_remote);
    lv_label_set_text(lbl_remote, "Distantes");

    lv_obj_t *btn_net = lv_btn_create(scr);
    lv_obj_set_size(btn_net, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(btn_net, btnN_x0, btnN_y0);
    lv_obj_add_event_cb(btn_net, source_btn_cb, LV_EVENT_CLICKED, (void*)IMAGE_SOURCE_NETWORK);
    lv_obj_t *lbl_net = lv_label_create(btn_net);
    lv_label_set_text(lbl_net, "Source reseau");

    lv_scr_load(scr);
    while (s_src_choice == -1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return (image_source_t)s_src_choice;
}

void draw_navigation_arrows(void)
{
    if (!s_nav_queue) {
        s_nav_queue = xQueueCreate(5, sizeof(int8_t));
        if (!s_nav_queue) {
            ESP_LOGE("NAV", "xQueueCreate failed");
            return;
        }
    } else {
        xQueueReset(s_nav_queue);
    }
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *btn_left = lv_btn_create(scr);
    lv_obj_set_size(btn_left, ARROW_WIDTH, ARROW_HEIGHT);
    lv_obj_set_pos(btn_left, g_display.margin_left, (g_display.height - ARROW_HEIGHT)/2);
    lv_obj_add_event_cb(btn_left, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_t *lbl_left = lv_label_create(btn_left);
    lv_label_set_text(lbl_left, "<");

    lv_obj_t *btn_right = lv_btn_create(scr);
    lv_obj_set_size(btn_right, ARROW_WIDTH, ARROW_HEIGHT);
    lv_obj_set_pos(btn_right, g_display.width - g_display.margin_right - ARROW_WIDTH,
                   (g_display.height - ARROW_HEIGHT)/2);
    lv_obj_add_event_cb(btn_right, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_t *lbl_right = lv_label_create(btn_right);
    lv_label_set_text(lbl_right, ">");

    lv_obj_t *btn_rotate = lv_btn_create(scr);
    lv_obj_set_size(btn_rotate, 100, 40);
    lv_obj_set_pos(btn_rotate, (g_display.width - 100)/2, g_display.margin_top);
    lv_obj_add_event_cb(btn_rotate, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)2);
    lv_obj_t *lbl_rotate = lv_label_create(btn_rotate);
    lv_label_set_text(lbl_rotate, "Rotation");
}

nav_action_t handle_touch_navigation(int8_t *idx, uint16_t *prev_x, uint16_t *prev_y)
{
    (void)prev_x;
    (void)prev_y;
    int8_t dir;
    if (s_nav_queue && xQueueReceive(s_nav_queue, &dir, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (dir == 2) {
            if (bmp_list.size > 0) {
                draw_filename_bar(bmp_list.items[*idx]);
            }
            return NAV_ROTATE;
        }
        if (bmp_list.size == 0) {
            return NAV_NONE;
        }
        *idx += dir;
        if (*idx >= (int8_t)bmp_list.size) {
            *idx = 0;
        } else if (*idx < 0) {
            *idx = (int8_t)bmp_list.size - 1;
        }
        draw_filename_bar(bmp_list.items[*idx]);
        return NAV_SCROLL;
    }
    return NAV_NONE;
}

void draw_filename_bar(const char *path)
{
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;

    const lv_font_t *font = LV_FONT_DEFAULT;
    lv_coord_t bar_h = font->line_height + 2 * FILENAME_BAR_PAD;

    if (!s_fname_bar || !lv_obj_is_valid(s_fname_bar)) {
        s_fname_bar = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_color(s_fname_bar, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_fname_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_fname_bar, FILENAME_BAR_PAD, LV_PART_MAIN);
        s_fname_label = lv_label_create(s_fname_bar);
        lv_obj_set_style_text_color(s_fname_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }

    lv_obj_set_size(s_fname_bar, g_display.width, bar_h);
    lv_obj_align(s_fname_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_label_set_text(s_fname_label, fname);
    lv_obj_center(s_fname_label);

    if (g_is_portrait) {
        lv_obj_set_style_transform_angle(s_fname_bar, 900, LV_PART_MAIN);
    } else {
        lv_obj_set_style_transform_angle(s_fname_bar, 0, LV_PART_MAIN);
    }
}

void ui_navigation_deinit(void)
{
    if (s_nav_queue) {
        vQueueDelete(s_nav_queue);
        s_nav_queue = NULL;
    }
}

