/*****************************************************************************
 * | Fichier    :   main.c
 * | Auteur     :   équipe Waveshare
 * | Rôle       :   Fonction principale
 * | Description:   Lit des fichiers BMP depuis la carte SD et les affiche à l'écran.
 * |                Utiliser l'écran tactile pour passer d'une image à l'autre.
 *----------------
 * | Version    :   V1.0
 * | Date       :   2024-12-06
 * | Note       :   Version de base + sélection de dossier (Reptiles/Presentation)
 *
 ******************************************************************************/

#include "rgb_lcd_port.h"    // En-tête du pilote LCD RGB Waveshare
#include "gui_paint.h"       // En-tête des fonctions de dessin graphique
#include "gui_bmp.h"         // En-tête pour la gestion des images BMP
#include "gt911.h"           // En-tête des opérations de l'écran tactile (GT911)
#include "sd.h"              // En-tête des opérations sur carte SD
#include "config.h"

#include <dirent.h>          // En-tête pour les opérations sur répertoires
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define MAX_BMP_FILES            256
#define TOUCH_QUEUE_LENGTH       10
#define TOUCH_TASK_STACK         4096
#define TOUCH_TASK_PRIORITY      5
#define TEXT_X_DIVISOR           5
#define TEXT_Y1_DIVISOR          3
#define TEXT_LINE_SPACING        40
#define BTN_WIDTH                320
#define BTN_HEIGHT               120
#define NAV_MARGIN               20
#define NAV_LINE_LEN             60
#define NAV_TOUCH_HEIGHT         60
#define NAV_HEAD_OFFSET          25
#define NAV_HEAD_Y_OFFSET        15
#define BTN_LABEL_L_OFFSET_X     60
#define BTN_LABEL_R_OFFSET_X     40
#define BTN_LABEL_OFFSET_Y       12
#define BASE_PATH_LEN            128
#define PAINT_SCALE              65
#define HOME_TOUCH_WIDTH         NAV_MARGIN
#define HOME_TOUCH_HEIGHT        NAV_TOUCH_HEIGHT

static char *BmpPath[MAX_BMP_FILES];        // Tableau pour stocker les chemins des fichiers BMP
static uint8_t bmp_num;           // Nombre de fichiers BMP trouvés
static const char *TAG = "APP";
static UBYTE *BlackImage;         // Framebuffer global
static TaskHandle_t s_touch_task_handle;  // Tâche de traitement tactile
static QueueHandle_t s_touch_queue;       // File de messages pour événements tactiles
static esp_lcd_touch_handle_t s_touch_handle; // Handle du contrôleur tactile
static const display_geometry_t g_display = {
    .width = LCD_H_RES,
    .height = LCD_V_RES,
    .margin_left = LCD_MARGIN_LEFT,
    .margin_right = LCD_MARGIN_RIGHT,
    .margin_top = LCD_MARGIN_TOP,
    .margin_bottom = LCD_MARGIN_BOTTOM,
};

typedef enum {
    APP_STATE_FOLDER_SELECTION = 0,
    APP_STATE_NAVIGATION,
    APP_STATE_ERROR,
    APP_STATE_EXIT
} app_state_t;

typedef enum {
    NAV_NONE = 0,
    NAV_EXIT,
    NAV_HOME
} nav_action_t;

static void free_bmp_paths(void);

static int bmp_path_cmp(const void *a, const void *b)
{
    const char *const *pa = a;
    const char *const *pb = b;
    return strcasecmp(*pa, *pb);
}

