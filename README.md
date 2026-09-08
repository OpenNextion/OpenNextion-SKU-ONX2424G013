[English](README.md) | [简体中文](README.zh-CN.md)

# ONX2424G013

<img src="Product%20Images/ONX2424G013_01.jpg" alt="ONX2424G013" width="360">

## Overview

`ONX2424G013` is a 1.28-inch Open Nextion Genius Series smart knob screen built around the `ESP32-S3R8`. Its round IPS display, rotary encoder, and push button provide a compact interface for menus, parameter adjustment, and connected controls.

This repository provides hardware documents, product photos, mechanical files, and nine examples across `ESP-IDF`, `ESP-Arduino`, and `ESPHome`, including source code, bilingual instructions, and merged factory firmware.

## Purchase Links

- Product: [Open Nextion Genius Series 1.28-inch ESP32-S3 Smart Knob Screen](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/)

## Hardware Specifications

Specifications are based on the [official product page](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/) and the included example documentation. See the [official Wiki](https://nextion.tech/wiki/onx2424g013/) for further hardware guidance.

| Item | Specification |
| --- | --- |
| Model | `ONX2424G013` |
| MCU | `ESP32-S3R8`, up to `240 MHz` |
| Flash / PSRAM | `16 MB` flash / `8 MB` OPI PSRAM |
| Display | `1.28-inch` round IPS, `240 x 240` |
| Colors / Brightness | `262K` / `300 nit` |
| LCD Driver | `GC9A01` family; examples use GC9A01-compatible drivers |
| User Input | Rotary encoder and physical KEY button |
| Wireless | `2.4 GHz` Wi-Fi, Bluetooth LE 5.0 |
| Power Input | `DC 5V/1A` |
| Interfaces | USB-C, UART, six GPIOs (`IO10`, `IO11`, `IO17`-`IO20`) |
| Operating Temperature | `-20 to 70 C` |
| Storage Temperature | `-30 to 80 C` |

## Key Features

- Round display and tactile controls for compact graphical interfaces.
- PWM backlight control, demonstrated with a rotary brightness selector and press-to-confirm interaction.
- Wi-Fi provisioning examples with an on-screen connection status and browser-based network selection.
- Three development workflows, with an ESPHome Wi-Fi example supporting Home Assistant discovery.
- Prebuilt factory images for testing the examples without compiling source code.

## Repository Contents

| Folder | Description |
| --- | --- |
| [Certifications/](Certifications/) | CE-RED, FCC ID, FCC SDOC, and RoHS documents |
| [Datasheets/](Datasheets/) | ESP32-S3 datasheet and technical reference manual, plus GC9A01N display datasheet |
| [Drawings/](Drawings/) | Mechanical dimensions and STEP 3D model |
| [Example Programs/](Example%20Programs/) | ESP-IDF, Arduino, and ESPHome examples, firmware, and hardware reference documents |
| [LTA Announcement/](LTA%20Announcement/) | Product announcement document |
| [Product Images/](Product%20Images/) | Product photos |
| [Schematic & Layout/](Schematic%20%26%20Layout/) | Schematic and PCB bitmap files |

## Example Projects

All three platforms provide the following examples. Each example includes `README.md`, `README_cn.md`, and a matching `*_factory.bin` image.

| Example | Description | ESP-IDF | Arduino | ESPHome |
| --- | --- | --- | --- | --- |
| `01_LCD_test` | LCD/LVGL interface and physical KEY confirmation | [Open](Example%20Programs/ESP-IDF/01_LCD_test/) | [Open](Example%20Programs/ESP-Arduino/01_LCD_test/) | [Open](Example%20Programs/ESPHome/01_LCD_test/) |
| `02_KNOB_KEY_BL_test` | Rotate to select brightness, then press KEY to apply | [Open](Example%20Programs/ESP-IDF/02_KNOB_KEY_BL_test/) | [Open](Example%20Programs/ESP-Arduino/02_KNOB_KEY_BL_test/) | [Open](Example%20Programs/ESPHome/02_KNOB_KEY_BL_test/) |
| `03_WiFi_test` | Captive-portal provisioning and LCD connection status | [Open](Example%20Programs/ESP-IDF/03_WiFi_test/) | [Open](Example%20Programs/ESP-Arduino/03_WiFi_test/) | [Open](Example%20Programs/ESPHome/03_WiFi_test/) |

## Quick Start

### 1. Connect the Board

Connect the board to your computer using a USB-C data cable. Start with `01_LCD_test` to check display output and KEY input, then try the knob and Wi-Fi examples. Replace `<PORT>` in commands with your device's serial port.

### 2. Run a Prebuilt Example

The included factory images are merged images intended for flash address `0x0`. Using the esptool.py syntax documented in the examples:

```bash
cd "Example Programs/ESP-IDF/01_LCD_test"
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

Flashing replaces the existing firmware. Use the image belonging to the desired example and framework.

### 3. Build with ESP-IDF

In an initialized ESP-IDF environment, run from the repository root:

```bash
cd "Example Programs/ESP-IDF/01_LCD_test"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

The project declares `esp_lcd_gc9a01` and LVGL dependencies in `main/idf_component.yml`. Keep the supplied configuration files; the Wi-Fi example includes its own larger application partition.

### 4. Build with Arduino

Install the ESP32 board package, `GFX Library for Arduino`, and LVGL 8.x (the examples recommend `8.3.11`). Open the desired `.ino` file and configure Arduino IDE as follows:

| Setting | Value |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| USB Mode | `Hardware CDC and JTAG` |
| USB CDC On Boot | `Enabled` |
| Upload Mode | `UART0 / Hardware CDC` |
| Flash Size | `16MB (128Mb)` |
| Partition Scheme | `16M Flash (3MB APP/9.9MB FATFS)` |
| PSRAM | `OPI PSRAM` |

Keep `build_opt.h` beside the sketch, compile, and upload. Open Serial Monitor at `115200` baud for logs. See the selected example's README for its Arduino CLI commands.

### 5. Build with ESPHome

With ESPHome installed, run from the repository root:

```bash
cd "Example Programs/ESPHome/01_LCD_test"
esphome run 01_LCD_test.yaml
```

The Wi-Fi example documents validation with ESPHome `2026.6.2`. If PlatformIO rejects paths containing spaces, use a working copy of the example in a path without spaces. Keep the Wi-Fi example's `components/wifi_portal` directory alongside its YAML file.

### 6. Try Wi-Fi Provisioning

Flash `03_WiFi_test`, connect your phone to the `ONX2424G013` access point, and open `http://192.168.4.1` if the portal does not appear automatically. Select a network and enter its password; the LCD reports the connection state and assigned IP address.

## Notes

- The examples use rotary and physical button input; their LVGL buttons are operated through KEY.
- Wi-Fi is limited to `2.4 GHz` networks.
- The provisioning demo creates an open access point. The ESPHome Wi-Fi demo also enables Native API without encryption or a password; configure access protection before using it beyond a test setup.
- GPIO19/GPIO20 are ESP32-S3 USB signals. Check the schematic and USB requirements before assigning expansion pins to another function.
- Display orientation settings are in `lcd_config.h`, `main/lcd_config.h`, or YAML substitutions, depending on the framework.
- See [mechanical dimensions](Drawings/ONX2424G013-Dimension.pdf) before designing an enclosure, and [schematic/layout files](Schematic%20%26%20Layout/) before wiring peripherals.

## References

- [ONX2424G013 Wiki](https://nextion.tech/wiki/onx2424g013/)
- [Official purchase page](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/)
- [Included hardware reference](Example%20Programs/ONX2424G013_Hardware_Skill.md)
