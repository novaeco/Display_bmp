#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "display"
#define NVS_KEY_ORIENT "orient"

bool g_is_portrait =
#if CONFIG_DISPLAY_ORIENTATION_PORTRAIT
    true;
#else
    false;
#endif

display_geometry_t g_display;

void display_update_geometry(void)
{
    if (g_is_portrait) {
        g_display.width = CONFIG_DISPLAY_HEIGHT;
        g_display.height = CONFIG_DISPLAY_WIDTH;
        g_display.margin_left = CONFIG_DISPLAY_MARGIN_TOP;
        g_display.margin_right = CONFIG_DISPLAY_MARGIN_BOTTOM;
        g_display.margin_top = CONFIG_DISPLAY_MARGIN_RIGHT;
        g_display.margin_bottom = CONFIG_DISPLAY_MARGIN_LEFT;
    } else {
        g_display.width = CONFIG_DISPLAY_WIDTH;
        g_display.height = CONFIG_DISPLAY_HEIGHT;
        g_display.margin_left = CONFIG_DISPLAY_MARGIN_LEFT;
        g_display.margin_right = CONFIG_DISPLAY_MARGIN_RIGHT;
        g_display.margin_top = CONFIG_DISPLAY_MARGIN_TOP;
        g_display.margin_bottom = CONFIG_DISPLAY_MARGIN_BOTTOM;
    }
}

void display_set_orientation(bool portrait)
{
    g_is_portrait = portrait;
    display_update_geometry();
}

esp_err_t display_load_orientation(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t val;
        err = nvs_get_u8(nvs, NVS_KEY_ORIENT, &val);
        if (err == ESP_OK) {
            g_is_portrait = (val != 0);
        }
        nvs_close(nvs);
    }
    display_update_geometry();
    return err;
}

esp_err_t display_save_orientation(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(nvs, NVS_KEY_ORIENT, g_is_portrait ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
