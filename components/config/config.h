#pragma once
#include "sdkconfig.h"

#ifndef CONFIG_DISPLAY_WIDTH
#warning "CONFIG_DISPLAY_WIDTH non défini, valeur 1024 utilisée"
#define CONFIG_DISPLAY_WIDTH 1024
#endif
#define LCD_WIDTH CONFIG_DISPLAY_WIDTH

#ifndef CONFIG_DISPLAY_HEIGHT
#warning "CONFIG_DISPLAY_HEIGHT non défini, valeur 600 utilisée"
#define CONFIG_DISPLAY_HEIGHT 600
#endif
#define LCD_HEIGHT CONFIG_DISPLAY_HEIGHT

#ifndef CONFIG_DISPLAY_MARGIN_LEFT
#warning "CONFIG_DISPLAY_MARGIN_LEFT non défini, valeur 120 utilisée"
#define CONFIG_DISPLAY_MARGIN_LEFT 120
#endif
#define LCD_MARGIN_LEFT CONFIG_DISPLAY_MARGIN_LEFT

#ifndef CONFIG_DISPLAY_MARGIN_RIGHT
#warning "CONFIG_DISPLAY_MARGIN_RIGHT non défini, valeur 120 utilisée"
#define CONFIG_DISPLAY_MARGIN_RIGHT 120
#endif
#define LCD_MARGIN_RIGHT CONFIG_DISPLAY_MARGIN_RIGHT

#ifndef CONFIG_DISPLAY_MARGIN_TOP
#warning "CONFIG_DISPLAY_MARGIN_TOP non défini, valeur 0 utilisée"
#define CONFIG_DISPLAY_MARGIN_TOP 0
#endif
#define LCD_MARGIN_TOP CONFIG_DISPLAY_MARGIN_TOP

#ifndef CONFIG_DISPLAY_MARGIN_BOTTOM
#warning "CONFIG_DISPLAY_MARGIN_BOTTOM non défini, valeur 0 utilisée"
#define CONFIG_DISPLAY_MARGIN_BOTTOM 0
#endif
#define LCD_MARGIN_BOTTOM CONFIG_DISPLAY_MARGIN_BOTTOM

#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t margin_left;
    uint16_t margin_right;
    uint16_t margin_top;
    uint16_t margin_bottom;
} display_geometry_t;

#if CONFIG_DISPLAY_ORIENTATION_PORTRAIT
#define LCD_H_RES LCD_HEIGHT
#define LCD_V_RES LCD_WIDTH
#define DISPLAY_IS_PORTRAIT 1
#else
#define LCD_H_RES LCD_WIDTH
#define LCD_V_RES LCD_HEIGHT
#define DISPLAY_IS_PORTRAIT 0
#endif
