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
#include "pm.h"
#include "can_display.h"
#include "rs485_display.h"
#include "http_server.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_sleep.h"

#define BASE_PATH_LEN            128
#define PAINT_SCALE              65
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define WIFI_MAX_RETRY           3

char g_base_path[BASE_PATH_LEN];     // Chemin du dossier actuellement affiché
static const char *TAG = "APP";
UBYTE *BlackImage;         // Framebuffer global
display_geometry_t g_display;

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
    ESP_ERROR_CHECK(nvs_flash_init());
    display_load_orientation();

    if (!touch_task_init()) {
        ESP_LOGE(TAG, "Échec d'initialisation de la tâche tactile");
        return false;
    }

    esp_lcd_panel_handle_t panel = waveshare_esp32_s3_rgb_lcd_init();
    if (panel == NULL) {
        ESP_LOGE(TAG, "Échec d'initialisation du LCD");
        return false;
    }

    waveshare_rgb_lcd_bl_on();
    waveshare_rgb_lcd_set_brightness(100);
    battery_init();

    if (can_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Échec d'initialisation du module CAN");
        return false;
    }
    if (rs485_display_init() != ESP_OK) {
        ESP_LOGE(TAG, "Échec d'initialisation du module RS485");
        return false;
    }

    UDOUBLE Imagesize = LCD_H_RES * LCD_V_RES * 2;
    BlackImage = (UBYTE *)heap_caps_malloc(Imagesize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (BlackImage == NULL) {
        ESP_LOGE(TAG, "Échec d’allocation mémoire pour le framebuffer : %s", esp_err_to_name(ESP_ERR_NO_MEM));
        return false;
    }

    Paint_NewImage(BlackImage, LCD_H_RES, LCD_V_RES, 0, WHITE);
    Paint_SetScale(PAINT_SCALE);
    Paint_SetRotate(g_is_portrait ? ROTATE_90 : ROTATE_0);
    Paint_Clear(WHITE);

    return true;
}


static TickType_t s_last_activity_ticks;

void pm_update_activity(void)
{
    s_last_activity_ticks = xTaskGetTickCount();
}

static void process_background_tasks(void)
{
    uint8_t batt = battery_get_percentage();
    uint8_t level = CONFIG_MIN_BRIGHTNESS +
                    (CONFIG_MAX_BRIGHTNESS - CONFIG_MIN_BRIGHTNESS) * batt / 100;
    waveshare_rgb_lcd_set_brightness(level);

    TickType_t now = xTaskGetTickCount();
    if (pdTICKS_TO_MS(now - s_last_activity_ticks) > INACTIVITY_TIMEOUT_MS) {
        esp_light_sleep_start();
        pm_update_activity();
    }
}

