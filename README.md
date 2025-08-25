| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

| Supported LCD Controller    | ST7262 |
| ----------------------------| -------|

| Supported TOUCH Controller    | GT911 |
| ----------------------------| -------|

## Software Prerequisites

* **ESP-IDF v5.1 or later** with the ESP32‑S3 toolchain (`xtensa-esp32s3-elf`).
* **Python 3.8+** with `pip` and the ESP‑IDF Python requirements (`pip install -r $IDF_PATH/requirements.txt`).
* **Build tools**: `git`, `cmake` ≥3.16, `ninja` ≥1.10, `esptool.py` and `idf.py` (installed via `install.sh` from ESP‑IDF).
* Optional utilities for debugging and flashing such as `openocd-esp32`.

## Hardware Required

* Waveshare **ESP32-S3-Touch-LCD-7B** (1024 × 600) development kit.
* microSD card (FAT formatted) containing BMP images.
* Optional peripherals according to the interface used (CAN/RS485 transceiver, Li‑ion battery, etc.).

## Hardware Connection

The connection between ESP Board and the LCD is as follows:

```
       ESP Board                           RGB  Panel
+-----------------------+              +-------------------+
|                   GND +--------------+GND                |
|                       |              |                   |
|                   3V3 +--------------+VCC                |
|                       |              |                   |
|                   PCLK+--------------+PCLK               |
|                       |              |                   |
|             DATA[15:0]+--------------+DATA[15:0]         |
|                       |              |                   |
|                  HSYNC+--------------+HSYNC              |
|                       |              |                   |
|                  VSYNC+--------------+VSYNC              |
|                       |              |                   |
|                     DE+--------------+DE                 |
|                       |              |                   |
|               BK_LIGHT+--------------+BLK                |
       ESP Board                             TOUCH  
+-----------------------+              +-------------------+
|                    GND+--------------+GND                |
|                       |              |                   |
|                    3V3+--------------+VCC                |
|                       |              |                   |
|                  GPIO8+--------------+SDA                |
|                       |              |                   |
|                  GPIO9+--------------+SCL                |
|                       |              |                   |
       ESP Board                              SD Card
+-----------------------+              +-------------------+
|                   GND +--------------+GND                |
|                       |              |                   |
|                   3V3 +--------------+VCC                |
|                       |              |                   |
|                 GPIO11+--------------+CMD                |
|                       |              |                   |
|                 GPIO12+--------------+CLK                |
|                       |              |                   |
|                 GPIO13+--------------+D0                 |
+-----------------------+              |                   |
|                       |              |                   |
       IO EXTENSION.EXIO1+--------------+TP_RST             |
|                       |              |                   |
       IO EXTENSION.EXIO2+--------------+DISP_EN            |
                                          |                   |
       IO EXTENSION.EXIO4+--------------+SD_CS              |
            
                                       +-------------------+
```

* Read BMP files from the SD card and display them on the screen.
* Use the touchscreen to switch between images.

## Project Configuration

1. **Select the target:**
   ```bash
   idf.py set-target esp32s3
   ```
2. **Load defaults and open menuconfig:**
   ```bash
   idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" menuconfig
   ```
3. **Key `sdkconfig` parameters:**

   | Option | Purpose | Recommended value |
   | ------ | ------- | ---------------- |
   | `CONFIG_ESP_WIFI_ENABLED` | Enable Wi‑Fi 802.11 b/g/n | `y` when using Wi‑Fi |
   | `CONFIG_BT_BLE_ENABLED` | Enable Bluetooth Low Energy stack | `y` when using BLE |
   | `CONFIG_TWAI` | Activate TWAI (CAN) controller | `y` when using CAN |
   | `CONFIG_UART_ISR_IN_IRAM` & `CONFIG_UART_RS485_MODE` | Allow deterministic RS485 on UART1 | `y` when using RS485 |
   | `CONFIG_PM_ENABLE` | Dynamic power management for battery operation | `y` |
   | `CONFIG_LCD_BACKLIGHT_PWM` | Backlight dimming via PWM | `y` |

### Build and Flash

```bash
idf.py -p PORT build flash monitor
```

The first invocation of `idf.py` downloads tools and may take additional time.

Press `Ctrl-]` to exit the serial monitor.

Refer to the [ESP‑IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for a complete toolchain installation tutorial.

## Hardware Options

### Wireless Connectivity
* **Wi‑Fi:** Integrated 2.4 GHz 802.11 b/g/n transceiver and PCB antenna. Disable `CONFIG_ESP_WIFI_ENABLED` when not required to save power.
* **Bluetooth Low Energy:** BLE 5 stack accessible through `esp_bt.h`; enable via `CONFIG_BT_BLE_ENABLED`.

### Field Bus Interfaces
* **CAN (TWAI):** `GPIO20` (TX) and `GPIO19` (RX) routed to the IO‑Extension header for connection to an external CAN transceiver. Configure `CONFIG_TWAI` and use the provided `can.c` driver.
* **RS485:** UART1 signals are exposed for half‑duplex RS485 with an external differential transceiver. Activate RS485 mode using `CONFIG_UART_RS485_MODE`.

### Battery Management
* On‑board single‑cell Li‑ion charger accepts 5 V from USB‑C or an external source and handles charge, protection and fuel gauging.
* Enable `CONFIG_PM_ENABLE` for dynamic frequency scaling and light‑sleep to extend battery life.

### Backlight Control
* LCD backlight is powered from 5 V and switched through the IO‑Extension (`IO_EXTENSION_IO_2`).
* `CONFIG_LCD_BACKLIGHT_PWM` selects the PWM channel; duty cycle defines luminance (0–100 %).

### Example: Wi‑Fi Station Connection

```c
#include "esp_wifi.h"

static void wifi_init_sta(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "MySSID",
            .password = "MyPassword",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
```

### Power Supply Diagram

```
    +5V USB/Ext ──[PMIC / Charger]──┬── 3.7V Li‑ion
                                   │
                                   ├─[Buck 3.3V]─> ESP32‑S3 & Logic
                                   │
                                   └─[Boost 5V]─> LCD Backlight (PWM)
```

## Troubleshooting

For any technical queries, please open an https://service.waveshare.com/. We will get back to you soon.

## License

This project is licensed under the [MIT License](LICENSE) © 2024 Waveshare team.
