| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

| Supported LCD Controller    | ST7262 |
| ----------------------------| -------|

| Supported TOUCH Controller    | GT911 |
| ----------------------------| -------|
## How to use the example

## ESP-IDF Required

### Hardware Required

* An Waveshare ESP32-S3-Touch-LCD-7B (1024*600)

### Hardware Connection

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

* Read BMP files from the SD card and display on the screen.
* Use the touchscreen to switch between images.

### Image size limitations

The decoders operate with a single line buffer (`UWORD row[width]`), so images must fit within the display geometry:

* Maximum width: **1024 px** (`LCD_H_RES`)
* Maximum height: **600 px** (`LCD_V_RES`)
* Recommended file size: **< 5&nbsp;MB** to keep decode time and SD transfers reasonable

Larger images or heavier files may fail to render or overflow the line buffers.

### Configure the Project

### Build and Flash

Run `idf.py set-target esp32s3` to select the target chip.

Run `idf.py -p PORT build flash monitor` to build, flash and monitor the project. A fancy animation will show up on the LCD as expected.

The first time you run `idf.py` for the example will cost extra time.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Troubleshooting

For any technical queries, please open an https://service.waveshare.com/. We will get back to you soon.

## License

This project is licensed under the [MIT License](LICENSE) © 2024 Waveshare team.
