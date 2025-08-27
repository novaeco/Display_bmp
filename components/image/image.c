#include "image.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "image"

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} bmp_file_header_t;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmp_info_header_t;
#pragma pack(pop)

bool image_load_bmp(const char *path, lv_image_dsc_t *out_img)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "unable to open %s", path);
        return false;
    }

    bmp_file_header_t fh;
    bmp_info_header_t ih;
    bool success = false;
    uint8_t *frame = NULL;
    uint8_t *row = NULL;

    do {
        if (fread(&fh, sizeof(fh), 1, f) != 1) break;
        if (fread(&ih, sizeof(ih), 1, f) != 1) break;
        if (fh.bfType != 0x4D42 || ih.biCompression != 0) break;
        if (ih.biBitCount != 24 && ih.biBitCount != 32) break;

        size_t width  = ih.biWidth;
        size_t height = llabs(ih.biHeight);
        size_t row_bytes    = ((ih.biBitCount * width + 31) / 32) * 4;
        size_t bytes_pp     = ih.biBitCount / 8;
        size_t buffer_bytes = width * height * 2;

        frame = lv_malloc(buffer_bytes);
        if (!frame) break;
        row = malloc(row_bytes);
        if (!row) break;

        bool bottom_up = (ih.biHeight > 0);
        for (size_t y = 0; y < height; y++) {
            size_t src_row = bottom_up ? (height - 1 - y) : y;
            fseek(f, fh.bfOffBits + src_row * row_bytes, SEEK_SET);
            if (fread(row, 1, row_bytes, f) != row_bytes) break;

            uint8_t *dst = frame + y * width * 2;
            for (size_t x = 0; x < width; x++) {
                uint8_t b = row[x * bytes_pp + 0];
                uint8_t g = row[x * bytes_pp + 1];
                uint8_t r = row[x * bytes_pp + 2];
                uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                dst[2 * x + 0] = rgb565 & 0xFF;
                dst[2 * x + 1] = rgb565 >> 8;
            }
        }

        free(row);
        row = NULL;

        out_img->header.w  = width;
        out_img->header.h  = height;
        out_img->header.cf = LV_COLOR_FORMAT_RGB565;
        out_img->data      = frame;
        out_img->data_size = buffer_bytes;

        success = true;
    } while (0);

    if (!success) {
        ESP_LOGE(TAG, "failed to decode %s", path);
        lv_free(frame);
        free(row);
    }

    fclose(f);
    return success;
}