// Fonction principale de l'application  
void app_main(void)
{
    if (!init_peripherals()) {
        app_cleanup();
        return;
    }

    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_cfg));
    pm_update_activity();

    esp_err_t sd_ret = sd_mmc_init();
    if (sd_ret != ESP_OK) {
        ESP_LOGE(TAG, "sd_mmc_init a échoué : %s", esp_err_to_name(sd_ret));
        UWORD err_x = g_display.width / TEXT_X_DIVISOR;
        UWORD err_y = g_display.height / TEXT_Y1_DIVISOR;
        Paint_DrawString_EN(err_x, err_y, "Échec carte SD !", &Font24, BLACK, WHITE);
        waveshare_rgb_lcd_display(BlackImage);
        app_cleanup();
        return;
    }

    draw_orientation_menu();

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
                esp_err_t wifi_ret = ESP_FAIL;
                for (int attempt = 0; attempt < WIFI_MAX_RETRY && wifi_ret != ESP_OK; ++attempt) {
                    wifi_ret = wifi_init_sta(WIFI_CONNECT_TIMEOUT_MS);
                    if (wifi_ret != ESP_OK) {
                        ESP_LOGW(TAG, "Tentative WiFi %d/%d échouée: %s", attempt + 1,
                                 WIFI_MAX_RETRY, esp_err_to_name(wifi_ret));
                        UWORD err_x = g_display.width / TEXT_X_DIVISOR;
                        UWORD err_y = g_display.height / TEXT_Y1_DIVISOR;
                        Paint_DrawString_EN(err_x, err_y, "Échec WiFi...", &Font24, RED, WHITE);
                        waveshare_rgb_lcd_display(BlackImage);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                if (wifi_ret != ESP_OK) {
                    state = APP_STATE_ERROR;
                    break;
                }
                image_fetch_http_to_sd(CONFIG_IMAGE_FETCH_URL, MOUNT_POINT "/remote.bmp");
                snprintf(g_base_path, sizeof(g_base_path), "%s", MOUNT_POINT);
                bmp_page_start = 0;
                esp_err_t err = list_files_sorted(g_base_path, bmp_page_start);
                if (err != ESP_OK || bmp_list.size == 0) {
                    UWORD nofile_x = g_display.width / TEXT_X_DIVISOR;
                    UWORD nofile_y = (g_display.height / TEXT_Y1_DIVISOR);
                    Paint_DrawString_EN(nofile_x, nofile_y, "Aucune image distante.", &Font24, RED, WHITE);
                    waveshare_rgb_lcd_display(BlackImage);
                    state = APP_STATE_ERROR;
                } else {
                    draw_navigation_arrows();
                    waveshare_rgb_lcd_display(BlackImage);
                    state = APP_STATE_NAVIGATION;
                }
            } else if (img_src == IMAGE_SOURCE_NETWORK) {
                esp_err_t wifi_ret = ESP_FAIL;
                for (int attempt = 0; attempt < WIFI_MAX_RETRY && wifi_ret != ESP_OK; ++attempt) {
                    wifi_ret = wifi_init_sta(WIFI_CONNECT_TIMEOUT_MS);
                    if (wifi_ret != ESP_OK) {
                        ESP_LOGW(TAG, "Tentative WiFi %d/%d échouée: %s", attempt + 1,
                                 WIFI_MAX_RETRY, esp_err_to_name(wifi_ret));
                        UWORD err_x = g_display.width / TEXT_X_DIVISOR;
                        UWORD err_y = g_display.height / TEXT_Y1_DIVISOR;
                        Paint_DrawString_EN(err_x, err_y, "Échec WiFi...", &Font24, RED, WHITE);
                        waveshare_rgb_lcd_display(BlackImage);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                if (wifi_ret == ESP_OK) {
                    if (start_file_server() == ESP_OK) {
                        esp_netif_ip_info_t ip;
                        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                        if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
                            char ip_str[IP4ADDR_STRLEN_MAX];
                            ip4addr_ntoa_r((ip4_addr_t *)&ip.ip, ip_str, sizeof(ip_str));
                            char url[32];
                            snprintf(url, sizeof(url), "http://%s", ip_str);
                            UWORD msg_x = g_display.width / TEXT_X_DIVISOR;
                            UWORD msg_y = g_display.height / TEXT_Y1_DIVISOR;
                            Paint_Clear(WHITE);
                            Paint_DrawString_EN(msg_x, msg_y, "Upload BMP via:", &Font24, BLACK, WHITE);
                            Paint_DrawString_EN(msg_x, msg_y + TEXT_LINE_SPACING, url, &Font24, BLACK, WHITE);
                            waveshare_rgb_lcd_display(BlackImage);
                        }
                    }
                }
                state = APP_STATE_SOURCE_SELECTION;
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
                waveshare_rgb_lcd_display(BlackImage);  
                app_cleanup();  
                state = APP_STATE_ERROR;  
            } else {  
                draw_navigation_arrows();  
                waveshare_rgb_lcd_display(BlackImage);  
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
                waveshare_rgb_lcd_display(BlackImage);
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
