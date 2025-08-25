#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize TWAI interface for display navigation commands.
 *
 * Installs and starts the TWAI driver on GPIO20 (TX) and GPIO19 (RX).
 * Incoming CAN frames containing the ASCII strings "NEXT" or "PREV"
 * will generate synthetic touch events to drive the image navigation.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t can_display_init(void);

#ifdef __cplusplus
}
#endif

