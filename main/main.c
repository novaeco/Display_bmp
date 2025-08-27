/*****************************************************************************
 * | Fichier    :   main.c
 * | Auteur     :   équipe Waveshare
 * | Rôle       :   Fonction principale
 * | Description:   Lit des fichiers PNG depuis la carte SD et les affiche à
 *l'écran. |                Utiliser l'écran tactile pour passer d'une image à
 *l'autre.
 *----------------
 * | Version    :   V1.0
 * | Date       :   2024-12-06
 * | Note       :   Version de base + sélection de dossier
 *(Reptiles/Presentation)
 *
 ******************************************************************************/

#include "battery.h"
#include "can_display.h"
#include "config.h"
#include "esp_netif.h"
#include "file_manager.h"
#include "gui.h"
#include "http_server.h"
#include "image_fetcher.h"
#include "lvfs_fatfs.h"
#include "lvgl.h"
#include "lwip/inet.h"
#include "pm.h"
#include "rgb_lcd_port.h" // En-tête du pilote LCD RGB Waveshare
#include "rs485_display.h"
#include "sd.h" // En-tête des opérations sur carte SD
#include "touch_task.h"
#include "ui_navigation.h"
#include "wifi_manager.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_psram.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE_PATH_LEN 128
#define WIFI_CONNECT_TIMEOUT_MS 10000

char g_base_path[BASE_PATH_LEN]; // Chemin du dossier actuellement affiché
static const char *TAG = "APP";

typedef enum {
  APP_STATE_SOURCE_SELECTION = 0,
  APP_STATE_FOLDER_SELECTION,
  APP_STATE_NAVIGATION,
  APP_STATE_ERROR,
  APP_STATE_EXIT
} app_state_t;

static volatile bool s_wifi_ready = false;
static volatile bool s_wifi_failed = false;

static void wifi_status_cb(wifi_manager_event_t event) {
  if (event == WIFI_MANAGER_EVENT_CONNECTED) {
    s_wifi_ready = true;
  } else if (event == WIFI_MANAGER_EVENT_FAIL) {
    s_wifi_failed = true;
  }
}

static void app_cleanup(void) {
  ui_navigation_deinit();
  touch_task_deinit();
  touch_gt911_deinit();
  wifi_manager_stop();
  stop_file_server();
  esp_err_t unmount_ret = sd_mmc_unmount();
  if (unmount_ret != ESP_OK) {
    ESP_LOGW(TAG, "sd_mmc_unmount a échoué : %s", esp_err_to_name(unmount_ret));
  }
  png_list_free();
  gui_deinit();
}

static bool init_peripherals(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  display_load_orientation();

  ESP_ERROR_CHECK(esp_psram_init());

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
  gui_init(panel);

  if (can_display_init() != ESP_OK) {
    ESP_LOGE(TAG, "Échec d'initialisation du module CAN");
    return false;
  }
  if (rs485_display_init() != ESP_OK) {
    ESP_LOGE(TAG, "Échec d'initialisation du module RS485");
    return false;
  }

  return true;
}

static TickType_t s_last_activity_ticks;

void pm_update_activity(void) { s_last_activity_ticks = xTaskGetTickCount(); }

static void process_background_tasks(void) {
  static int s_prev_level = -1;
  uint8_t batt = battery_get_percentage();
  float normalized = batt / 100.0f;
  float corrected = powf(normalized, BRIGHTNESS_GAMMA);
  uint8_t level =
      CONFIG_MIN_BRIGHTNESS +
      (uint8_t)((CONFIG_MAX_BRIGHTNESS - CONFIG_MIN_BRIGHTNESS) * corrected);

  if (s_prev_level < 0 ||
      abs((int)level - s_prev_level) >= CONFIG_BRIGHTNESS_HYSTERESIS) {
    waveshare_rgb_lcd_set_brightness(level);
    s_prev_level = level;
  }

  TickType_t now = xTaskGetTickCount();
  if (pdTICKS_TO_MS(now - s_last_activity_ticks) > INACTIVITY_TIMEOUT_MS) {
    esp_err_t slp_ret = esp_light_sleep_start();
    if (slp_ret != ESP_OK) {
      ESP_LOGW(TAG, "Light sleep failed: %s", esp_err_to_name(slp_ret));
    }
    pm_update_activity();
  }
}

