# 01_LCD_test

This ESPHome example drives the ONX2424G013 240 x 240 round SPI LCD and renders an LVGL UI.

## Features

- Initializes the GC9A01A-compatible SPI LCD on GPIO5/GPIO1/GPIO2/GPIO3/GPIO8.
- Uses BGR color order and display inversion.
- Controls the GPIO6 backlight with 25 kHz PWM to reduce angle-dependent stripe artifacts.
- Displays the text `LCD_test`.
- Shows one LVGL button.
- Maps the physical KEY button on GPIO9 to LVGL keypad `ENTER`.
- Focuses the LVGL button after boot, so KEY press/release uses native LVGL button states and click handling.
- Supports mirror and rotation configuration with YAML substitutions.

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

## Orientation

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
```

Rotation notes:

- 0 degree on ONX2424G013: `mirror_x: true`, `mirror_y: false`, `swap_xy: false`
- 90/270 degrees: enable `swap_xy`, then adjust `mirror_x` and `mirror_y` until the mounted direction is correct.
- 180 degrees: set both `mirror_x` and `mirror_y` to `true`.

## Build And Upload

```bash
esphome run 01_LCD_test.yaml
```

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_01_LCD_test_esphome_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esphome_factory.bin
```

Recommended hardware configuration: ESP32-S3, 16 MB flash, 8 MB OPI PSRAM.
