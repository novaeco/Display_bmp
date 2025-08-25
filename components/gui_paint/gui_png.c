#include "gui_png.h"
#include "pngle.h"
#include "gui_bmp.h" // for RGB macro
#include "Debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    UWORD x;
    UWORD y;
    UWORD width;
    UWORD height;
    UWORD cur_row;
    UWORD *rowbuf;
    uint16_t bg;
    int err;
} PNGCTX;

static void png_init(pngle_t *pngle, uint32_t w, uint32_t h) {
    PNGCTX *ctx = (PNGCTX *)pngle_get_user_data(pngle);
    ctx->width = w;
    ctx->height = h;
    ctx->cur_row = 0;
    ctx->bg = WHITE;
    if (ctx->x + w > Paint.Width || ctx->y + h > Paint.Height) {
        Debug("GUI_ReadPng: image too big %u x %u\n", w, h);
        ctx->err = 1;
        return;
    }
    ctx->rowbuf = (UWORD *)malloc(w * sizeof(UWORD));
    if (!ctx->rowbuf) {
        Debug("GUI_ReadPng: row buffer alloc failed\n");
        ctx->err = 1;
    }
}

static void flush_row(PNGCTX *ctx) {
    if (!ctx->rowbuf) return;
    UBYTE *dst = Paint.Image + (ctx->y + ctx->cur_row) * Paint.WidthByte + ctx->x * 2;
    memcpy(dst, ctx->rowbuf, ctx->width * 2);
}

static void png_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]) {
    PNGCTX *ctx = (PNGCTX *)pngle_get_user_data(pngle);
    if (ctx->err) return;
    for (uint32_t iy = 0; iy < h; iy++) {
        uint32_t ay = y + iy;
        if (ay != ctx->cur_row) {
            flush_row(ctx);
            ctx->cur_row = ay;
        }
        for (uint32_t ix = 0; ix < w; ix++) {
            uint32_t ax = x + ix;
            uint8_t r = rgba[0];
            uint8_t g = rgba[1];
            uint8_t b = rgba[2];
            uint8_t a = rgba[3];
            if (a < 255) {
                uint8_t br = (ctx->bg >> 11) & 0x1F;
                uint8_t bg = (ctx->bg >> 5) & 0x3F;
                uint8_t bb = ctx->bg & 0x1F;
                r = (r * a + (br << 3) * (255 - a)) / 255;
                g = (g * a + (bg << 2) * (255 - a)) / 255;
                b = (b * a + (bb << 3) * (255 - a)) / 255;
            }
            ctx->rowbuf[ax] = RGB(r, g, b);
        }
    }
}

static void png_done(pngle_t *pngle) {
    PNGCTX *ctx = (PNGCTX *)pngle_get_user_data(pngle);
    if (ctx->err) return;
    flush_row(ctx);
}

UBYTE GUI_ReadPng(UWORD Xstart, UWORD Ystart, const char *path) {
    if (!path) {
        Debug("GUI_ReadPng: path is NULL\n");
        return 0;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        Debug("GUI_ReadPng: Cannot open %s\n", path);
        return 0;
    }
    pngle_t *pngle = pngle_new();
    if (!pngle) {
        fclose(fp);
        return 0;
    }
    PNGCTX ctx = {
        .x = Xstart,
        .y = Ystart,
        .rowbuf = NULL,
        .bg = WHITE,
        .err = 0,
    };
    pngle_set_user_data(pngle, &ctx);
    pngle_set_init_callback(pngle, png_init);
    pngle_set_draw_callback(pngle, png_draw);
    pngle_set_done_callback(pngle, png_done);

    char buf[1024];
    size_t remain = 0;
    int len;
    while (!feof(fp)) {
        len = fread(buf + remain, 1, sizeof(buf) - remain, fp);
        if (len <= 0) break;
        int fed = pngle_feed(pngle, buf, remain + len);
        if (fed < 0) {
            Debug("GUI_ReadPng: %s\n", pngle_error(pngle));
            ctx.err = 1;
            break;
        }
        remain = remain + len - fed;
        if (remain > 0) memmove(buf, buf + fed, remain);
    }

    if (ctx.rowbuf) free(ctx.rowbuf);
    pngle_destroy(pngle);
    fclose(fp);
    return ctx.err ? 0 : 1;
}

