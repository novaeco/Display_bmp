#include "ui_navigation.h"
#include "gui_paint.h"
#include "gui_bmp.h"
#include "battery.h"
#include "file_manager.h"
#include "touch_task.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

extern UBYTE *BlackImage;
extern display_geometry_t g_display;
extern char g_base_path[];

static float s_zoom_level = 1.0f;
static int16_t s_scroll_x = 0;
static int16_t s_scroll_y = 0;
static const char *TAG_NAV = "NAV";

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

image_source_t draw_source_selection(void)
{
    touch_gt911_point_t point_data;

    UWORD text_x = g_display.width / TEXT_X_DIVISOR;
    UWORD text_y1 = g_display.height / TEXT_Y1_DIVISOR;
    UWORD text_y2 = text_y1 + TEXT_LINE_SPACING;

    Paint_DrawString_EN(text_x, text_y1, "Source d'image :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, "Choisissez :", &Font24, BLACK, WHITE);

    UWORD btnL_x0 = g_display.margin_left;
    UWORD btnL_y0 = (g_display.height - BTN_HEIGHT) / 2;
    UWORD btnL_x1 = btnL_x0 + BTN_WIDTH;
    UWORD btnL_y1 = btnL_y0 + BTN_HEIGHT;

    UWORD btnR_x1 = g_display.width - g_display.margin_right;
    UWORD btnR_x0 = btnR_x1 - BTN_WIDTH;
    UWORD btnR_y0 = btnL_y0;
    UWORD btnR_y1 = btnL_y1;

    UWORD btnN_x0 = (g_display.width - BTN_WIDTH) / 2;
    UWORD btnN_y0 = btnL_y1 + NAV_MARGIN;
    UWORD btnN_x1 = btnN_x0 + BTN_WIDTH;
    UWORD btnN_y1 = btnN_y0 + BTN_HEIGHT;

    draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                       "Locales", BTN_LABEL_L_OFFSET_X, WHITE);
    draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                       "Distantes", BTN_LABEL_R_OFFSET_X, WHITE);
    draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                       "Source réseau", BTN_LABEL_N_OFFSET_X, WHITE);

    waveshare_rgb_lcd_display(BlackImage);

    enum { HIGHLIGHT_NONE, HIGHLIGHT_LEFT, HIGHLIGHT_RIGHT, HIGHLIGHT_NET } highlight = HIGHLIGHT_NONE;
    while (1) {
        if (xQueueReceive(s_touch_queue, &point_data, portMAX_DELAY) == pdTRUE) {
            if (point_data.cnt == 1) {
                uint16_t tx = point_data.x[0];
                uint16_t ty = point_data.y[0];
                orient_coords(&tx, &ty);
                if (tx >= btnL_x0 && tx <= btnL_x1 && ty >= btnL_y0 && ty <= btnL_y1) {
                    if (highlight != HIGHLIGHT_LEFT) {
                        if (highlight == HIGHLIGHT_RIGHT) {
                            draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                               "Distantes", BTN_LABEL_R_OFFSET_X, WHITE);
                        } else if (highlight == HIGHLIGHT_NET) {
                            draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                                               "Source réseau", BTN_LABEL_N_OFFSET_X, WHITE);
                        }
                        draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                           "Locales", BTN_LABEL_L_OFFSET_X, GRAY);
                        waveshare_rgb_lcd_display(BlackImage);
                        highlight = HIGHLIGHT_LEFT;
                    }
                } else if (tx >= btnR_x0 && tx <= btnR_x1 && ty >= btnR_y0 && ty <= btnR_y1) {
                    if (highlight != HIGHLIGHT_RIGHT) {
                        if (highlight == HIGHLIGHT_LEFT) {
                            draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                               "Locales", BTN_LABEL_L_OFFSET_X, WHITE);
                        } else if (highlight == HIGHLIGHT_NET) {
                            draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                                               "Source réseau", BTN_LABEL_N_OFFSET_X, WHITE);
                        }
                        draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                           "Distantes", BTN_LABEL_R_OFFSET_X, GRAY);
                        waveshare_rgb_lcd_display(BlackImage);
                        highlight = HIGHLIGHT_RIGHT;
                    }
                } else if (tx >= btnN_x0 && tx <= btnN_x1 && ty >= btnN_y0 && ty <= btnN_y1) {
                    if (highlight != HIGHLIGHT_NET) {
                        if (highlight == HIGHLIGHT_LEFT) {
                            draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                               "Locales", BTN_LABEL_L_OFFSET_X, WHITE);
                        } else if (highlight == HIGHLIGHT_RIGHT) {
                            draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                               "Distantes", BTN_LABEL_R_OFFSET_X, WHITE);
                        }
                        draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                                           "Source réseau", BTN_LABEL_N_OFFSET_X, GRAY);
                        waveshare_rgb_lcd_display(BlackImage);
                        highlight = HIGHLIGHT_NET;
                    }
                } else if (highlight != HIGHLIGHT_NONE) {
                    if (highlight == HIGHLIGHT_LEFT) {
                        draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                           "Locales", BTN_LABEL_L_OFFSET_X, WHITE);
                    } else if (highlight == HIGHLIGHT_RIGHT) {
                        draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                           "Distantes", BTN_LABEL_R_OFFSET_X, WHITE);
                    } else {
                        draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                                           "Source réseau", BTN_LABEL_N_OFFSET_X, WHITE);
                    }
                    waveshare_rgb_lcd_display(BlackImage);
                    highlight = HIGHLIGHT_NONE;
                }
            } else if (point_data.cnt == 0 && highlight != HIGHLIGHT_NONE) {
                image_source_t src = IMAGE_SOURCE_LOCAL;
                if (highlight == HIGHLIGHT_RIGHT) {
                    src = IMAGE_SOURCE_REMOTE;
                } else if (highlight == HIGHLIGHT_NET) {
                    src = IMAGE_SOURCE_NETWORK;
                }
                draw_folder_button(btnL_x0, btnL_y0, btnL_x1, btnL_y1,
                                   "Locales", BTN_LABEL_L_OFFSET_X, WHITE);
                draw_folder_button(btnR_x0, btnR_y0, btnR_x1, btnR_y1,
                                   "Distantes", BTN_LABEL_R_OFFSET_X, WHITE);
                draw_folder_button(btnN_x0, btnN_y0, btnN_x1, btnN_y1,
                                   "Source réseau", BTN_LABEL_N_OFFSET_X, WHITE);
                waveshare_rgb_lcd_display(BlackImage);
                return src;
            }
        }
    }
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

