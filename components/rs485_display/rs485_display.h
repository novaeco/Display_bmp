#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART1 in RS485 half-duplex mode for navigation commands.
 *
 * The UART is configured on GPIO15 (TXD) and GPIO16 (RXD).
 * ASCII frames "NEXT" or "PREV" received over the bus generate
 * synthetic touch events to control image navigation.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t rs485_display_init(void);

/**
 * @brief Deinitialize RS485 display UART and delete task.
 *
 * Removes the FreeRTOS task created by rs485_display_init() and deletes
 * the UART driver.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t rs485_display_deinit(void);

#ifdef __cplusplus
}
#endif

