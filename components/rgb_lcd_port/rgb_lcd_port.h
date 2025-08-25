/*****************************************************************************
 * | File        :   rgb_lcd_port.h
 * | Author      :   Waveshare team
 * | Function    :   Hardware underlying interface
 * | Info        :
 *                   This header file contains configuration and function 
 *                   declarations for the RGB LCD driver interface.
 *----------------
 * | Version     :   V1.0
 * | Date        :   2024-11-19
 * | Info        :   Basic version
 *
 ******************************************************************************/

#ifndef _RGB_LCD_H_
#define _RGB_LCD_H_

#pragma once

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "config.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief LCD Resolution and Timing
 *
 * LCD_H_RES and LCD_V_RES are defined in config.h based on menuconfig options.
 */
#define LCD_PIXEL_CLOCK_HZ      CONFIG_LCD_PIXEL_CLOCK_HZ ///< Pixel clock frequency in Hz

/**
 * @brief Color and Pixel Configuration
 */
#define LCD_BIT_PER_PIXEL       CONFIG_LCD_BIT_PER_PIXEL   ///< Bits per pixel (color depth)
#define LCD_RGB_BIT_PER_PIXEL   CONFIG_LCD_BIT_PER_PIXEL   ///< RGB interface color depth
#define LCD_RGB_DATA_WIDTH      CONFIG_LCD_RGB_DATA_WIDTH  ///< Data width for RGB interface
#define LCD_RGB_BUFFER_NUMS     CONFIG_LCD_RGB_BUFFER_NUMS ///< Number of frame buffers for double buffering
#define LCD_RGB_BOUNCE_BUFFER_SIZE  (LCD_H_RES * 10) ///< Size of bounce buffer for RGB data

/**
 * @brief GPIO Pins for RGB LCD Signals
 */
#define LCD_IO_RGB_DISP         CONFIG_LCD_IO_RGB_DISP   ///< DISP signal, -1 if not used
#define LCD_IO_RGB_VSYNC        CONFIG_LCD_IO_RGB_VSYNC  ///< Vertical sync signal
#define LCD_IO_RGB_HSYNC        CONFIG_LCD_IO_RGB_HSYNC  ///< Horizontal sync signal
#define LCD_IO_RGB_DE           CONFIG_LCD_IO_RGB_DE     ///< Data enable signal
#define LCD_IO_RGB_PCLK         CONFIG_LCD_IO_RGB_PCLK   ///< Pixel clock signal

/**
 * @brief GPIO Pins for RGB Data Signals
 */
// Blue data signals
#define LCD_IO_RGB_DATA0        CONFIG_LCD_IO_RGB_DATA0 ///< B3
#define LCD_IO_RGB_DATA1        CONFIG_LCD_IO_RGB_DATA1 ///< B4
#define LCD_IO_RGB_DATA2        CONFIG_LCD_IO_RGB_DATA2 ///< B5
#define LCD_IO_RGB_DATA3        CONFIG_LCD_IO_RGB_DATA3 ///< B6
#define LCD_IO_RGB_DATA4        CONFIG_LCD_IO_RGB_DATA4 ///< B7

// Green data signals
#define LCD_IO_RGB_DATA5        CONFIG_LCD_IO_RGB_DATA5 ///< G2
#define LCD_IO_RGB_DATA6        CONFIG_LCD_IO_RGB_DATA6 ///< G3
#define LCD_IO_RGB_DATA7        CONFIG_LCD_IO_RGB_DATA7 ///< G4
#define LCD_IO_RGB_DATA8        CONFIG_LCD_IO_RGB_DATA8 ///< G5
#define LCD_IO_RGB_DATA9        CONFIG_LCD_IO_RGB_DATA9 ///< G6
#define LCD_IO_RGB_DATA10       CONFIG_LCD_IO_RGB_DATA10 ///< G7

// Red data signals
#define LCD_IO_RGB_DATA11       CONFIG_LCD_IO_RGB_DATA11 ///< R3
#define LCD_IO_RGB_DATA12       CONFIG_LCD_IO_RGB_DATA12 ///< R4
#define LCD_IO_RGB_DATA13       CONFIG_LCD_IO_RGB_DATA13 ///< R5
#define LCD_IO_RGB_DATA14       CONFIG_LCD_IO_RGB_DATA14 ///< R6
#define LCD_IO_RGB_DATA15       CONFIG_LCD_IO_RGB_DATA15 ///< R7

/**
 * @brief Reset and Backlight Configuration
 */
#define LCD_IO_RST              CONFIG_LCD_IO_RST         ///< Reset pin, -1 if not used
#define LCD_PIN_NUM_BK_LIGHT    CONFIG_LCD_PIN_NUM_BK_LIGHT   ///< Backlight pin (EXIO2)
#define LCD_BK_LIGHT_ON_LEVEL   (CONFIG_LCD_BK_LIGHT_ON_LEVEL)    ///< Logic level to turn on backlight
#define LCD_BK_LIGHT_OFF_LEVEL  (!CONFIG_LCD_BK_LIGHT_ON_LEVEL)   ///< Logic level to turn off backlight

/**
 * @brief Function Declarations
 */
esp_lcd_panel_handle_t waveshare_esp32_s3_rgb_lcd_init();
void waveshare_esp32_s3_rgb_lcd_deinit(void);
/**
 * @brief Turn on the LCD backlight.
 */
void waveshare_rgb_lcd_bl_on();
/**
 * @brief Turn off the LCD backlight.
 */
void waveshare_rgb_lcd_bl_off();

/**
 * @brief Set RGB LCD backlight brightness (0-100%).
 */
void waveshare_rgb_lcd_set_brightness(uint8_t level);

/**
 * @brief Display a rectangular region of an image on the RGB LCD.
 *
 * @param Xstart Starting X coordinate of the region.
 * @param Ystart Starting Y coordinate of the region.
 * @param Xend Ending X coordinate of the region.
 * @param Yend Ending Y coordinate of the region.
 * @param Image Pointer to the image data buffer.
 */
void waveshare_rgb_lcd_display_window(int16_t Xstart, int16_t Ystart, int16_t Xend, int16_t Yend, uint8_t *Image);

/**
 * @brief Display a full-frame image on the RGB LCD.
 *
 * @param Image Pointer to the image data buffer.
 */
void waveshare_rgb_lcd_display(uint8_t *Image);

/**
 * @brief Retrieve pointers to the frame buffers for double buffering.
 *
 * @param buf1 Pointer to hold the address of the first frame buffer.
 * @param buf2 Pointer to hold the address of the second frame buffer.
 */
void waveshare_get_frame_buffer(void **buf1, void **buf2);

#endif // _RGB_LCD_H_
