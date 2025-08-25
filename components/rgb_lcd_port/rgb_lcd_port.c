/*****************************************************************************
 * | File      	 :   rgb_lcd_port.c
 * | Author      :   Waveshare team
 * | Function    :   Hardware underlying interface
 * | Info        :
 *                   RGB LCD driver code
 *----------------
 * |This version :   V1.0
 * | Date        :   2024-11-19
 * | Info        :   Basic version
 *
 ******************************************************************************/

#include "rgb_lcd_port.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "io_extension.h"
#include "config.h"
#include <string.h>


const char *TAG = "rgb_lcd_port";

// Handle for the RGB LCD panel
static esp_lcd_panel_handle_t panel_handle = NULL; // Declare a handle for the LCD panel

// Statically allocated window buffer and its mutex
static uint8_t *s_window_buf = NULL;
static SemaphoreHandle_t s_window_buf_mutex = NULL;
static bool s_backlight_pwm_init = false;

/**
 * @brief Initialize the RGB LCD panel on the ESP32-S3
 *
 * This function configures and initializes an RGB LCD panel driver
 * using the ESP-IDF RGB LCD driver API. It sets up timing parameters,
 * GPIOs, data width, and framebuffer settings for the LCD panel.
 *
 * @return
 *    - ESP_OK: Initialization successful.
 *    - Other error codes: Initialization failed.
 */
esp_lcd_panel_handle_t waveshare_esp32_s3_rgb_lcd_init()
{
    // Log the start of the RGB LCD panel driver installation
    ESP_LOGI(TAG, "Install RGB LCD panel driver");

    // Configuration structure for the RGB LCD panel
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT, // Use the default clock source
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ, // Pixel clock frequency in Hz
            .h_res = LCD_H_RES,            // Horizontal resolution (number of pixels per row)
            .v_res = LCD_V_RES,            // Vertical resolution (number of rows)
            .hsync_pulse_width = 162,                // Horizontal sync pulse width
            .hsync_back_porch = 152,                 // Horizontal back porch
            .hsync_front_porch = 48,                // Horizontal front porch
            .vsync_pulse_width = 45,                // Vertical sync pulse width
            .vsync_back_porch = 13,                 // Vertical back porch
            .vsync_front_porch = 3,                // Vertical front porch
            .flags = {
                .pclk_active_neg = 1, // Set pixel clock polarity to active low
            },
        },
        .data_width = LCD_RGB_DATA_WIDTH,                    // Data width for RGB signals
        .bits_per_pixel = LCD_RGB_BIT_PER_PIXEL,             // Number of bits per pixel (color depth)
        .num_fbs = LCD_RGB_BUFFER_NUMS,                  // Number of framebuffers for double/triple buffering
        .bounce_buffer_size_px = LCD_RGB_BOUNCE_BUFFER_SIZE, // Bounce buffer size in pixels
        .sram_trans_align = 4,                                   // SRAM transaction alignment in bytes
        .psram_trans_align = 64,                                 // PSRAM transaction alignment in bytes
        .hsync_gpio_num = LCD_IO_RGB_HSYNC,              // GPIO for horizontal sync signal
        .vsync_gpio_num = LCD_IO_RGB_VSYNC,              // GPIO for vertical sync signal
        .de_gpio_num = LCD_IO_RGB_DE,                    // GPIO for data enable signal
        .pclk_gpio_num = LCD_IO_RGB_PCLK,                // GPIO for pixel clock signal
        .disp_gpio_num = LCD_IO_RGB_DISP,                // GPIO for display enable signal
        .data_gpio_nums = {
            // GPIOs for RGB data signals
            LCD_IO_RGB_DATA0,  // Data bit 0
            LCD_IO_RGB_DATA1,  // Data bit 1
            LCD_IO_RGB_DATA2,  // Data bit 2
            LCD_IO_RGB_DATA3,  // Data bit 3
            LCD_IO_RGB_DATA4,  // Data bit 4
            LCD_IO_RGB_DATA5,  // Data bit 5
            LCD_IO_RGB_DATA6,  // Data bit 6
            LCD_IO_RGB_DATA7,  // Data bit 7
            LCD_IO_RGB_DATA8,  // Data bit 8
            LCD_IO_RGB_DATA9,  // Data bit 9
            LCD_IO_RGB_DATA10, // Data bit 10
            LCD_IO_RGB_DATA11, // Data bit 11
            LCD_IO_RGB_DATA12, // Data bit 12
            LCD_IO_RGB_DATA13, // Data bit 13
            LCD_IO_RGB_DATA14, // Data bit 14
            LCD_IO_RGB_DATA15, // Data bit 15
        },
        .flags = {
            .fb_in_psram = 1, // Use PSRAM for framebuffers to save internal SRAM
        },
    };

    // Create and register the RGB LCD panel driver with the configuration above
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    // Log the initialization of the RGB LCD panel
    ESP_LOGI(TAG, "Initialize RGB LCD panel");

    io_extension_lcd_vdd_enable(true);
    // Initialize the RGB LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Allocate the window buffer and its mutex
    if (!s_window_buf) {
        s_window_buf = heap_caps_malloc(
            LCD_H_RES * LCD_V_RES * 2,
            MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM
        );
        if (!s_window_buf) {
            ESP_LOGE(TAG, "Failed to allocate window buffer");
        }
    }
    if (!s_window_buf_mutex) {
        s_window_buf_mutex = xSemaphoreCreateMutex();
        if (!s_window_buf_mutex) {
            ESP_LOGE(TAG, "Failed to create window buffer mutex");
        }
    }

    // Return success status
    return panel_handle;
}

