# 02_KNOB_KEY_BL_test

This ESP-IDF example uses LVGL to test the ONX2424G013 LCD backlight control with the rotary encoder and KEY button.

## Features

- Reuses the 01 LCD initialization path with Espressif's official `esp_lcd_gc9a01` panel component.
- Controls the GPIO6 LCD backlight with 25 kHz, 11-bit LEDC PWM and gamma-corrected brightness.
- Displays a 220 px LVGL arc around the screen as the target brightness indicator.
- Shows the current brightness and target brightness in the center of the LCD.
- Rotary encoder on GPIO48/GPIO47 changes the target brightness in the range 10-100.
- A short press of KEY on GPIO9 confirms the target value and applies it to the LCD backlight.
- Keeps the UI geometry, colors, font size, and interaction behavior aligned with the ESP-Arduino and ESPHome versions.

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

Edit `main/lcd_config.h` to adjust hardware and interaction settings:

```c
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_MIN_PERCENT 10
#define LCD_BRIGHTNESS_PERCENT 100
#define LCD_BACKLIGHT_GAMMA 2.8f
#define KNOB_EDGES_PER_BRIGHTNESS_STEP KNOB_SENSITIVITY_MEDIUM
#define KNOB_DIRECTION_INVERT 0
```

`KNOB_EDGES_PER_BRIGHTNESS_STEP` controls encoder sensitivity. The default is medium. Presets are high `1`, medium `2`, and low `4`.

`LCD_BRIGHTNESS_MIN_PERCENT` limits the user-adjustable range shown by the Arc. The backlight helper still accepts `0` internally so startup code can keep the LCD dark until the first LVGL frame is ready.

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

## Build And Flash

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_02_KNOB_KEY_BL_test_esp_idf_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_02_KNOB_KEY_BL_test_esp_idf_factory.bin
```

Recommended hardware configuration: 16 MB flash, 8 MB OPI PSRAM.
