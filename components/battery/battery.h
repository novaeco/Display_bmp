#pragma once
#include <stdint.h>

/**
 * @brief Initialize battery measurement subsystem.
 */
void battery_init(void);

/**
 * @brief Read current battery level as percentage 0-100.
 */
uint8_t battery_get_percentage(void);