/**
 * @brief Display a specific window of an image on the RGB LCD.
 *
 * This function updates a rectangular portion of the RGB LCD screen with the
 * image data provided. The region is defined by the start and end coordinates
 * in both X and Y directions. If the specified coordinates exceed the screen
 * boundaries, they will be clipped accordingly.
 *
 * @param Xstart Starting X coordinate of the display window (inclusive).
 * @param Ystart Starting Y coordinate of the display window (inclusive).
 * @param Xend Ending X coordinate of the display window (exclusive, relative to Xstart).
 * @param Yend Ending Y coordinate of the display window (exclusive, relative to Ystart).
 * @param Image Pointer to the image data buffer, representing the full LCD resolution.
 */
void waveshare_rgb_lcd_display_window(int16_t Xstart, int16_t Ystart, int16_t Xend, int16_t Yend, uint8_t *Image)
{
    // Ensure Xstart is within valid range, clip Xend to the screen width if necessary
    if (Xstart < 0) Xstart = 0;
    else if (Xend > g_display.width) Xend = g_display.width;

    if (Ystart < 0) Ystart = 0;
    else if (Yend > g_display.height) Yend = g_display.height;

    // Calculate the width and height of the cropped region
    int crop_width = Xend - Xstart;
    int crop_height = Yend - Ystart;

    if (crop_width <= 0 || crop_height <= 0 || !Image || !s_window_buf) {
        return;
    }

    if (s_window_buf_mutex) {
        xSemaphoreTake(s_window_buf_mutex, portMAX_DELAY);
    }

    // Copy the image data into the pre-allocated window buffer
    for (int y = 0; y < crop_height; y++) {
        const uint8_t *src_row = Image + ((Ystart + y) * LCD_H_RES + Xstart) * 2;
        uint8_t *dst_row = s_window_buf + y * crop_width * 2;
        memcpy(dst_row, src_row, crop_width * 2);
    }

    // Draw the cropped region onto the LCD at the specified coordinates
    esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, s_window_buf);

    if (s_window_buf_mutex) {
        xSemaphoreGive(s_window_buf_mutex);
    }
}


/**
 * @brief Display a full-screen image on the RGB LCD.
 *
 * This function replaces the entire LCD screen content with the image data
 * provided. It assumes the display resolution is 800x480.
 *
 * @param Image Pointer to the image data buffer.
 */
void waveshare_rgb_lcd_display(uint8_t *Image)
{
    // Draw the entire image on the screen
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, Image);
}

void waveshare_get_frame_buffer(void **buf1, void **buf2)
{
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, buf1, buf2));
}

void waveshare_esp32_s3_rgb_lcd_deinit(void)
{
    if (s_window_buf_mutex) {
        vSemaphoreDelete(s_window_buf_mutex);
        s_window_buf_mutex = NULL;
    }
    if (s_window_buf) {
        free(s_window_buf);
        s_window_buf = NULL;
    }
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
}
/**
 * @brief Turn on the RGB LCD screen backlight.
 *
 * This function enables the backlight of the screen by configuring the IO EXTENSION
 * I/O expander to output mode and setting the backlight pin to high. The IO EXTENSION
 * is controlled via I2C.
 *
 * @return
 *    - ESP_OK: Operation successful.
 */
void waveshare_rgb_lcd_bl_on()
{
    waveshare_rgb_lcd_set_brightness(100);
}

/**
 * @brief Turn off the RGB LCD screen backlight.
 *
 * This function disables the backlight of the screen by configuring the IO EXTENSION
 * I/O expander to output mode and setting the backlight pin to low. The IO EXTENSION
 * is controlled via I2C.
 *
 * @return
 *    - ESP_OK: Operation successful.
 */
void waveshare_rgb_lcd_bl_off()
{
    waveshare_rgb_lcd_set_brightness(0);
}

static void rgb_lcd_backlight_pwm_init(void)
{
    if (s_backlight_pwm_init) {
        return;
    }
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = LCD_PIN_NUM_BK_LIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);

    s_backlight_pwm_init = true;
}

void waveshare_rgb_lcd_set_brightness(uint8_t level)
{
    if (!s_backlight_pwm_init) {
        rgb_lcd_backlight_pwm_init();
    }
    if (level > 100) {
        level = 100;
    }
    uint32_t duty = (1 << LEDC_TIMER_10_BIT) * level / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}