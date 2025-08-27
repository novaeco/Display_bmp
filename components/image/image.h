#pragma once
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Charge un fichier BMP depuis la carte SD et produit un `lv_img_dsc_t`.
 * L’appelant est responsable de libérer `out_img->data` via `lv_mem_free()`.
 */
bool image_load_bmp(const char *path, lv_img_dsc_t *out_img);

#ifdef __cplusplus
}
#endif
