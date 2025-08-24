/*****************************************************************************
* | File      	:   BMP_APP.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                   The bmp picture is read from the SD card and drawn into the buffer
*                
*----------------
* |	This version:   V1.0
* | Date        :   2024-12-06
* | Info        :   Basic version
*
******************************************************************************/ 
#include "gui_bmp.h"
#include <string.h>
#include "esp_timer.h"

// Global palette storage populated when reading the BMP file
static RGBQUAD RGBPAD[256];

// Convert one row of an 8bpp paletted BMP to RGB565
static void ConvertRow8To565(UWORD *dst, const UBYTE *src, int width)
{
    for (int i = 0; i < width; i++) {
        const RGBQUAD *c = &RGBPAD[src[i]];
        dst[i] = RGB(c->rgbRed, c->rgbGreen, c->rgbBlue);
    }
}

// Convert one row of a 16bpp BMP to RGB565
static void ConvertRow16To565(UWORD *dst, const UBYTE *src, int width, const BMPINF *hdr)
{
    const UWORD *s = (const UWORD *)src;
    if (hdr->bInfoSize == 0x38) {
        memcpy(dst, s, width * sizeof(UWORD));
    } else if ((hdr->bInfoSize == 0x28) && (hdr->bCompression == 0x00)) {
        for (int i = 0; i < width; i++) {
            UWORD pixel = s[i];
            UWORD r = (pixel >> 10) & 0x1F;
            UWORD g = (pixel >> 5) & 0x1F;
            UWORD b = pixel & 0x1F;
            dst[i] = (r << 11) | (((g << 1) | (g >> 4)) << 5) | b;
        }
    }
}

// Convert one row of a 24bpp BMP to RGB565
static void ConvertRow24To565(UWORD *dst, const UBYTE *src, int width)
{
    for (int i = 0; i < width; i++) {
        const UBYTE *p = src + i * 3;
        dst[i] = RGB(p[2], p[1], p[0]);
    }
}

// Function to extract pixel color based on the bit depth of the BMP image
UWORD ExtractPixelColor(UBYTE *row_data, int col, int bBitCount, BMPINF *bmpInfoHeader) {
    UWORD color = 0;
    
    switch (bBitCount) {
        case 1: {  // 1 bit per pixel (black and white)
            int byte_offset = col / 8;   // 1 byte for every 8 pixels
            int bit_offset = 7 - (col % 8); // High bit first
            UBYTE index = (row_data[byte_offset] >> bit_offset) & 0x01;
            color = RGB(RGBPAD[index].rgbRed, RGBPAD[index].rgbGreen, RGBPAD[index].rgbBlue);
            break;
        }
        case 4: {  // 4 bits per pixel (16 colors)
            int byte_offset = col / 2;   // 1 byte for every 2 pixels
            int nibble_offset = (col % 2 == 0) ? 4 : 0; // High nibble or low nibble
            UBYTE index = (row_data[byte_offset] >> nibble_offset) & 0x0F;
            color = RGB(RGBPAD[index].rgbRed, RGBPAD[index].rgbGreen, RGBPAD[index].rgbBlue);
            break;
        }
        case 8: {  // 8 bits per pixel (256 colors)
            UBYTE index = row_data[col];
            color = RGB(RGBPAD[index].rgbRed, RGBPAD[index].rgbGreen, RGBPAD[index].rgbBlue);
            break;
        }
        case 16: { // 16 bits per pixel (RGB565 or XRGB1555)
            UWORD pixel = ((UWORD *)row_data)[col];
            if (bmpInfoHeader->bInfoSize == 0x38) { // RGB565 format
                color = pixel;
            } else if ((bmpInfoHeader->bInfoSize == 0x28) && (bmpInfoHeader->bCompression == 0x00)) { // XRGB1555 format
                color = ((((pixel >> 10) & 0x1F) * 0x1F / 0x1F) << 11) |
                        ((((pixel >> 5) & 0x1F) * 0x3F / 0x1F) << 5) |
                        ((pixel & 0x1F) * 0x1F / 0x1F);
            }
            break;
        }
        case 24: { // 24 bits per pixel (RGB888)
            int byte_offset = col * 3;
            UBYTE blue = row_data[byte_offset];
            UBYTE green = row_data[byte_offset + 1];
            UBYTE red = row_data[byte_offset + 2];
            color = RGB(red, green, blue);
            break;
        }
        case 32: { // 32 bits per pixel (ARGB8888 or XRGB8888)
            int byte_offset = col * 4;
            UBYTE blue = row_data[byte_offset];
            UBYTE green = row_data[byte_offset + 1];
            UBYTE red = row_data[byte_offset + 2];
            // Ignore the Alpha channel, or process it if necessary
            color = RGB(red, green, blue);
            break;
        }
        default:
            printf("Unsupported bBitCount: %d\n", bBitCount);  // Print an error message for unsupported bit depths
            break;
    }
    
    return color;
}

