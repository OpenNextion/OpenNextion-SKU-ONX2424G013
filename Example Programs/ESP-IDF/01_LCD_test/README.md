# 01_LCD_test

This ESP-IDF example drives the ONX2424G013 240 x 240 round SPI LCD and renders an LVGL UI.

## Features

- Initializes the GC9A01A-compatible SPI LCD on GPIO5/GPIO1/GPIO2/GPIO3/GPIO8 with Espressif's `esp_lcd_gc9a01` panel component.
- Enables BGR color order and display inversion.
- Controls the GPIO6 backlight with 25 kHz PWM to reduce angle-dependent stripe artifacts.
- Displays the text `LCD_test`.
- Shows one LVGL button.
- Maps the physical KEY button on GPIO9 to LVGL keypad `ENTER`, so press/release uses native LVGL button states.
- Supports mirror and rotation configuration with macros in `main/lcd_config.h`.

## Pin Map

| Function | GPIO |
| --- | --- |
| LCD SCLK | GPIO5 |
| LCD MOSI | GPIO1 |
| LCD CS | GPIO2 |
| LCD DC | GPIO3 |
| LCD RST | GPIO8 |
| LCD BL | GPIO6 |
| KEY | GPIO9 |

## Key Settings

```c
#define LCD_SPI_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 80
```

The LCD is created through the official `esp_lcd_gc9a01` component. The example does not provide a custom vendor initialization table.

## Orientation

```c
#define LCD_MIRROR_X 1
#define LCD_MIRROR_Y 0
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
```

## Build And Flash

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

Recommended hardware configuration: 16 MB flash, 8 MB OPI PSRAM.