// Fonction listant et triant tous les fichiers BMP d'un répertoire
esp_err_t list_files_sorted(const char *base_path)
{
    free_bmp_paths();
    DIR *dir = opendir(base_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire : %s", base_path);
        bmp_num = 0;
        return ESP_FAIL;
    }

    struct dirent *entry;
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        const char *file_name = entry->d_name;
        size_t len = strlen(file_name);
        if (len > 4 && strcasecmp(&file_name[len - 4], ".bmp") == 0) {
            if (i >= MAX_BMP_FILES) {
                ESP_LOGE(TAG, "Quota maximal de chemins BMP atteint (%d)", MAX_BMP_FILES);
                closedir(dir);
                bmp_num = i;
                qsort(BmpPath, bmp_num, sizeof(char *), bmp_path_cmp);
                return ESP_ERR_INVALID_SIZE;
            }
            size_t length = strlen(base_path) + strlen(file_name) + 2;
            BmpPath[i] = malloc(length);
            if (BmpPath[i] == NULL) {
                ESP_LOGE(TAG, "Échec d'allocation mémoire pour le chemin BMP");
                closedir(dir);
                bmp_num = i;
                qsort(BmpPath, bmp_num, sizeof(char *), bmp_path_cmp);
                return ESP_ERR_NO_MEM;
            }
            snprintf(BmpPath[i], length, "%s/%s", base_path, file_name);
            i++;
        }
    }
    closedir(dir);
    bmp_num = i;
    qsort(BmpPath, bmp_num, sizeof(char *), bmp_path_cmp);
    return ESP_OK;
}

static void free_bmp_paths(void)
{
    for (int i = 0; i < bmp_num && i < MAX_BMP_FILES; i++) {
        free(BmpPath[i]);
        BmpPath[i] = NULL;
    }
    bmp_num = 0;
}

static void app_cleanup(void)
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
    esp_err_t unmount_ret = sd_mmc_unmount();
    if (unmount_ret != ESP_OK) {
        ESP_LOGW(TAG, "sd_mmc_unmount a échoué : %s", esp_err_to_name(unmount_ret));
    }
    free_bmp_paths();
    free(BlackImage);
    BlackImage = NULL;
}

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
        data = touch_gt911_read_point(1);
        xQueueSend(s_touch_queue, &data, portMAX_DELAY);
    }
}

static inline void orient_coords(uint16_t *x, uint16_t *y)
{
#if CONFIG_DISPLAY_ORIENTATION_PORTRAIT
    uint16_t tx = *x;
    *x = *y;
    *y = g_display.width - tx;
#endif
}

static bool init_peripherals(void)
{
    s_touch_handle = touch_gt911_init();
    if (s_touch_handle == NULL) {
        ESP_LOGE(TAG, "Échec d'initialisation du contrôleur tactile");
        return false;
    }

    esp_lcd_panel_handle_t panel = waveshare_esp32_s3_rgb_lcd_init();
    if (panel == NULL) {
        ESP_LOGE(TAG, "Échec d'initialisation du LCD");
        return false;
    }

    wavesahre_rgb_lcd_bl_on();

    UDOUBLE Imagesize = g_display.width * g_display.height * 2;
    BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        ESP_LOGE(TAG, "Échec d’allocation mémoire pour le framebuffer...");
        return false;
    }

    Paint_NewImage(BlackImage, g_display.width, g_display.height, 0, WHITE);
    Paint_SetScale(PAINT_SCALE);
    Paint_Clear(WHITE);

    s_touch_queue = xQueueCreate(TOUCH_QUEUE_LENGTH, sizeof(touch_gt911_point_t));
    if (s_touch_queue == NULL) {
        ESP_LOGE(TAG, "Échec de création de la file tactile");
        return false;
    }
    if (xTaskCreate(touch_task, "touch_task", TOUCH_TASK_STACK, NULL, TOUCH_TASK_PRIORITY, &s_touch_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Échec de création de la tâche tactile");
        return false;
    }
    esp_err_t err = esp_lcd_touch_register_interrupt_callback(s_touch_handle, touch_int_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement du callback tactile échoué : %s", esp_err_to_name(err));
        return false;
    }

    return true;
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

static const char *draw_folder_selection(void)
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

    wavesahre_rgb_lcd_display(BlackImage);

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
                        wavesahre_rgb_lcd_display(BlackImage);
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
                        wavesahre_rgb_lcd_display(BlackImage);
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
                    wavesahre_rgb_lcd_display(BlackImage);
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
                wavesahre_rgb_lcd_display(BlackImage);
                highlight = HIGHLIGHT_NONE;
            }
        }
    }

    Paint_Clear(WHITE);
    Paint_DrawString_EN(text_x, text_y1, "Dossier choisi :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2, (char *)selected_dir, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(text_x, text_y2 + TEXT_LINE_SPACING, "Touchez la fleche pour demarrer.", &Font24, BLACK, WHITE);
    wavesahre_rgb_lcd_display(BlackImage);
    return selected_dir;
}