// Function to read and display BMP image from file
UBYTE GUI_ReadBmp(UWORD Xstart, UWORD Ystart, const char *path) {
    FILE *fp;
    
    // Open the BMP file for reading
    if ((fp = fopen(path, "rb")) == NULL) {
        Debug("Cannot open the file: %s\n", path);  // Print error if file can't be opened
        return 0;
    }
    printf("open: %s\n", path);  // Print the file path
    
    // Read and parse BMP headers
    BMPFILEHEADER bmpFileHeader;
    BMPINF bmpInfoHeader;
    fread(&bmpFileHeader, sizeof(BMPFILEHEADER), 1, fp);
    fread(&bmpInfoHeader, sizeof(BMPINF), 1, fp);

    // Read the color palette if present
    if (bmpInfoHeader.bBitCount <= 8) {
        uint32_t palette_count = bmpInfoHeader.bClrUsed ? bmpInfoHeader.bClrUsed : (1U << bmpInfoHeader.bBitCount);
        if (palette_count > 256) {
            palette_count = 256;
        }
        fread(RGBPAD, sizeof(RGBQUAD), palette_count, fp);
    }

    // Compute the row size and allocate a buffer for one row
    int row_bytes = ((bmpInfoHeader.bWidth * bmpInfoHeader.bBitCount + 31) / 32) * 4;
    UBYTE *row_buffer = malloc(row_bytes);
    if (!row_buffer) {
        Debug("Memory allocation failed\n");
        fclose(fp);
        return 0;
    }

    printf("bBitCount = %d\n", bmpInfoHeader.bBitCount);  // Print the number of bits per pixel

    // Seek to the beginning of the pixel data
    fseek(fp, bmpFileHeader.bOffset, SEEK_SET);

    UWORD *row_565 = malloc(bmpInfoHeader.bWidth * sizeof(UWORD));
    if (!row_565) {
        free(row_buffer);
        fclose(fp);
        return 0;
    }

    int64_t start_time = esp_timer_get_time();

    for (int row = 0; row < bmpInfoHeader.bHeight; row++) {
        if (fread(row_buffer, row_bytes, 1, fp) != 1) {
            free(row_buffer);
            free(row_565);
            fclose(fp);
            return 0;
        }

        switch (bmpInfoHeader.bBitCount) {
        case 8:
            ConvertRow8To565(row_565, row_buffer, bmpInfoHeader.bWidth);
            break;
        case 16:
            ConvertRow16To565(row_565, row_buffer, bmpInfoHeader.bWidth, &bmpInfoHeader);
            break;
        case 24:
            ConvertRow24To565(row_565, row_buffer, bmpInfoHeader.bWidth);
            break;
        default:
            for (int col = 0; col < bmpInfoHeader.bWidth; col++) {
                row_565[col] = ExtractPixelColor(row_buffer, col, bmpInfoHeader.bBitCount, &bmpInfoHeader);
            }
            break;
        }

        UBYTE *dst = Paint.Image +
                     (Ystart + bmpInfoHeader.bHeight - row - 1) * Paint.WidthByte +
                     Xstart * 2;
        memcpy(dst, row_565, bmpInfoHeader.bWidth * sizeof(UWORD));
    }

    int64_t end_time = esp_timer_get_time();
    printf("GUI_ReadBmp render time: %lld us\n", (long long)(end_time - start_time));

    free(row_buffer);
    free(row_565);
    fclose(fp);
    return 1;  // Return success
}
