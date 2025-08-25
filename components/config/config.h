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

#ifndef CONFIG_INACTIVITY_TIMEOUT_MS
#define CONFIG_INACTIVITY_TIMEOUT_MS 60000
#endif
#define INACTIVITY_TIMEOUT_MS CONFIG_INACTIVITY_TIMEOUT_MS

#ifndef CONFIG_LCD_PIXEL_CLOCK_HZ
#define CONFIG_LCD_PIXEL_CLOCK_HZ 30000000
#endif

#ifndef CONFIG_LCD_BIT_PER_PIXEL
#define CONFIG_LCD_BIT_PER_PIXEL 16
#endif

#ifndef CONFIG_LCD_RGB_DATA_WIDTH
#define CONFIG_LCD_RGB_DATA_WIDTH 16
#endif

#ifndef CONFIG_LCD_RGB_BUFFER_NUMS
#define CONFIG_LCD_RGB_BUFFER_NUMS 2
#endif

#ifndef CONFIG_LCD_IO_RGB_DISP
#define CONFIG_LCD_IO_RGB_DISP -1
#endif

#ifndef CONFIG_LCD_IO_RGB_VSYNC
#define CONFIG_LCD_IO_RGB_VSYNC 3
#endif

#ifndef CONFIG_LCD_IO_RGB_HSYNC
#define CONFIG_LCD_IO_RGB_HSYNC 46
#endif

#ifndef CONFIG_LCD_IO_RGB_DE
#define CONFIG_LCD_IO_RGB_DE 5
#endif

#ifndef CONFIG_LCD_IO_RGB_PCLK
#define CONFIG_LCD_IO_RGB_PCLK 7
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA0
#define CONFIG_LCD_IO_RGB_DATA0 14
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA1
#define CONFIG_LCD_IO_RGB_DATA1 38
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA2
#define CONFIG_LCD_IO_RGB_DATA2 18
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA3
#define CONFIG_LCD_IO_RGB_DATA3 17
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA4
#define CONFIG_LCD_IO_RGB_DATA4 10
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA5
#define CONFIG_LCD_IO_RGB_DATA5 39
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA6
#define CONFIG_LCD_IO_RGB_DATA6 0
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA7
#define CONFIG_LCD_IO_RGB_DATA7 45
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA8
#define CONFIG_LCD_IO_RGB_DATA8 48
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA9
#define CONFIG_LCD_IO_RGB_DATA9 47
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA10
#define CONFIG_LCD_IO_RGB_DATA10 21
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA11
#define CONFIG_LCD_IO_RGB_DATA11 1
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA12
#define CONFIG_LCD_IO_RGB_DATA12 2
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA13
#define CONFIG_LCD_IO_RGB_DATA13 42
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA14
#define CONFIG_LCD_IO_RGB_DATA14 41
#endif

#ifndef CONFIG_LCD_IO_RGB_DATA15
#define CONFIG_LCD_IO_RGB_DATA15 40
#endif

#ifndef CONFIG_LCD_IO_RST
#define CONFIG_LCD_IO_RST -1
#endif

#ifndef CONFIG_LCD_PIN_NUM_BK_LIGHT
#define CONFIG_LCD_PIN_NUM_BK_LIGHT 16
#endif

#ifndef CONFIG_LCD_BK_LIGHT_ON_LEVEL
#define CONFIG_LCD_BK_LIGHT_ON_LEVEL 1
#endif

#ifndef CONFIG_IOEXT_LCD_VDD_EN
#define CONFIG_IOEXT_LCD_VDD_EN 6
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t margin_left;
    uint16_t margin_right;
    uint16_t margin_top;
    uint16_t margin_bottom;
} display_geometry_t;

extern bool g_is_portrait;
extern display_geometry_t g_display;

void display_update_geometry(void);
void display_set_orientation(bool portrait);
esp_err_t display_load_orientation(void);
esp_err_t display_save_orientation(void);

#define LCD_H_RES LCD_WIDTH
#define LCD_V_RES LCD_HEIGHT
