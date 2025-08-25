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
#include "sd.h"              // En-tête des opérations sur carte SD
#include "config.h"
#include "battery.h"
#include "file_manager.h"
#include "ui_navigation.h"
#include "touch_task.h"
#include "wifi.h"
#include "image_fetcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BASE_PATH_LEN            128
#define PAINT_SCALE              65

char g_base_path[BASE_PATH_LEN];     // Chemin du dossier actuellement affiché
static const char *TAG = "APP";
UBYTE *BlackImage;         // Framebuffer global
const display_geometry_t g_display = {
    .width = LCD_H_RES,  
    .height = LCD_V_RES,  
    .margin_left = LCD_MARGIN_LEFT,  
    .margin_right = LCD_MARGIN_RIGHT,  
    .margin_top = LCD_MARGIN_TOP,  
    .margin_bottom = LCD_MARGIN_BOTTOM,  
};  

typedef enum {
    APP_STATE_SOURCE_SELECTION = 0,
    APP_STATE_FOLDER_SELECTION,
    APP_STATE_NAVIGATION,
    APP_STATE_ERROR,
    APP_STATE_EXIT
} app_state_t;


static void app_cleanup(void)
{
    touch_task_deinit();
    esp_err_t unmount_ret = sd_mmc_unmount();
    if (unmount_ret != ESP_OK) {
        ESP_LOGW(TAG, "sd_mmc_unmount a échoué : %s", esp_err_to_name(unmount_ret));
    }
    bmp_list_free();
    free(BlackImage);
    BlackImage = NULL;
}

static bool init_peripherals(void)
{
    if (!touch_task_init()) {
        ESP_LOGE(TAG, "Échec d'initialisation de la tâche tactile");
        return false;
    }

    esp_lcd_panel_handle_t panel = waveshare_esp32_s3_rgb_lcd_init();
    if (panel == NULL) {
        ESP_LOGE(TAG, "Échec d'initialisation du LCD");
        return false;
    }

    wavesahre_rgb_lcd_bl_on();
    waveshare_rgb_lcd_set_brightness(100);
    battery_init();

    UDOUBLE Imagesize = g_display.width * g_display.height * 2;
    BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        ESP_LOGE(TAG, "Échec d’allocation mémoire pour le framebuffer...");
        return false;
    }

    Paint_NewImage(BlackImage, g_display.width, g_display.height, 0, WHITE);
    Paint_SetScale(PAINT_SCALE);
    Paint_Clear(WHITE);

    return true;
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

    app_state_t state = APP_STATE_SOURCE_SELECTION;
    const char *selected_dir = NULL;
    image_source_t img_src = IMAGE_SOURCE_LOCAL;
    int8_t index = 0;
    uint16_t prev_x = 0;
    uint16_t prev_y = 0;

    while (state != APP_STATE_EXIT) {  
        switch (state) {  
        case APP_STATE_SOURCE_SELECTION:
            img_src = draw_source_selection();
            if (img_src == IMAGE_SOURCE_REMOTE) {
                wifi_init_sta();
                image_fetch_http_to_sd(CONFIG_IMAGE_FETCH_URL, MOUNT_POINT "/remote.bmp");
                snprintf(g_base_path, sizeof(g_base_path), "%s", MOUNT_POINT);
                bmp_page_start = 0;
                esp_err_t err = list_files_sorted(g_base_path, bmp_page_start);
                if (err != ESP_OK || bmp_list.size == 0) {
                    UWORD nofile_x = g_display.width / TEXT_X_DIVISOR;
                    UWORD nofile_y = (g_display.height / TEXT_Y1_DIVISOR);
                    Paint_DrawString_EN(nofile_x, nofile_y, "Aucune image distante.", &Font24, RED, WHITE);
                    wavesahre_rgb_lcd_display(BlackImage);
                    state = APP_STATE_ERROR;
                } else {
                    draw_navigation_arrows();
                    wavesahre_rgb_lcd_display(BlackImage);
                    state = APP_STATE_NAVIGATION;
                }
            } else {
                state = APP_STATE_FOLDER_SELECTION;
            }
            break;

        case APP_STATE_FOLDER_SELECTION:
            selected_dir = draw_folder_selection();
            if (selected_dir == NULL) {
                state = APP_STATE_EXIT;
                break;
            }
            snprintf(g_base_path, sizeof(g_base_path), "%s/%s", MOUNT_POINT, selected_dir);
            bmp_page_start = 0;
            esp_err_t err = list_files_sorted(g_base_path, bmp_page_start);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Erreur lors du listage : %s", esp_err_to_name(err));
            }
            if (bmp_list.size == 0) {
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
                bmp_list_free();
                index = 0;
                prev_x = 0;
                prev_y = 0;
                selected_dir = NULL;
                Paint_Clear(WHITE);
                wavesahre_rgb_lcd_display(BlackImage);
                state = APP_STATE_SOURCE_SELECTION;
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