static void draw_left_arrow(void)
{
    Paint_DrawRectangle(NAV_MARGIN, NAV_MARGIN,
                        NAV_MARGIN + ARROW_WIDTH, NAV_MARGIN + ARROW_HEIGHT,
                        WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    GUI_ReadBmp(NAV_MARGIN, NAV_MARGIN, MOUNT_POINT "/pic/arrow_left.bmp");
}

static void draw_right_arrow(void)
{
    UWORD x = g_display.width - NAV_MARGIN - ARROW_WIDTH;
    Paint_DrawRectangle(x, NAV_MARGIN,
                        x + ARROW_WIDTH, NAV_MARGIN + ARROW_HEIGHT,
                        WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    GUI_ReadBmp(x, NAV_MARGIN, MOUNT_POINT "/pic/arrow_right.bmp");
}

void draw_navigation_arrows(void)
{
    draw_left_arrow();
    draw_right_arrow();
    Paint_DrawString_EN(2, NAV_MARGIN, "H", &Font16, BLUE, WHITE);
    char batt_buf[8];
    uint8_t batt = battery_get_percentage();
    snprintf(batt_buf, sizeof(batt_buf), "%u%%", batt);
    UWORD bx = g_display.width - 4 * Font16.Width;
    UWORD by = NAV_MARGIN;
    Paint_DrawRectangle(bx, by, g_display.width, by + Font16.Height, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(bx, by, batt_buf, &Font16, BLACK, WHITE);
}

void draw_filename_bar(const char *path)
{
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;

    const sFONT *font = &Font20;
    UWORD bar_height = font->Height + 2 * FILENAME_BAR_PAD;
    UWORD y0 = g_display.height - bar_height;

    Paint_DrawRectangle(0, y0, g_display.width, g_display.height,
                        GRAY, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    UWORD max_chars = (g_display.width - 2 * FILENAME_BAR_PAD) / font->Width;
    char name_buf[128];
    if (strlen(fname) > max_chars) {
        if (max_chars > 3) {
            strncpy(name_buf, fname, max_chars - 3);
            name_buf[max_chars - 3] = '\0';
            strcat(name_buf, "...");
        } else {
            strncpy(name_buf, fname, max_chars);
            name_buf[max_chars] = '\0';
        }
        fname = name_buf;
    }

    Paint_DrawString_EN(FILENAME_BAR_PAD, y0 + FILENAME_BAR_PAD,
                        fname, font, WHITE, GRAY);
}

nav_action_t handle_touch_navigation(int8_t *idx, uint16_t *prev_x, uint16_t *prev_y)
{
    touch_gt911_point_t point_data;
    static enum { NAV_HL_NONE, NAV_HL_LEFT, NAV_HL_RIGHT } nav_hl = NAV_HL_NONE;
    static uint32_t prev_dist_sq = 0;
    static uint16_t prev_cx = 0, prev_cy = 0;
    if (xQueueReceive(s_touch_queue, &point_data, pdMS_TO_TICKS(50)) != pdTRUE) {
        return NAV_NONE;
    }
    if (point_data.cnt == 0) {
        if (nav_hl == NAV_HL_LEFT) {
            draw_left_arrow();
        } else if (nav_hl == NAV_HL_RIGHT) {
            draw_right_arrow();
        }
        if (nav_hl != NAV_HL_NONE) {
            waveshare_rgb_lcd_display(BlackImage);
        }
        nav_hl = NAV_HL_NONE;
        *prev_x = 0;
        *prev_y = 0;
        prev_dist_sq = 0;
        prev_cx = 0;
        prev_cy = 0;
        return NAV_NONE;
    }
    if (point_data.cnt >= 2) {
        uint16_t tx1 = point_data.x[0];
        uint16_t ty1 = point_data.y[0];
        uint16_t tx2 = point_data.x[1];
        uint16_t ty2 = point_data.y[1];
        orient_coords(&tx1, &ty1);
        orient_coords(&tx2, &ty2);
        uint32_t dx = (tx1 > tx2) ? tx1 - tx2 : tx2 - tx1;
        uint32_t dy = (ty1 > ty2) ? ty1 - ty2 : ty2 - ty1;
        uint32_t dist_sq = dx * dx + dy * dy;
        uint16_t cx = (tx1 + tx2) / 2;
        uint16_t cy = (ty1 + ty2) / 2;
        if (prev_dist_sq != 0) {
            int32_t diff = (int32_t)dist_sq - (int32_t)prev_dist_sq;
            if (diff > 1000 || diff < -1000) {
                s_zoom_level += (diff > 0) ? 0.1f : -0.1f;
                prev_dist_sq = dist_sq;
                ESP_LOGI(TAG_NAV, "Zoom niveau: %.2f", s_zoom_level);
                return (diff > 0) ? NAV_ZOOM_IN : NAV_ZOOM_OUT;
            }
            int16_t dcx = (int16_t)cx - (int16_t)prev_cx;
            int16_t dcy = (int16_t)cy - (int16_t)prev_cy;
            if (dcx > 10 || dcx < -10 || dcy > 10 || dcy < -10) {
                s_scroll_x += dcx;
                s_scroll_y += dcy;
                prev_cx = cx;
                prev_cy = cy;
                ESP_LOGI(TAG_NAV, "Défilement: %d,%d", s_scroll_x, s_scroll_y);
                return NAV_SCROLL;
            }
        } else {
            prev_dist_sq = dist_sq;
            prev_cx = cx;
            prev_cy = cy;
        }
        return NAV_NONE;
    }
    if (point_data.cnt != 1) {
        return NAV_NONE;
    }
    uint16_t tx = point_data.x[0];
    uint16_t ty = point_data.y[0];
    orient_coords(&tx, &ty);
    if ((*prev_x == tx) && (*prev_y == ty)) {
        return NAV_NONE;
    }
    if (tx < HOME_TOUCH_WIDTH && ty <= HOME_TOUCH_HEIGHT) {
        return NAV_HOME;
    }
    if (tx >= NAV_MARGIN && tx < NAV_MARGIN + ARROW_WIDTH &&
        ty >= NAV_MARGIN && ty < NAV_MARGIN + ARROW_HEIGHT) {
        (*idx)--;
        if (*idx < 0) {
            if (bmp_page_start > 0) {
                size_t new_start = (bmp_page_start >= bmp_last_page_size) ? bmp_page_start - bmp_last_page_size : 0;
                if (list_files_sorted(g_base_path, new_start) == ESP_OK && bmp_list.size > 0) {
                    *idx = bmp_list.size - 1;
                } else {
                    *idx = 0;
                }
            } else {
                *idx = bmp_list.size ? bmp_list.size - 1 : 0;
            }
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, bmp_list.items[*idx]);
        draw_navigation_arrows();
        draw_filename_bar(bmp_list.items[*idx]);
        draw_left_arrow();
        waveshare_rgb_lcd_display(BlackImage);
        nav_hl = NAV_HL_LEFT;
        *prev_x = tx;
        *prev_y = ty;
    } else if (tx >= g_display.width - NAV_MARGIN - ARROW_WIDTH &&
               tx < g_display.width - NAV_MARGIN &&
               ty >= NAV_MARGIN && ty < NAV_MARGIN + ARROW_HEIGHT) {
        (*idx)++;
        if (*idx >= bmp_list.size) {
            if (bmp_has_more) {
                size_t new_start = bmp_page_start + bmp_list.size;
                if (list_files_sorted(g_base_path, new_start) == ESP_OK && bmp_list.size > 0) {
                    *idx = 0;
                } else {
                    *idx = bmp_list.size ? bmp_list.size - 1 : 0;
                }
            } else {
                *idx = 0;
            }
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, bmp_list.items[*idx]);
        draw_navigation_arrows();
        draw_filename_bar(bmp_list.items[*idx]);
        draw_right_arrow();
        waveshare_rgb_lcd_display(BlackImage);
        nav_hl = NAV_HL_RIGHT;
        *prev_x = tx;
        *prev_y = ty;
    } else {
        if (nav_hl == NAV_HL_LEFT) {
            draw_left_arrow();
            waveshare_rgb_lcd_display(BlackImage);
            nav_hl = NAV_HL_NONE;
        } else if (nav_hl == NAV_HL_RIGHT) {
            draw_right_arrow();
            waveshare_rgb_lcd_display(BlackImage);
            nav_hl = NAV_HL_NONE;
        }
    }
    return NAV_NONE;
}
