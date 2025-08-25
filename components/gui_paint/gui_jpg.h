#ifndef __GUI_JPG_H
#define __GUI_JPG_H

#include "gui_paint.h"

#define JPG_OK            1
#define JPG_ERR_OPEN      0
#define JPG_ERR_FORMAT    2
#define JPG_ERR_TOOBIG    3

UBYTE GUI_ReadJpg(UWORD Xstart, UWORD Ystart, const char *path);

#endif
