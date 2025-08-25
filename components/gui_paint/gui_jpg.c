#include "gui_jpg.h"
#include "tjpgd.h"
#include "Debug.h"
#include "esp_timer.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Context structure passed to decoder callbacks
typedef struct {
    FILE *fp;
    UWORD x;
    UWORD y;
    UWORD width;
    UWORD row[LCD_H_RES];
} JPGCTX;

static size_t jpg_in_func(JDEC *jd, uint8_t *buf, size_t len) {
    JPGCTX *ctx = (JPGCTX *)jd->device;
    if (buf) {
        return fread(buf, 1, len, ctx->fp);
    } else {
        // skip bytes
        fseek(ctx->fp, len, SEEK_CUR);
        return len;
    }
}

static int jpg_out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    JPGCTX *ctx = (JPGCTX *)jd->device;
    uint16_t *src = (uint16_t *)bitmap;
    uint32_t w = rect->right - rect->left + 1;
    for (uint32_t y = rect->top; y <= rect->bottom; y++) {
        memcpy(ctx->row + rect->left, src, w * 2);
        src += w;
        if (rect->right + 1 == ctx->width) {
            UBYTE *dst = Paint.Image + (ctx->y + y) * Paint.WidthByte + ctx->x * 2;
            memcpy(dst, ctx->row, ctx->width * 2);
        }
    }
    return 1;
}

UBYTE GUI_ReadJpg(UWORD Xstart, UWORD Ystart, const char *path) {
    if (!path) {
        Debug("GUI_ReadJpg: path is NULL\n");
        return JPG_ERR_OPEN;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        Debug("GUI_ReadJpg: Cannot open %s\n", path);
        return JPG_ERR_OPEN;
    }

    void *work = malloc(TJPGD_WORKSPACE_SIZE);
    if (!work) {
        fclose(fp);
        Debug("GUI_ReadJpg: workspace alloc failed\n");
        return JPG_ERR_OPEN;
    }

    JPGCTX ctx = {
        .fp = fp,
        .x = Xstart,
        .y = Ystart,
        .width = 0,
    };
    JDEC jd;
    JRESULT res = jd_prepare(&jd, jpg_in_func, work, TJPGD_WORKSPACE_SIZE, &ctx);
    if (res != JDR_OK) {
        Debug("GUI_ReadJpg: jd_prepare failed %d\n", res);
        free(work);
        fclose(fp);
        return JPG_ERR_FORMAT;
    }

    ctx.width = jd.width;
    if (jd.width > LCD_H_RES || jd.width > Paint.Width || jd.height > Paint.Height ||
        Xstart + jd.width > Paint.Width || Ystart + jd.height > Paint.Height) {
        Debug("GUI_ReadJpg: image too big %u x %u\n", jd.width, jd.height);
        free(work);
        fclose(fp);
        return JPG_ERR_TOOBIG;
    }

    int64_t start_time = esp_timer_get_time();
    res = jd_decomp(&jd, jpg_out_func, 0); // scale = 0 (1/1)
    int64_t end_time = esp_timer_get_time();
    free(work);
    fclose(fp);
    printf("GUI_ReadJpg render time: %lld us\n", (long long)(end_time - start_time));
    if (res != JDR_OK) {
        Debug("GUI_ReadJpg: jd_decomp failed %d\n", res);
        return JPG_ERR_FORMAT;
    }
    return JPG_OK;
}

