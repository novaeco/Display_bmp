#include "file_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

png_list_t png_list = {0};
size_t png_page_start = 0;
bool png_has_more = false;
size_t png_last_page_size = 0;
DIR *png_dir = NULL;

static const char *TAG = "FILE_MANAGER";
static char s_base_path[PATH_MAX];

static esp_err_t png_list_append(png_list_t *list, char *path) {
  if (list->size == list->capacity) {
    size_t new_cap = list->capacity ? list->capacity * 2 : PNG_LIST_INIT_CAP;
    char **tmp = heap_caps_realloc(list->items, new_cap * sizeof(char *),
                                   MALLOC_CAP_DEFAULT);
    if (tmp == NULL) {
      return ESP_ERR_NO_MEM;
    }
    list->items = tmp;
    list->capacity = new_cap;
  }
  list->items[list->size++] = path;
  return ESP_OK;
}

static bool is_png(const char *name) {
  const char *ext = strrchr(name, '.');
  return ext && strcasecmp(ext, ".png") == 0;
}

static void png_list_clear(void) {
  for (size_t i = 0; i < png_list.size; ++i) {
    free(png_list.items[i]);
  }
  free(png_list.items);
  png_list.items = NULL;
  png_list.size = 0;
  png_list.capacity = 0;
}

static int png_path_cmp(const void *a, const void *b) {
  const char *const *pa = a;
  const char *const *pb = b;
  return strcmp(*pa, *pb);
}

static void png_list_sort(void) {
  if (png_list.size > 1) {
    qsort(png_list.items, png_list.size, sizeof(char *), png_path_cmp);
  }
}

static esp_err_t read_dir_page(size_t max_files) {
  if (!png_dir) {
    return ESP_FAIL;
  }

  struct dirent *entry;
  esp_err_t ret = ESP_OK;
  while (png_list.size < max_files && (entry = readdir(png_dir)) != NULL) {
    if (!is_png(entry->d_name)) {
      continue;
    }
    size_t length = strlen(s_base_path) + strlen(entry->d_name) + 2;
    char *full_path =
        heap_caps_calloc(length, sizeof(char), MALLOC_CAP_DEFAULT);
    if (full_path == NULL) {
      ret = ESP_ERR_NO_MEM;
      break;
    }
    int written =
        snprintf(full_path, length, "%s/%s", s_base_path, entry->d_name);
    if (written < 0 || (size_t)written >= length) {
      free(full_path);
      ret = ESP_ERR_INVALID_SIZE;
      break;
    }
    ret = png_list_append(&png_list, full_path);
    if (ret != ESP_OK) {
      free(full_path);
      break;
    }
  }

  png_last_page_size = png_list.size;

  if (ret != ESP_OK) {
    return ret;
  }

  long pos = telldir(png_dir);
  png_has_more = false;
  while ((entry = readdir(png_dir)) != NULL) {
    if (is_png(entry->d_name)) {
      png_has_more = true;
      break;
    }
  }
  if (png_has_more) {
    seekdir(png_dir, pos);
  } else {
    closedir(png_dir);
    png_dir = NULL;
  }

  return ret;
}

esp_err_t list_files_sorted(const char *base_path, size_t start_idx,
                            size_t max_files) {
  png_list_free();

  png_dir = opendir(base_path);
  if (png_dir == NULL) {
    ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire : %s", base_path);
    return ESP_FAIL;
  }

  strncpy(s_base_path, base_path, sizeof(s_base_path) - 1);
  s_base_path[sizeof(s_base_path) - 1] = '\0';

  struct dirent *entry;
  size_t skipped = 0;
  while (skipped < start_idx && (entry = readdir(png_dir)) != NULL) {
    if (is_png(entry->d_name)) {
      skipped++;
    }
  }

  png_page_start = start_idx;
  esp_err_t ret = read_dir_page(max_files);
  if (ret == ESP_OK) {
    png_list_sort();
  }
  return ret;
}

esp_err_t file_manager_next_page(size_t max_files) {
  if (!png_dir) {
    return ESP_FAIL;
  }
  size_t old_start = png_page_start;
  png_list_clear();
  png_page_start = old_start + png_last_page_size;
  esp_err_t ret = read_dir_page(max_files);
  if (ret == ESP_OK) {
    png_list_sort();
  }
  return ret;
}

void png_list_free(void) {
  png_list_clear();
  if (png_dir) {
    closedir(png_dir);
    png_dir = NULL;
  }
  png_page_start = 0;
  png_last_page_size = 0;
  png_has_more = false;
}
