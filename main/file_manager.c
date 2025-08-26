#include "file_manager.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

bmp_list_t bmp_list = {0};
size_t bmp_page_start = 0;
bool bmp_has_more = false;
size_t bmp_last_page_size = 0;
DIR *bmp_dir = NULL;

static const char *TAG = "FILE_MANAGER";
static char s_base_path[PATH_MAX];

static esp_err_t bmp_list_append(bmp_list_t *list, char *path)
{
    if (list->size == list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : BMP_LIST_INIT_CAP;
        char **tmp = heap_caps_realloc(list->items, new_cap * sizeof(char *), MALLOC_CAP_DEFAULT);
        if (tmp == NULL) {
            return ESP_ERR_NO_MEM;
        }
        list->items = tmp;
        list->capacity = new_cap;
    }
    list->items[list->size++] = path;
    return ESP_OK;
}

static bool is_bmp(const char *name)
{
    size_t len = strlen(name);
    return (len > 4 && strcasecmp(&name[len - 4], ".bmp") == 0);
}

static void bmp_list_clear(void)
{
    for (size_t i = 0; i < bmp_list.size; ++i) {
        free(bmp_list.items[i]);
    }
    free(bmp_list.items);
    bmp_list.items = NULL;
    bmp_list.size = 0;
    bmp_list.capacity = 0;
}

static int bmp_path_cmp(const void *a, const void *b)
{
    const char *const *pa = a;
    const char *const *pb = b;
    return strcmp(*pa, *pb);
}

static void bmp_list_sort(void)
{
    if (bmp_list.size > 1) {
        qsort(bmp_list.items, bmp_list.size, sizeof(char *), bmp_path_cmp);
    }
}

static esp_err_t read_dir_page(size_t max_files)
{
    if (!bmp_dir) {
        return ESP_FAIL;
    }

    struct dirent *entry;
    esp_err_t ret = ESP_OK;
    while (bmp_list.size < max_files && (entry = readdir(bmp_dir)) != NULL) {
        if (!is_bmp(entry->d_name)) {
            continue;
        }
        size_t length = strlen(s_base_path) + strlen(entry->d_name) + 2;
        char *full_path = heap_caps_calloc(length, sizeof(char), MALLOC_CAP_DEFAULT);
        if (full_path == NULL) {
            ret = ESP_ERR_NO_MEM;
            break;
        }
        int written = snprintf(full_path, length, "%s/%s", s_base_path, entry->d_name);
        if (written < 0 || (size_t)written >= length) {
            free(full_path);
            ret = ESP_ERR_INVALID_SIZE;
            break;
        }
        ret = bmp_list_append(&bmp_list, full_path);
        if (ret != ESP_OK) {
            free(full_path);
            break;
        }
    }

    bmp_last_page_size = bmp_list.size;

    if (ret != ESP_OK) {
        return ret;
    }

    long pos = telldir(bmp_dir);
    bmp_has_more = false;
    while ((entry = readdir(bmp_dir)) != NULL) {
        if (is_bmp(entry->d_name)) {
            bmp_has_more = true;
            break;
        }
    }
    if (bmp_has_more) {
        seekdir(bmp_dir, pos);
    } else {
        closedir(bmp_dir);
        bmp_dir = NULL;
    }

    return ret;
}

esp_err_t list_files_sorted(const char *base_path, size_t start_idx, size_t max_files)
{
    bmp_list_free();

    bmp_dir = opendir(base_path);
    if (bmp_dir == NULL) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire : %s", base_path);
        return ESP_FAIL;
    }

    strncpy(s_base_path, base_path, sizeof(s_base_path) - 1);
    s_base_path[sizeof(s_base_path) - 1] = '\0';

    struct dirent *entry;
    size_t skipped = 0;
    while (skipped < start_idx && (entry = readdir(bmp_dir)) != NULL) {
        if (is_bmp(entry->d_name)) {
            skipped++;
        }
    }

    bmp_page_start = start_idx;
    esp_err_t ret = read_dir_page(max_files);
    if (ret == ESP_OK) {
        bmp_list_sort();
    }
    return ret;
}

esp_err_t file_manager_next_page(size_t max_files)
{
    if (!bmp_dir) {
        return ESP_FAIL;
    }
    size_t old_start = bmp_page_start;
    bmp_list_clear();
    bmp_page_start = old_start + bmp_last_page_size;
    esp_err_t ret = read_dir_page(max_files);
    if (ret == ESP_OK) {
        bmp_list_sort();
    }
    return ret;
}

void bmp_list_free(void)
{
    bmp_list_clear();
    if (bmp_dir) {
        closedir(bmp_dir);
        bmp_dir = NULL;
    }
    bmp_page_start = 0;
    bmp_last_page_size = 0;
    bmp_has_more = false;
}
