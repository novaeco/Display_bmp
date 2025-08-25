#include "battery.h"
#include "io_extension.h"
#include "esp_log.h"

#define BATTERY_ADC_MAX          4095
#define BATTERY_VOLTAGE_MAX_MV   4200
#define BATTERY_VOLTAGE_MIN_MV   3300

static const char *TAG = "battery";

void battery_init(void)
{
    // IO extension is initialized elsewhere (e.g., touch driver). Nothing to do for now.
    ESP_LOGI(TAG, "battery subsystem initialized");
}

uint8_t battery_get_percentage(void)
{
    uint16_t raw = IO_EXTENSION_Adc_Input();
    uint32_t voltage_mv = (uint32_t)raw * BATTERY_VOLTAGE_MAX_MV / BATTERY_ADC_MAX;
    if (voltage_mv < BATTERY_VOLTAGE_MIN_MV) {
        return 0;
    }
    if (voltage_mv > BATTERY_VOLTAGE_MAX_MV) {
        voltage_mv = BATTERY_VOLTAGE_MAX_MV;
    }
    uint32_t percent = (voltage_mv - BATTERY_VOLTAGE_MIN_MV) * 100 /
                       (BATTERY_VOLTAGE_MAX_MV - BATTERY_VOLTAGE_MIN_MV);
    if (percent > 100) {
        percent = 100;
    }
    return (uint8_t)percent;
}

