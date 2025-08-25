#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stddef.h>
#include <stdbool.h>
#include <dirent.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char **items;
    size_t size;
    size_t capacity;
} bmp_list_t;

extern bmp_list_t bmp_list;
extern size_t bmp_page_start;
extern bool bmp_has_more;
extern size_t bmp_last_page_size;
extern DIR *bmp_dir;

#define BMP_LIST_INIT_CAP 16

void bmp_list_free(void);
esp_err_t list_files_sorted(const char *base_path, size_t start_idx, size_t max_files);
esp_err_t file_manager_next_page(size_t max_files);

#ifdef __cplusplus
}
#endif

#endif // FILE_MANAGER_H
