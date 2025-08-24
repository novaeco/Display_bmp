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

static char *BmpPath[256];        // Tableau pour stocker les chemins des fichiers BMP
static uint8_t bmp_num;           // Nombre de fichiers BMP trouvés
static const char *TAG = "APP";
static UBYTE *BlackImage;         // Framebuffer global
static TaskHandle_t s_touch_task_handle;  // Tâche de traitement tactile
static QueueHandle_t s_touch_queue;       // File de messages pour événements tactiles
static esp_lcd_touch_handle_t s_touch_handle; // Handle du contrôleur tactile

typedef enum {
    APP_STATE_FOLDER_SELECTION = 0,
    APP_STATE_NAVIGATION,
    APP_STATE_ERROR,
    APP_STATE_EXIT
} app_state_t;

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
            if (i >= 256) {
                ESP_LOGE(TAG, "Quota maximal de chemins BMP atteint (256)");
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
    for (int i = 0; i < bmp_num && i < 256; i++) {
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

    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;
    BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        ESP_LOGE(TAG, "Échec d’allocation mémoire pour le framebuffer...");
        return false;
    }

    Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(WHITE);

    s_touch_queue = xQueueCreate(10, sizeof(touch_gt911_point_t));
    if (s_touch_queue == NULL) {
        ESP_LOGE(TAG, "Échec de création de la file tactile");
        return false;
    }
    if (xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, &s_touch_task_handle) != pdPASS) {
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

static const char *draw_folder_selection(void)
{
    touch_gt911_point_t point_data;

    Paint_DrawString_EN(200, 200, "Carte SD OK !", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(200, 240, "Choisissez un dossier :", &Font24, BLACK, WHITE);

    UWORD btnL_x0 = 120;
    UWORD btnL_y0 = 280;
    UWORD btnL_x1 = 440;
    UWORD btnL_y1 = 400;

    UWORD btnR_x1 = EXAMPLE_LCD_H_RES - 120;
    UWORD btnR_x0 = btnR_x1 - 320;
    UWORD btnR_y0 = 280;
    UWORD btnR_y1 = 400;

    // Rectangle gauche Reptiles
    Paint_DrawLine(btnL_x0, btnL_y0, btnL_x1, btnL_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnL_x1, btnL_y0, btnL_x1, btnL_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnL_x1, btnL_y1, btnL_x0, btnL_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnL_x0, btnL_y1, btnL_x0, btnL_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawString_EN(btnL_x0 + 60, (btnL_y0 + btnL_y1)/2 - 12, "Reptiles", &Font24, BLACK, WHITE);

    // Rectangle droit Presentation
    Paint_DrawLine(btnR_x0, btnR_y0, btnR_x1, btnR_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnR_x1, btnR_y0, btnR_x1, btnR_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnR_x1, btnR_y1, btnR_x0, btnR_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(btnR_x0, btnR_y1, btnR_x0, btnR_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawString_EN(btnR_x0 + 40, (btnR_y0 + btnR_y1)/2 - 12, "Presentation", &Font24, BLACK, WHITE);

    wavesahre_rgb_lcd_display(BlackImage);

    const char *selected_dir = NULL;
    while (selected_dir == NULL) {
        if (xQueueReceive(s_touch_queue, &point_data, portMAX_DELAY) == pdTRUE) {
            if (point_data.cnt == 2) {
                return NULL;
            }
            if (point_data.cnt == 1) {
                uint16_t tx = point_data.x[0];
                uint16_t ty = point_data.y[0];
                if (tx >= btnL_x0 && tx <= btnL_x1 && ty >= btnL_y0 && ty <= btnL_y1) {
                    selected_dir = "Reptiles";
                } else if (tx >= btnR_x0 && tx <= btnR_x1 && ty >= btnR_y0 && ty <= btnR_y1) {
                    selected_dir = "Presentation";
                }
            }
        }
    }

    Paint_Clear(WHITE);
    Paint_DrawString_EN(200, 200, "Dossier choisi :", &Font24, BLACK, WHITE);
    Paint_DrawString_EN(200, 240, (char *)selected_dir, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(200, 280, "Touchez la fleche pour demarrer.", &Font24, BLACK, WHITE);
    wavesahre_rgb_lcd_display(BlackImage);
    return selected_dir;
}

static void draw_navigation_arrows(void)
{
    // Flèche gauche
    Paint_DrawLine(20, 20, 80, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(20, 20, 45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(20, 20, 45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    // Flèche droite
    Paint_DrawLine(EXAMPLE_LCD_H_RES-80, 20, EXAMPLE_LCD_H_RES-20, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

static bool handle_touch_navigation(int8_t *idx, uint16_t *prev_x, uint16_t *prev_y)
{
    touch_gt911_point_t point_data;
    if (xQueueReceive(s_touch_queue, &point_data, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (point_data.cnt == 2) {
        return true;
    }
    if (point_data.cnt != 1) {
        return false;
    }
    if ((*prev_x == point_data.x[0]) && (*prev_y == point_data.y[0])) {
        return false;
    }
    if (point_data.x[0] >= 20 && point_data.x[0] <= 80 && point_data.y[0] >= 0 && point_data.y[0] <= 60) {
        (*idx)--;
        if (*idx < 0) {
            *idx = bmp_num - 1;
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, BmpPath[*idx]);
        draw_navigation_arrows();
        wavesahre_rgb_lcd_display(BlackImage);
        *prev_x = point_data.x[0];
        *prev_y = point_data.y[0];
    } else if (point_data.x[0] >= EXAMPLE_LCD_H_RES-80 && point_data.x[0] <= EXAMPLE_LCD_H_RES-20 && point_data.y[0] >= 0 && point_data.y[0] <= 60) {
        (*idx)++;
        if (*idx > bmp_num - 1) {
            *idx = 0;
        }
        Paint_Clear(WHITE);
        GUI_ReadBmp(0, 0, BmpPath[*idx]);
        draw_navigation_arrows();
        wavesahre_rgb_lcd_display(BlackImage);
        *prev_x = point_data.x[0];
        *prev_y = point_data.y[0];
    }
    return false;
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
        Paint_DrawString_EN(200, 200, "Échec carte SD !", &Font24, BLACK, WHITE);
        wavesahre_rgb_lcd_display(BlackImage);
        app_cleanup();
        return;
    }

    app_state_t state = APP_STATE_FOLDER_SELECTION;
    const char *selected_dir = NULL;
    char base_path[128];
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
                ESP_LOGW(TAG, "Plus de 256 fichiers BMP détectés, pagination requise");
            } else if (err != ESP_OK) {
                ESP_LOGE(TAG, "Erreur lors du listage : %s", esp_err_to_name(err));
            }
            if (bmp_num == 0) {
                Paint_DrawString_EN(200, 320, "Aucun fichier BMP dans ce dossier.", &Font24, RED, WHITE);
                wavesahre_rgb_lcd_display(BlackImage);
                app_cleanup();
                state = APP_STATE_ERROR;
            } else {
                draw_navigation_arrows();
                wavesahre_rgb_lcd_display(BlackImage);
                state = APP_STATE_NAVIGATION;
            }
            break;

        case APP_STATE_NAVIGATION:
            if (handle_touch_navigation(&index, &prev_x, &prev_y)) {
                state = APP_STATE_EXIT;
            }
            break;

        case APP_STATE_ERROR:
            vTaskDelay(portMAX_DELAY);
            break;

        case APP_STATE_EXIT:
            break;
        }
    }
    app_cleanup();
    vTaskDelete(NULL);
}