static void draw_left_arrow(UWORD color)
{
    Paint_DrawLine(NAV_MARGIN, NAV_MARGIN, NAV_MARGIN + NAV_LINE_LEN, NAV_MARGIN,
                   color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(NAV_MARGIN, NAV_MARGIN, NAV_MARGIN + NAV_HEAD_OFFSET,
                   NAV_MARGIN - NAV_HEAD_Y_OFFSET, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(NAV_MARGIN, NAV_MARGIN, NAV_MARGIN + NAV_HEAD_OFFSET,
                   NAV_MARGIN + NAV_HEAD_Y_OFFSET, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

static void draw_right_arrow(UWORD color)
{
    Paint_DrawLine(g_display.width - NAV_MARGIN - NAV_LINE_LEN, NAV_MARGIN,
                   g_display.width - NAV_MARGIN, NAV_MARGIN, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(g_display.width - NAV_MARGIN, NAV_MARGIN,
                   g_display.width - NAV_MARGIN - NAV_HEAD_OFFSET,
                   NAV_MARGIN - NAV_HEAD_Y_OFFSET, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(g_display.width - NAV_MARGIN, NAV_MARGIN,
                   g_display.width - NAV_MARGIN - NAV_HEAD_OFFSET,
                   NAV_MARGIN + NAV_HEAD_Y_OFFSET, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

static void draw_navigation_arrows(void)
{
    draw_left_arrow(RED);
    draw_right_arrow(RED);
    // Bouton Home (lettre H dans le coin supérieur gauche)
    Paint_DrawString_EN(2, NAV_MARGIN, "H", &Font16, BLUE, WHITE);
}

static nav_action_t handle_touch_navigation(int8_t *idx, uint16_t *prev_x, uint16_t *prev_y)
{
    touch_gt911_point_t point_data;
    static enum { NAV_HL_NONE, NAV_HL_LEFT, NAV_HL_RIGHT } nav_hl = NAV_HL_NONE;
    if (xQueueReceive(s_touch_queue, &point_data, pdMS_TO_TICKS(50)) != pdTRUE) {
        return NAV_NONE;
    }
    if (point_data.cnt == 2) {
        return NAV_EXIT;
    }
    if (point_data.cnt == 0) {
        if (nav_hl == NAV_HL_LEFT) {
            draw_left_arrow(RED);
        } else if (nav_hl == NAV_HL_RIGHT) {
            draw_right_arrow(RED);
        }
        if (nav_hl != NAV_HL_NONE) {
            wavesahre_rgb_lcd_display(BlackImage);
        }
        nav_hl = NAV_HL_NONE;
        *prev_x = 0;
        *prev_y = 0;
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
    if (tx >= NAV_MARGIN && tx <= NAV_MARGIN + NAV_LINE_LEN && ty >= 0 && ty <= NAV_TOUCH_HEIGHT) {
        (*idx)--;
        if (*idx < 0) {
            *idx = bmp_num - 1;
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, BmpPath[*idx]);
        draw_navigation_arrows();
        draw_left_arrow(GRAY);
        wavesahre_rgb_lcd_display(BlackImage);
        nav_hl = NAV_HL_LEFT;
        *prev_x = tx;
        *prev_y = ty;
    } else if (tx >= g_display.width - NAV_MARGIN - NAV_LINE_LEN && tx <= g_display.width - NAV_MARGIN && ty >= 0 && ty <= NAV_TOUCH_HEIGHT) {
        (*idx)++;
        if (*idx > bmp_num - 1) {
            *idx = 0;
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, BmpPath[*idx]);
        draw_navigation_arrows();
        draw_right_arrow(GRAY);
        wavesahre_rgb_lcd_display(BlackImage);
        nav_hl = NAV_HL_RIGHT;
        *prev_x = tx;
        *prev_y = ty;
    } else {
        if (nav_hl == NAV_HL_LEFT) {
            draw_left_arrow(RED);
            wavesahre_rgb_lcd_display(BlackImage);
            nav_hl = NAV_HL_NONE;
        } else if (nav_hl == NAV_HL_RIGHT) {
            draw_right_arrow(RED);
            wavesahre_rgb_lcd_display(BlackImage);
            nav_hl = NAV_HL_NONE;
        }
    }
    return NAV_NONE;
}

static void process_background_tasks(void)
{
    // Espace réservé pour d'autres traitements périodiques : watchdog, animations, etc.
}
// Fonction principale de l'application
void app_main(void)
{
    if (!init_peripherals()) {
        app_cleanup();
        return;
    }

    esp_err_t sd_ret = sd_mmc_init();
    if (sd_ret != ESP_OK) {
        ESP_LOGE(TAG, "sd_mmc_init a échoué : %s", esp_err_to_name(sd_ret));
        UWORD err_x = g_display.width / TEXT_X_DIVISOR;
        UWORD err_y = g_display.height / TEXT_Y1_DIVISOR;
        Paint_DrawString_EN(err_x, err_y, "Échec carte SD !", &Font24, BLACK, WHITE);
        wavesahre_rgb_lcd_display(BlackImage);
        app_cleanup();
        return;
    }

    app_state_t state = APP_STATE_FOLDER_SELECTION;
    const char *selected_dir = NULL;
    char base_path[BASE_PATH_LEN];
    int8_t index = 0;
    uint16_t prev_x = 0;
    uint16_t prev_y = 0;

    while (state != APP_STATE_EXIT) {
        switch (state) {
        case APP_STATE_FOLDER_SELECTION:
            selected_dir = draw_folder_selection();
            if (selected_dir == NULL) {
                state = APP_STATE_EXIT;
                break;
            }
            snprintf(base_path, sizeof(base_path), "%s/%s", MOUNT_POINT, selected_dir);
            esp_err_t err = list_files_sorted(base_path);
            if (err == ESP_ERR_INVALID_SIZE) {
                ESP_LOGW(TAG, "Plus de %d fichiers BMP détectés, pagination requise", MAX_BMP_FILES);
            } else if (err != ESP_OK) {
                ESP_LOGE(TAG, "Erreur lors du listage : %s", esp_err_to_name(err));
            }
            if (bmp_num == 0) {
                UWORD nofile_x = g_display.width / TEXT_X_DIVISOR;
                UWORD nofile_y = (g_display.height / TEXT_Y1_DIVISOR) + 3 * TEXT_LINE_SPACING;
                Paint_DrawString_EN(nofile_x, nofile_y, "Aucun fichier BMP dans ce dossier.", &Font24, RED, WHITE);
                wavesahre_rgb_lcd_display(BlackImage);
                app_cleanup();
                state = APP_STATE_ERROR;
            } else {
                draw_navigation_arrows();
                wavesahre_rgb_lcd_display(BlackImage);
                state = APP_STATE_NAVIGATION;
            }
            break;

        case APP_STATE_NAVIGATION: {
            nav_action_t act = handle_touch_navigation(&index, &prev_x, &prev_y);
            if (act == NAV_EXIT) {
                state = APP_STATE_EXIT;
            } else if (act == NAV_HOME) {
                free_bmp_paths();
                index = 0;
                prev_x = 0;
                prev_y = 0;
                selected_dir = NULL;
                Paint_Clear(WHITE);
                wavesahre_rgb_lcd_display(BlackImage);
                state = APP_STATE_FOLDER_SELECTION;
            }
            break;
        }

        case APP_STATE_ERROR:
            vTaskDelay(portMAX_DELAY);
            break;

        case APP_STATE_EXIT:
            break;
        }
        process_background_tasks();
    }
    app_cleanup();
    vTaskDelete(NULL);
}
