/******************************************************************************
* | File       : gui_image.c
* | Author     : Waveshare team
* | Function   : Image format dispatcher
* | Info       : Detects image file extension and invokes format-specific reader
*
*----------------
* | Version:   V1.0
* | Date  :    2024-12-06
* | Info  :    Basic version
*
******************************************************************************/

#include "gui_image.h"
#include "gui_bmp.h"
#include "gui_jpg.h"
#include <string.h>
#include <strings.h>

UBYTE GUI_ReadPng(UWORD Xstart, UWORD Ystart, const char *path);

UBYTE GUI_ReadImage(UWORD Xstart, UWORD Ystart, const char *path)
{
    if (path == NULL) {
        Debug("GUI_ReadImage: path is NULL\n");
        return 0;
    }
    const char *ext = strrchr(path, '.');
    if (ext == NULL) {
        Debug("GUI_ReadImage: no file extension for %s\n", path);
        return 0;
    }
    ext++; // skip '.'
    if (strcasecmp(ext, "bmp") == 0) {
        return GUI_ReadBmp(Xstart, Ystart, path);
    } else if (strcasecmp(ext, "png") == 0) {
        return GUI_ReadPng(Xstart, Ystart, path);
    } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
        return GUI_ReadJpg(Xstart, Ystart, path);
    } else {
        Debug("GUI_ReadImage: unsupported extension %s\n", ext);
        return 0;
    }
}

UBYTE __attribute__((weak)) GUI_ReadPng(UWORD Xstart, UWORD Ystart, const char *path)
{
    Debug("GUI_ReadPng not implemented\n");
    return 0;
}