// Fonction principale de l'application
void app_main(void) {
  bool init_failed = false;

  if (!init_peripherals()) {
    init_failed = true;
  } else {
    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };
#if CONFIG_PM_ENABLE
    esp_err_t err = esp_pm_configure(&pm_cfg);
    if (err == ESP_ERR_NOT_SUPPORTED) {
      ESP_LOGW(TAG, "PM non supporté, configuration ignorée");
    } else {
      ESP_ERROR_CHECK(err);
    }
#endif
    pm_update_activity();

    esp_err_t sd_ret = sd_mmc_init();
    if (sd_ret != ESP_OK) {
      ESP_LOGE(TAG, "sd_mmc_init a échoué : %s", esp_err_to_name(sd_ret));
      lv_obj_t *lbl = lv_label_create(lv_scr_act());
      lv_label_set_text(lbl, "Échec carte SD !");
      lv_obj_center(lbl);
      init_failed = true;
    } else {
      lvfs_fatfs_register('S');
      snprintf(g_base_path, sizeof(g_base_path), "%s", MOUNT_POINT);
      png_page_start = 0;
      if (list_files_sorted(g_base_path, png_page_start, PNG_LIST_INIT_CAP) ==
              ESP_OK &&
          png_list.size > 0) {
        ui_navigation_show_image(png_list.items[0]);
        draw_filename_bar(png_list.items[0]);
      }

      // Stop the temporary touch task and free its queue, but keep GT911
      // initialized so LVGL can continue polling the controller directly.
      touch_task_deinit();

      wifi_manager_register_callback(wifi_status_cb);

      const char *selected_dir = NULL;
      image_source_t img_src = IMAGE_SOURCE_LOCAL;
      int8_t index = 0;

      app_state_t state = APP_STATE_SOURCE_SELECTION;

      while (state != APP_STATE_EXIT) {
        switch (state) {
        case APP_STATE_SOURCE_SELECTION:
          wifi_manager_stop();
          stop_file_server();
          img_src = draw_source_selection();
          lv_obj_clean(lv_scr_act());
          if (img_src == IMAGE_SOURCE_REMOTE) {
            s_wifi_ready = false;
            s_wifi_failed = false;
            wifi_manager_start();
            int wait_ms = WIFI_CONNECT_TIMEOUT_MS;
          while (!s_wifi_ready && !s_wifi_failed && wait_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_ms -= 100;
          }
          if (!s_wifi_ready) {
            lv_obj_t *lbl = lv_label_create(lv_scr_act());
            lv_label_set_text(lbl, "Échec WiFi...");
            lv_obj_center(lbl);
            state = APP_STATE_ERROR;
            break;
          }
            image_fetch_http_to_sd(CONFIG_IMAGE_FETCH_URL,
                                   MOUNT_POINT "/remote.png");
            snprintf(g_base_path, sizeof(g_base_path), "%s", MOUNT_POINT);
            png_page_start = 0;
            esp_err_t err = list_files_sorted(g_base_path, png_page_start,
                                              PNG_LIST_INIT_CAP);
            if (err != ESP_OK || png_list.size == 0) {
              lv_obj_t *lbl = lv_label_create(lv_scr_act());
              lv_label_set_text(lbl, "Aucune image distante.");
              lv_obj_center(lbl);
              state = APP_STATE_ERROR;
            } else {
              ui_navigation_show_image(png_list.items[index]);
              draw_navigation_arrows();
              draw_filename_bar(png_list.items[index]);
              state = APP_STATE_NAVIGATION;
            }
          } else if (img_src == IMAGE_SOURCE_NETWORK) {
            s_wifi_ready = false;
            s_wifi_failed = false;
            wifi_manager_start();
            int wait_ms = WIFI_CONNECT_TIMEOUT_MS;
            while (!s_wifi_ready && !s_wifi_failed && wait_ms > 0) {
              vTaskDelay(pdMS_TO_TICKS(100));
              wait_ms -= 100;
            }
            esp_err_t err = start_file_server();
            if (!s_wifi_ready || s_wifi_failed || err != ESP_OK) {
              const char *msg = (!s_wifi_ready || s_wifi_failed)
                                     ? "Échec WiFi..."
                                     : "Échec serveur.";
              lv_obj_t *lbl = lv_label_create(lv_scr_act());
              lv_label_set_text(lbl, msg);
              lv_obj_center(lbl);
              wifi_manager_stop();
              stop_file_server();
              state = APP_STATE_SOURCE_SELECTION;
            } else {
              esp_netif_ip_info_t ip;
              esp_netif_t *netif =
                  esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
              if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
                char ip_str[IP4ADDR_STRLEN_MAX];
                ip4addr_ntoa_r((ip4_addr_t *)&ip.ip, ip_str, sizeof(ip_str));
                char url[32];
                snprintf(url, sizeof(url), "http://%s", ip_str);
                lv_obj_clean(lv_scr_act());
                lv_obj_t *l1 = lv_label_create(lv_scr_act());
                lv_label_set_text(l1, "Upload PNG via:");
                lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 0);
                lv_obj_t *l2 = lv_label_create(lv_scr_act());
                lv_label_set_text(l2, url);
                lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, 40);
                state = APP_STATE_NAVIGATION;
              }
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
          snprintf(g_base_path, sizeof(g_base_path), "%s/%s", MOUNT_POINT,
                   selected_dir);
          png_page_start = 0;
          esp_err_t err =
              list_files_sorted(g_base_path, png_page_start, PNG_LIST_INIT_CAP);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erreur lors du listage : %s", esp_err_to_name(err));
          }
          if (png_list.size == 0) {
            lv_obj_t *lbl = lv_label_create(lv_scr_act());
            lv_label_set_text(lbl, "Aucun fichier PNG dans ce dossier.");
            lv_obj_center(lbl);

            app_cleanup();
            state = APP_STATE_ERROR;
          } else {
            ui_navigation_show_image(png_list.items[index]);
            draw_navigation_arrows();
            draw_filename_bar(png_list.items[index]);
            state = APP_STATE_NAVIGATION;
          }
          free((void *)selected_dir);
          selected_dir = NULL;
          break;

        case APP_STATE_NAVIGATION: {
          nav_action_t act = handle_touch_navigation(&index);
          if (act == NAV_EXIT) {
            ui_navigation_deinit();
            state = APP_STATE_EXIT;
          } else if (act == NAV_HOME) {
            ui_navigation_deinit();
            png_list_free();
            index = 0;
            selected_dir = NULL;
            lv_obj_clean(lv_scr_act());
            state = APP_STATE_SOURCE_SELECTION;
          } else if (act == NAV_SCROLL) {
            if (index >= png_list.size)
              index = 0;
            else if (index < 0)
              index = png_list.size - 1;
            ui_navigation_show_image(png_list.items[index]);
            draw_filename_bar(png_list.items[index]);
          } else if (act == NAV_ROTATE) {
            display_set_orientation(!g_is_portrait);
            lv_obj_clean(lv_scr_act());
            ui_navigation_show_image(png_list.items[index]);
            draw_navigation_arrows();
            draw_filename_bar(png_list.items[index]);
            display_save_orientation();
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
        vTaskDelay(pdMS_TO_TICKS(CONFIG_BG_TASK_DELAY_MS));
      }
    }
  }

  app_cleanup();

  if (init_failed) {
    ESP_LOGE(TAG, "Initialisation échouée, redémarrage dans 5s");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
  }
  vTaskDelete(NULL); // Ne jamais retourner
}
