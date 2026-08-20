# 03_WiFi_test

This ESP-Arduino example uses LVGL to test WiFi provisioning on ONX2424G013.

Required Arduino libraries:

- `lvgl` 8.x, recommended 8.3.11
- `GFX Library for Arduino`

## Features

- Reuses the stable LCD and LVGL initialization path from the ESP-Arduino 01/02 examples.
- Shows the same LCD UI layout as the ESP-IDF 03 example.
- Starts an open AP named `ONX2424G013`.
- Runs a captive portal on `192.168.4.1`.
- Uses wildcard DNS so phone captive-portal probes resolve to the device.
- The portal scans nearby WiFi networks on entry and supports manual refresh.
- WiFi scanning uses the ESP-IDF WiFi scan API inside the `/scan` request handler, matching the ESP-IDF 03 example behavior.
- The user can select an SSID, enter the password, and connect the device.
- The LCD shows the selected WiFi name, connection state, and assigned STA IP.
- Prints diagnostic logs to the USB Serial/JTAG port at 115200 baud.

## Key Settings

Edit `lcd_config.h` if needed:

```c
#define WIFI_AP_SSID "ONX2424G013"
#define WIFI_PORTAL_IP "192.168.4.1"
#define WIFI_SCAN_MAX_AP 20
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100
```

The AP is intentionally open because this is a provisioning test example.

## UI Consistency

The LCD UI follows the ESP-IDF 03 example:

- Title: `WiFi_test`, Montserrat 28
- Status labels: Montserrat 14
- Background: `0x101820`
- AP/status/IP/hint text positions and colors match the ESP-IDF version
- Bottom hint is split into short wrapped lines to stay inside the round screen boundary

The web provisioning page uses the same route structure and visual style as the ESP-IDF version:

- `/` and unknown paths serve the portal page
- `/scan` returns nearby WiFi APs as JSON
- `/connect` accepts `ssid` and `password`
- `/status` returns selected SSID, connection state, and STA IP
- `/js_start` and `/js_error` are diagnostic endpoints used only for USB serial troubleshooting.

## Arduino IDE Setup

- Install the `esp32` board package and select **ESP32S3 Dev Module**.
- Configure the board for the ONX2424G013 hardware:
  - USB Mode: **Hardware CDC and JTAG**
  - USB CDC On Boot: **Enabled**
  - Upload Mode: **UART0 / Hardware CDC**
  - Flash Size: **16MB (128Mb)**
  - Partition Scheme: **16M Flash (3MB APP/9.9MB FATFS)**
  - PSRAM: **OPI PSRAM**
  - Core Debug Level: **Info**
- Open `ESP-Arduino/03_WiFi_test/03_WiFi_test.ino`.
- Keep `build_opt.h` in the same folder as the sketch. It enables the LVGL options required by this example, including `lv_font_montserrat_28`.
- Compile and upload.

Arduino CLI command:

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,DebugLevel=info" ESP-Arduino/03_WiFi_test
arduino-cli upload -p <PORT> --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,DebugLevel=info" ESP-Arduino/03_WiFi_test
```

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_03_WiFi_test_esp_arduino_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esp_arduino_factory.bin
```
