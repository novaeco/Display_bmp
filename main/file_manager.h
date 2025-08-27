#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "esp_err.h"
#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char **items;
  size_t size;
  size_t capacity;
} png_list_t;

extern png_list_t png_list;
extern size_t png_page_start;
extern bool png_has_more;
extern size_t png_last_page_size;
extern DIR *png_dir;

#define PNG_LIST_INIT_CAP 16

void png_list_free(void);
/**
 * @brief Load a page of PNG files sorted alphabetically.
 *
 * Populate ::png_list with up to @p max_files entries starting from @p
 * start_idx within the directory located at @p base_path. The resulting list is
 * sorted using @c strcmp for deterministic, case-sensitive ordering.
 */
esp_err_t list_files_sorted(const char *base_path, size_t start_idx,
                            size_t max_files);
esp_err_t file_manager_next_page(size_t max_files);

#ifdef __cplusplus
}
#endif

#endif // FILE_MANAGER_H
