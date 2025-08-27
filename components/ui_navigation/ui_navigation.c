#include "ui_navigation.h"
#include "battery.h"
#include "config.h"
#include "esp_log.h"
#include "file_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "sd.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern display_geometry_t g_display;
extern char g_base_path[];

static const char *s_folder_choice = NULL;
static void folder_label_cb(lv_event_t *e) {
  s_folder_choice = (const char *)lv_event_get_user_data(e);
}

static bool is_folder_excluded(const char *name) {
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


const char *draw_folder_selection(void) {
  uint16_t text_x = g_display.width / TEXT_X_DIVISOR;
  uint16_t text_y1 = g_display.height / TEXT_Y1_DIVISOR;
  uint16_t text_y2 = text_y1 + TEXT_LINE_SPACING;

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
    bool has_png = false;
    struct dirent *e2;
    while ((e2 = readdir(sub)) != NULL) {
      if (e2->d_type != DT_REG) {
        continue;
      }
      const char *ext = strrchr(e2->d_name, '.');
      if (ext && strcasecmp(ext, ".png") == 0) {
        has_png = true;
        break;
      }
    }
    closedir(sub);
    if (!has_png) {
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

  uint16_t list_y = text_y2 + TEXT_LINE_SPACING;
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

  lv_scr_load(NULL); // unload selection screen to avoid it remaining active
  lv_obj_del(scr);   // delete screen object to prevent RAM accumulation

  for (size_t i = 0; i < fl.count; ++i) {
    if (fl.names[i] != selected_dir) {
      free(fl.names[i]);
    }
  }
  free(fl.names);
  free(fl.labels);
  s_folder_choice = NULL;

  return selected_dir;
}

static QueueHandle_t s_nav_queue;
static volatile int s_src_choice = -1;
static lv_obj_t *s_fname_bar = NULL;
static lv_obj_t *s_fname_label = NULL;
static lv_obj_t *s_main_img = NULL;

static void source_btn_cb(lv_event_t *e) {
  s_src_choice = (int)lv_event_get_user_data(e);
}

static void nav_btn_cb(lv_event_t *e) {
  nav_cmd_t cmd = (nav_cmd_t)(intptr_t)lv_event_get_user_data(e);
  if (s_nav_queue) {
    xQueueSend(s_nav_queue, &cmd, 0);
  }
}

image_source_t draw_source_selection(void) {
  s_src_choice = -1;
  lv_obj_t *scr = lv_obj_create(NULL);

  uint16_t btnL_x0 = g_display.margin_left;
  uint16_t btnL_y0 = (g_display.height - BTN_HEIGHT) / 2;
  uint16_t btnR_x1 = g_display.width - g_display.margin_right;
  uint16_t btnR_x0 = btnR_x1 - BTN_WIDTH;
  uint16_t btnR_y0 = btnL_y0;
  uint16_t btnN_x0 = (g_display.width - BTN_WIDTH) / 2;
  uint16_t btnN_y0 = btnL_y0 + BTN_HEIGHT + NAV_MARGIN;

  lv_obj_t *btn_local = lv_btn_create(scr);
  lv_obj_set_size(btn_local, BTN_WIDTH, BTN_HEIGHT);
  lv_obj_set_pos(btn_local, btnL_x0, btnL_y0);
  lv_obj_add_event_cb(btn_local, source_btn_cb, LV_EVENT_CLICKED,
                      (void *)IMAGE_SOURCE_LOCAL);
  lv_obj_t *lbl_local = lv_label_create(btn_local);
  lv_label_set_text(lbl_local, "Locales");

  lv_obj_t *btn_remote = lv_btn_create(scr);
  lv_obj_set_size(btn_remote, BTN_WIDTH, BTN_HEIGHT);
  lv_obj_set_pos(btn_remote, btnR_x0, btnR_y0);
  lv_obj_add_event_cb(btn_remote, source_btn_cb, LV_EVENT_CLICKED,
                      (void *)IMAGE_SOURCE_REMOTE);
  lv_obj_t *lbl_remote = lv_label_create(btn_remote);
  lv_label_set_text(lbl_remote, "Distantes");

  lv_obj_t *btn_net = lv_btn_create(scr);
  lv_obj_set_size(btn_net, BTN_WIDTH, BTN_HEIGHT);
  lv_obj_set_pos(btn_net, btnN_x0, btnN_y0);
  lv_obj_add_event_cb(btn_net, source_btn_cb, LV_EVENT_CLICKED,
                      (void *)IMAGE_SOURCE_NETWORK);
  lv_obj_t *lbl_net = lv_label_create(btn_net);
  lv_label_set_text(lbl_net, "Source reseau");

  lv_scr_load(scr);
  while (s_src_choice == -1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  lv_scr_load(NULL); // unload selection screen to avoid it remaining active
  lv_obj_del(scr);   // delete screen object to prevent RAM accumulation
  return (image_source_t)s_src_choice;
}

static void add_btn_img_or_label(lv_obj_t *btn, const char *img_path,
                                 const char *fallback) {
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

void draw_navigation_arrows(void) {
  if (!s_nav_queue) {
    s_nav_queue = xQueueCreate(5, sizeof(nav_cmd_t));
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
  lv_obj_set_pos(btn_left, g_display.margin_left,
                 (g_display.height - ARROW_HEIGHT) / 2);
  lv_obj_add_event_cb(btn_left, nav_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)NAV_CMD_PREV);
  add_btn_img_or_label(btn_left, MOUNT_POINT "/pic/arrow_left.png", "<");

  lv_obj_t *btn_right = lv_btn_create(scr);
  lv_obj_set_size(btn_right, ARROW_WIDTH, ARROW_HEIGHT);
  lv_obj_set_pos(btn_right,
                 g_display.width - g_display.margin_right - ARROW_WIDTH,
                 (g_display.height - ARROW_HEIGHT) / 2);
  lv_obj_add_event_cb(btn_right, nav_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)NAV_CMD_NEXT);
  add_btn_img_or_label(btn_right, MOUNT_POINT "/pic/arrow_right.png", ">");

  lv_obj_t *btn_rotate = lv_btn_create(scr);
  lv_obj_set_size(btn_rotate, 100, 40);
  lv_obj_set_pos(btn_rotate, (g_display.width - 100) / 2, g_display.margin_top);
  lv_obj_add_event_cb(btn_rotate, nav_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)NAV_CMD_ROTATE);
  add_btn_img_or_label(btn_rotate, MOUNT_POINT "/pic/rotate.png", "Rotation");

  lv_obj_t *btn_home = lv_btn_create(scr);
  lv_obj_set_size(btn_home, 100, 40);
  lv_obj_set_pos(btn_home, g_display.margin_left,
                 g_display.height - g_display.margin_bottom - 40);
  lv_obj_add_event_cb(btn_home, nav_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)NAV_CMD_HOME);
  add_btn_img_or_label(btn_home, MOUNT_POINT "/pic/home.png", "Home");

  lv_obj_t *btn_exit = lv_btn_create(scr);
  lv_obj_set_size(btn_exit, 100, 40);
  lv_obj_set_pos(btn_exit, g_display.width - g_display.margin_right - 100,
                 g_display.height - g_display.margin_bottom - 40);
  lv_obj_add_event_cb(btn_exit, nav_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)NAV_CMD_EXIT);
  add_btn_img_or_label(btn_exit, MOUNT_POINT "/pic/bluetooth.png", "Exit");
}

