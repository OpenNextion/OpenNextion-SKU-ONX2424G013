# 02_KNOB_KEY_BL_test

This ESPHome example uses LVGL to test the ONX2424G013 LCD backlight control with the rotary encoder and KEY button.

## Features

- Reuses the 01 LCD configuration with ESPHome's `ili9xxx` GC9A01A-compatible display driver.
- Controls the GPIO6 LCD backlight with 25 kHz LEDC PWM.
- Uses ESPHome's default light gamma correction value, `2.8`, for brightness mapping.
- Displays a 220 px LVGL arc around the screen as the target brightness indicator.
- Shows the current brightness and target brightness in the center of the LCD.
- Rotary encoder on GPIO48/GPIO47 changes the target brightness in the range 10-100.
- A short press of KEY on GPIO9 confirms the target value and applies it to the LCD backlight.
- Keeps the UI geometry, colors, font size, and interaction behavior aligned with the ESP-IDF and ESP-Arduino versions.

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
| Encoder A | GPIO48 |
| Encoder B | GPIO47 |

## Key Settings

Edit the YAML substitutions to adjust orientation and encoder sensitivity:

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
  knob_edges_per_brightness_step: "2"  # medium
```

`knob_edges_per_brightness_step` controls encoder sensitivity. The default is medium. Recommended values are high `"1"`, medium `"2"`, and low `"4"`.

The user-adjustable brightness range is `10..100`. ESPHome's light component still supports turning the backlight fully off internally during boot or service calls.

## UI Consistency

The three platform examples use the same LVGL layout:

- Background: `0x101820`
- Arc size: `220 x 220`
- Arc range: `10..100`
- Arc line width: `12`
- Target color: `0x46C2FF`
- Text font: `montserrat_14`
- Text line spacing: `4`
- Backlight gamma: `2.8`

## Build And Upload

```bash
esphome run 02_KNOB_KEY_BL_test.yaml
```

If PlatformIO rejects a local path that contains spaces, compile from a temporary path without spaces or move/copy the example to a path without spaces. The source YAML itself does not require any generated files.

Recommended hardware configuration: ESP32-S3, 16 MB flash, 8 MB OPI PSRAM.

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_02_KNOB_KEY_BL_test_esphome_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_02_KNOB_KEY_BL_test_esphome_factory.bin
```
