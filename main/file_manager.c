#include "file_manager.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

#define BMP_LIST_INIT_CAP 16

bmp_list_t bmp_list = {0};
size_t bmp_page_start = 0;
bool bmp_has_more = false;
size_t bmp_last_page_size = 0;

static const char *TAG = "FILE_MANAGER";

static int bmp_path_cmp(const void *a, const void *b)
{
    const char *const *pa = a;
    const char *const *pb = b;
    return strcasecmp(*pa, *pb);
}

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

esp_err_t list_files_sorted(const char *base_path, size_t start_idx)
{
    bmp_list_free();

    DIR *dir = opendir(base_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Impossible d'ouvrir le répertoire : %s", base_path);
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    char **names = NULL;
    size_t names_size = 0;
    size_t names_cap = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *file_name = entry->d_name;
        size_t len = strlen(file_name);
        if (len > 4 && strcasecmp(&file_name[len - 4], ".bmp") == 0) {
            if (names_size == names_cap) {
                size_t new_cap = names_cap ? names_cap * 2 : BMP_LIST_INIT_CAP;
                char **tmp = heap_caps_realloc(names, new_cap * sizeof(char *), MALLOC_CAP_DEFAULT);
                if (tmp == NULL) {
                    bmp_has_more = true;
                    ret = ESP_ERR_NO_MEM;
                    break;
                }
                names = tmp;
                names_cap = new_cap;
            }
            names[names_size] = heap_caps_calloc(len + 1, sizeof(char), MALLOC_CAP_DEFAULT);
            if (names[names_size] == NULL) {
                bmp_has_more = true;
                ret = ESP_ERR_NO_MEM;
                break;
            }
            strcpy(names[names_size], file_name);
            names_size++;
        }
    }
    closedir(dir);

    if (names_size == 0) {
        free(names);
        return ret;
    }

    qsort(names, names_size, sizeof(char *), bmp_path_cmp);

    bmp_page_start = start_idx;
    bmp_has_more = false;

    for (size_t i = start_idx; i < names_size; ++i) {
        size_t length = strlen(base_path) + strlen(names[i]) + 2;
        char *full_path = heap_caps_calloc(length, sizeof(char), MALLOC_CAP_DEFAULT);
        if (full_path == NULL) {
            bmp_has_more = true;
            ret = ESP_ERR_NO_MEM;
            break;
        }
        snprintf(full_path, length, "%s/%s", base_path, names[i]);
        esp_err_t app_ret = bmp_list_append(&bmp_list, full_path);
        if (app_ret != ESP_OK) {
            free(full_path);
            bmp_has_more = true;
            ret = app_ret;
            break;
        }
    }

    if (bmp_page_start + bmp_list.size < names_size) {
        bmp_has_more = true;
    }

    bmp_last_page_size = bmp_list.size;

    for (size_t i = 0; i < names_size; ++i) {
        free(names[i]);
    }
    free(names);

    return ret;
}

void bmp_list_free(void)
{
    for (size_t i = 0; i < bmp_list.size; ++i) {
        free(bmp_list.items[i]);
    }
    free(bmp_list.items);
    bmp_list.items = NULL;
    bmp_list.size = 0;
    bmp_list.capacity = 0;
    bmp_page_start = 0;
    bmp_last_page_size = 0;
    bmp_has_more = false;
}