nav_action_t handle_touch_navigation(int8_t *idx) {
  nav_cmd_t cmd;
  if (s_nav_queue &&
      xQueueReceive(s_nav_queue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (cmd == NAV_CMD_ROTATE) {
      if (png_list.size > 0) {
        draw_filename_bar(png_list.items[*idx]);
      }
      return NAV_ROTATE;
    }
    if (cmd == NAV_CMD_HOME) {
      return NAV_HOME;
    }
    if (cmd == NAV_CMD_EXIT) {
      return NAV_EXIT;
    }
    if (cmd == NAV_CMD_NEXT || cmd == NAV_CMD_PREV) {
      if (png_list.size == 0) {
        return NAV_NONE;
      }
      *idx += (int8_t)cmd;
      if (*idx >= (int8_t)png_list.size) {
        *idx = 0;
      } else if (*idx < 0) {
        *idx = (int8_t)png_list.size - 1;
      }
      draw_filename_bar(png_list.items[*idx]);
      return NAV_SCROLL;
    }
  }
  return NAV_NONE;
}

void draw_filename_bar(const char *path) {
  const char *fname = strrchr(path, '/');
  fname = fname ? fname + 1 : path;

  const lv_font_t *font = LV_FONT_DEFAULT;
  lv_coord_t bar_h = font->line_height + 2 * FILENAME_BAR_PAD;

  if (!s_fname_bar || !lv_obj_is_valid(s_fname_bar)) {
    s_fname_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(s_fname_bar, lv_color_hex(0x808080),
                              LV_PART_MAIN);
    lv_obj_set_style_border_width(s_fname_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_fname_bar, FILENAME_BAR_PAD, LV_PART_MAIN);
    s_fname_label = lv_label_create(s_fname_bar);
    lv_obj_set_style_text_color(s_fname_label, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN);
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

void ui_navigation_show_image(const char *path) {
  if (!s_main_img || !lv_obj_is_valid(s_main_img)) {
    s_main_img = lv_img_create(lv_scr_act());
  }
  lv_img_set_src(s_main_img, path);
  lv_obj_center(s_main_img);
  if (g_is_portrait) {
    lv_obj_set_style_transform_angle(s_main_img, 900, LV_PART_MAIN);
  } else {
    lv_obj_set_style_transform_angle(s_main_img, 0, LV_PART_MAIN);
  }
}

void ui_navigation_deinit(void) {
  if (s_nav_queue) {
    vQueueDelete(s_nav_queue);
    s_nav_queue = NULL;
  }

  if (s_fname_bar) {
    lv_obj_del(s_fname_bar);
  }
  s_fname_bar = NULL;
  s_fname_label = NULL;
  if (s_main_img) {
    lv_obj_del(s_main_img);
  }
  s_main_img = NULL;
}
