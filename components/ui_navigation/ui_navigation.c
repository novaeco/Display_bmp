#include "ui_navigation.h"
#include "gui_paint.h"
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
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include "esp_log.h"

extern UBYTE *BlackImage;
extern display_geometry_t g_display;
extern char g_base_path[];

static const char *s_folder_choice = NULL;
static void folder_label_cb(lv_event_t *e)
{
    s_folder_choice = (const char *)lv_event_get_user_data(e);
}

static bool is_folder_excluded(const char *name)
{
    const char *list = UI_NAV_EXCLUDED_DIRS;
    const char *p = list;
    while (*p) {
        while (*p == ' ' || *p == ',') {
            p++;
        }
        const char *start = p;
        while (*p && *p != ',') {
            p++;
        }
        size_t len = p - start;
        if (len == 0) {
            continue;
        }
        if (strncasecmp(name, start, len) == 0 && name[len] == '\0') {
            return true;
        }
    }
    return false;
}

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
            if (point_data.cnt == 2) {
                Paint_Clear(WHITE);
                Paint_DrawString_EN(text_x, text_y1, "Double touche detectee", &Font24, BLACK, WHITE);
                Paint_DrawString_EN(text_x, text_y2, "Reessayer", &Font24, BLACK, WHITE);
                waveshare_rgb_lcd_display(BlackImage);
                vTaskDelay(pdMS_TO_TICKS(2000));
                Paint_Clear(WHITE);
                Paint_DrawString_EN(text_x, text_y1, "Orientation :", &Font24, BLACK, WHITE);
                Paint_DrawString_EN(text_x, text_y2, "Choisissez :", &Font24, BLACK, WHITE);
                draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                   "Paysage", BTN_LABEL_L_OFFSET_X, WHITE);
                draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                   "Portrait", BTN_LABEL_R_OFFSET_X, WHITE);
                waveshare_rgb_lcd_display(BlackImage);
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
    UWORD text_x = g_display.width / TEXT_X_DIVISOR;
    UWORD text_y1 = g_display.height / TEXT_Y1_DIVISOR;
    UWORD text_y2 = text_y1 + TEXT_LINE_SPACING;

    s_folder_choice = NULL;

    typedef struct {
        char **names;
        lv_obj_t **labels;
        size_t count;
        size_t cap;
    } folder_list_t;

    folder_list_t fl = {0};
    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE("NAV", "opendir %s failed", MOUNT_POINT);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (is_folder_excluded(entry->d_name)) {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, entry->d_name);
        DIR *sub = opendir(path);
        if (!sub) {
            continue;
        }
        bool has_bmp = false;
        struct dirent *e2;
        while ((e2 = readdir(sub)) != NULL) {
            if (e2->d_type != DT_REG) {
                continue;
            }
            const char *ext = strrchr(e2->d_name, '.');
            if (ext && strcasecmp(ext, ".bmp") == 0) {
                has_bmp = true;
                break;
            }
        }
        closedir(sub);
        if (!has_bmp) {
            continue;
        }

        if (fl.count == fl.cap) {
            size_t newcap = fl.cap ? fl.cap * 2 : 4;
            char **old_names = fl.names;
            lv_obj_t **old_labels = fl.labels;
            char **newnames = realloc(fl.names, newcap * sizeof(char *));
            lv_obj_t **newlabels = realloc(fl.labels, newcap * sizeof(lv_obj_t *));
            if (!newnames || !newlabels) {
                ESP_LOGE("NAV", "realloc failed");
                if (newnames && newnames != old_names) {
                    free(newnames);
                }
                if (newlabels && newlabels != old_labels) {
                    free(newlabels);
                }
                fl.names = old_names;
                fl.labels = old_labels;
                closedir(dir);
                return NULL;
            }
            fl.names = newnames;
            fl.labels = newlabels;
            fl.cap = newcap;
        }
        fl.names[fl.count++] = strdup(entry->d_name);
    }
    closedir(dir);

    if (fl.count == 0) {
        free(fl.names);
        free(fl.labels);
        return NULL;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *lbl_ok = lv_label_create(scr);
    lv_label_set_text(lbl_ok, "Carte SD OK !");
    lv_obj_set_pos(lbl_ok, text_x, text_y1);
    lv_obj_t *lbl_choose = lv_label_create(scr);
    lv_label_set_text(lbl_choose, "Choisissez un dossier :");
    lv_obj_set_pos(lbl_choose, text_x, text_y2);

    UWORD list_y = text_y2 + TEXT_LINE_SPACING;
    for (size_t i = 0; i < fl.count; ++i) {
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, fl.names[i]);
        lv_obj_set_pos(lbl, text_x, list_y + i * TEXT_LINE_SPACING);
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lbl, folder_label_cb, LV_EVENT_CLICKED, fl.names[i]);
        fl.labels[i] = lbl;
    }

    lv_scr_load(scr);
    while (s_folder_choice == NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const char *selected_dir = s_folder_choice;

    lv_scr_load(NULL);           // unload selection screen to avoid it remaining active
    lv_obj_del(scr);             // delete screen object to prevent RAM accumulation

    for (size_t i = 0; i < fl.count; ++i) {
        if (fl.names[i] != selected_dir) {
            free(fl.names[i]);
        }
    }
    free(fl.names);
    free(fl.labels);
    s_folder_choice = NULL;

    Paint_Clear(WHITE);
    Paint_DrawString_EN(text_x, text_y1, "Dossier choisi :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, selected_dir, &Font24, BLACK, WHITE);
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
    lv_scr_load(NULL);           // unload selection screen to avoid it remaining active
    lv_obj_del(scr);             // delete screen object to prevent RAM accumulation
    return (image_source_t)s_src_choice;
}

static void add_btn_img_or_label(lv_obj_t *btn, const char *img_path, const char *fallback)
{
    FILE *f = fopen(img_path, "rb");
    if (f) {
        fclose(f);
        lv_obj_t *img = lv_img_create(btn);
        lv_img_set_src(img, img_path);
        lv_obj_center(img);
    } else {
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, fallback);
        lv_obj_center(lbl);
    }
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
    add_btn_img_or_label(btn_left, MOUNT_POINT "/pic/pic/arrow_left.bmp", "<");

    lv_obj_t *btn_right = lv_btn_create(scr);
    lv_obj_set_size(btn_right, ARROW_WIDTH, ARROW_HEIGHT);
    lv_obj_set_pos(btn_right, g_display.width - g_display.margin_right - ARROW_WIDTH,
                   (g_display.height - ARROW_HEIGHT)/2);
    lv_obj_add_event_cb(btn_right, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    add_btn_img_or_label(btn_right, MOUNT_POINT "/pic/pic/arrow_right.bmp", ">");

    lv_obj_t *btn_rotate = lv_btn_create(scr);
    lv_obj_set_size(btn_rotate, 100, 40);
    lv_obj_set_pos(btn_rotate, (g_display.width - 100)/2, g_display.margin_top);
    lv_obj_add_event_cb(btn_rotate, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)2);
    add_btn_img_or_label(btn_rotate, MOUNT_POINT "/pic/pic/wifi.bmp", "Rotation");

    lv_obj_t *btn_home = lv_btn_create(scr);
    lv_obj_set_size(btn_home, 100, 40);
    lv_obj_set_pos(btn_home, g_display.margin_left,
                   g_display.height - g_display.margin_bottom - 40);
    lv_obj_add_event_cb(btn_home, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)3);
    add_btn_img_or_label(btn_home, MOUNT_POINT "/pic/pic/home.bmp", "Home");

    lv_obj_t *btn_exit = lv_btn_create(scr);
    lv_obj_set_size(btn_exit, 100, 40);
    lv_obj_set_pos(btn_exit,
                   g_display.width - g_display.margin_right - 100,
                   g_display.height - g_display.margin_bottom - 40);
    lv_obj_add_event_cb(btn_exit, nav_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)4);
    add_btn_img_or_label(btn_exit, MOUNT_POINT "/pic/pic/bluetooth.bmp", "Exit");
}

nav_action_t handle_touch_navigation(int8_t *idx)
{
    int8_t dir;
    if (s_nav_queue && xQueueReceive(s_nav_queue, &dir, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (dir == 2) {
            if (bmp_list.size > 0) {
                draw_filename_bar(bmp_list.items[*idx]);
            }
            return NAV_ROTATE;
        }
        if (dir == 3) {
            return NAV_HOME;
        }
        if (dir == 4) {
            return NAV_EXIT;
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

    if (s_fname_bar) {
        lv_obj_del(s_fname_bar);
    }
    s_fname_bar = NULL;
    s_fname_label = NULL;
}

