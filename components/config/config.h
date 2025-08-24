#pragma once
#include "sdkconfig.h"
#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t margin_left;
    uint16_t margin_right;
    uint16_t margin_top;
    uint16_t margin_bottom;
} display_geometry_t;

#define LCD_WIDTH            CONFIG_DISPLAY_WIDTH
#define LCD_HEIGHT           CONFIG_DISPLAY_HEIGHT
#define LCD_MARGIN_LEFT      CONFIG_DISPLAY_MARGIN_LEFT
#define LCD_MARGIN_RIGHT     CONFIG_DISPLAY_MARGIN_RIGHT
#define LCD_MARGIN_TOP       CONFIG_DISPLAY_MARGIN_TOP
#define LCD_MARGIN_BOTTOM    CONFIG_DISPLAY_MARGIN_BOTTOM

#if CONFIG_DISPLAY_ORIENTATION_PORTRAIT
#define LCD_H_RES LCD_HEIGHT
#define LCD_V_RES LCD_WIDTH
#define DISPLAY_IS_PORTRAIT 1
#else
#define LCD_H_RES LCD_WIDTH
#define LCD_V_RES LCD_HEIGHT
#define DISPLAY_IS_PORTRAIT 0
#endif
