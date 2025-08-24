/*****************************************************************************
 * | File         :   sd.c
 * | Author       :   Waveshare team
 * | Function     :   SD card driver code for mounting, reading capacity, and unmounting
 * | Info         :
 * |                  This is the C file for SD card configuration and usage.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-28
 * | Info         :   Basic version, includes functions to initialize,
 * |                  read memory capacity, and manage SD card mounting/unmounting.
 *
 ******************************************************************************/

#include "sd.h"  // Include header file for SD card functions
#include <stdbool.h>

// Global variable for SD card structure
static sdmmc_card_t *card = NULL;

// Track initialization state to prevent double mount/unmount
static bool sd_initialized = false;

// Translate FatFs error codes to esp_err_t
static esp_err_t ff_result_to_esp_err(FRESULT result) {
    switch (result) {
        case FR_OK:
            return ESP_OK;
        case FR_DISK_ERR:
        case FR_INT_ERR:
        case FR_NOT_READY:
        case FR_NO_FILESYSTEM:
        case FR_MKFS_ABORTED:
        case FR_WRITE_PROTECTED:
        case FR_INVALID_DRIVE:
        case FR_DENIED:
        case FR_EXIST:
            return ESP_ERR_INVALID_STATE;
        case FR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return ESP_ERR_NOT_FOUND;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
            return ESP_ERR_INVALID_ARG;
        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
            return ESP_ERR_NO_MEM;
        default:
            return ESP_FAIL;
    }
}

// Define the mount point for the SD card
const char mount_point[] = MOUNT_POINT;

/**
 * @brief Initialize the SD card and mount the filesystem.
 * 
 * This function configures the SDMMC peripheral, sets up the host and slot, 
 * and mounts the FAT filesystem from the SD card.
 * 
 * @retval ESP_OK on success.
 * @retval ESP_ERR_INVALID_STATE if already initialized.
 * @retval other esp_err_t codes from esp_vfs_fat_sdmmc_mount on failure.
 */
esp_err_t sd_mmc_init() {
    esp_err_t ret;

    if (sd_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Configuration for mounting the FAT filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = EXAMPLE_FORMAT_IF_MOUNT_FAILED, // Format if mount fails
        .max_files = 5,                  // Max number of open files
        .allocation_unit_size = 16 * 1024 // Allocation unit size
    };

    ESP_LOGI(SD_TAG, "Initializing SD card");

    // Use the SDMMC peripheral for SD card communication
    ESP_LOGI(SD_TAG, "Using SDMMC peripheral");

    // Host configuration with default settings for high-speed operation
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // Slot configuration for SDMMC
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = EXAMPLE_PIN_CLK;
    slot_config.cmd = EXAMPLE_PIN_CMD;
    slot_config.d0 = EXAMPLE_PIN_D0;
#ifdef CONFIG_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;
    slot_config.d1 = EXAMPLE_PIN_D1;
    slot_config.d2 = EXAMPLE_PIN_D2;
    slot_config.d3 = EXAMPLE_PIN_D3;
#else
    slot_config.width = 1;
#endif
    // Enable internal pull-ups on the GPIOs
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(SD_TAG, "Mounting filesystem");
    ESP_LOGI(SD_TAG, "SD bus width: %d", slot_config.width);

    // Mount the filesystem and initialize the SD card
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. "
                             "Format the card if mount fails.");
        } else {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). "
                             "Check pull-up resistors on the card lines.", esp_err_to_name(ret));
        }
        return ret;
    }
    sd_initialized = true;
    ESP_LOGI(SD_TAG, "Filesystem mounted");
    return ret;
}

/**
 * @brief Print detailed SD card information to the console.
 * 
 * Uses the built-in `sdmmc_card_print_info` function to log information 
 * about the SD card to the standard output.
 */
void sd_card_print_info() {
    if (card) {
        sdmmc_card_print_info(stdout, card);
    }
}

/**
 * @brief Unmount the SD card and release resources.
 * 
 * This function unmounts the FAT filesystem and ensures all resources 
 * associated with the SD card are released.
 * 
 * @retval ESP_OK if unmounting succeeds.
 * @retval ESP_ERR_INVALID_STATE if the card is not mounted.
 * @retval other esp_err_t codes from esp_vfs_fat_sdcard_unmount on failure.
 */
esp_err_t sd_mmc_unmount() {
    if (!card) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (ret == ESP_OK) {
        card = NULL;
        sd_initialized = false;
    }
    return ret;
}

/**
 * @brief Get total and available memory capacity of the SD card.
 * 
 * @param total_capacity Pointer to store the total capacity (in KB).
 * @param available_capacity Pointer to store the available capacity (in KB).
 * 
 * @retval ESP_OK if memory information is successfully retrieved.
 * @retval ESP_ERR_INVALID_STATE if the card is not initialized.
 * @retval other esp_err_t codes translated from FatFs errors.
 */
esp_err_t read_sd_capacity(size_t *total_capacity, size_t *available_capacity) {
    if (!sd_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    FATFS *fs;
    uint32_t free_clusters;

    // Get the number of free clusters in the filesystem
    FRESULT res = f_getfree(mount_point, &free_clusters, &fs);
    if (res != FR_OK) {
        ESP_LOGE(SD_TAG, "Failed to get number of free clusters (%d)", res);
        return ff_result_to_esp_err(res);
    }

    // Calculate total and free sectors based on cluster size
    uint64_t total_sectors = ((uint64_t)(fs->n_fatent - 2)) * fs->csize;
    uint64_t free_sectors = ((uint64_t)free_clusters) * fs->csize;

    // Convert sectors to size in KB
    size_t sd_total_KB = (total_sectors * fs->ssize) / 1024;
    size_t sd_available_KB = (free_sectors * fs->ssize) / 1024;

    // Store total capacity if the pointer is valid
    if (total_capacity != NULL) {
        *total_capacity = sd_total_KB;
    }

    // Store available capacity if the pointer is valid
    if (available_capacity != NULL) {
        *available_capacity = sd_available_KB;
    }

    return ESP_OK;
}

