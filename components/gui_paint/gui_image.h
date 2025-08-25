/******************************************************************************
* | File       : gui_image.h
* | Author     : Waveshare team
* | Function   : Generic image loader interface
* | Info       : Detects file extension and dispatches to the appropriate reader
*
*----------------
* | Version:   V1.0
* | Date  :    2024-12-06
* | Info  :    Basic version
*
******************************************************************************/

#ifndef __GUI_IMAGE_H
#define __GUI_IMAGE_H

#include "gui_paint.h"

UBYTE GUI_ReadImage(UWORD Xstart, UWORD Ystart, const char *path);

#endif // __GUI_IMAGE_H
