# 03_WiFi_test

This ESP-IDF example uses LVGL to test WiFi provisioning on ONX2424G013.

## Features

- Reuses the stable LCD initialization path from the previous ESP-IDF LCD examples.
- Shows `WiFi_test` on the LCD after boot.
- Starts an open AP named `ONX2424G013`.
- Runs a captive portal on `192.168.4.1`.
- Starts a local DNS responder so phone captive-portal probes resolve to the device.
- The portal scans nearby WiFi networks on entry and supports manual refresh.
- The user can select an SSID, enter the password, and connect the device.
- The LCD shows the selected WiFi name, connection state, and assigned STA IP.
- The LCD bottom hint uses short wrapped lines so it stays inside the round screen boundary.

## Usage

1. Build and flash the firmware.
2. Connect a phone to the open WiFi AP `ONX2424G013`.
3. The phone should open the provisioning page automatically.
4. If no page opens, browse to `http://192.168.4.1`.
5. Select a WiFi network, enter its password, and tap **Connect**.
6. Check the LCD for connection status and IP address.

## Key Settings

Edit `main/lcd_config.h` if needed:

```c
#define WIFI_AP_SSID "ONX2424G013"
#define WIFI_PORTAL_IP "192.168.4.1"
#define WIFI_SCAN_MAX_AP 20
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100
```

The AP is intentionally open because this is a provisioning test example.

## Implementation Notes

- The HTTP server stack is increased to handle captive-portal probe bursts from phone operating systems.
- WiFi scan results are allocated from heap instead of the HTTP server task stack.
- `max_open_sockets` is kept at `7` to match the default ESP-IDF/LwIP socket limit.
- The backlight uses 25 kHz LEDC PWM with the APB clock source.

## Build

```bash
idf.py set-target esp32s3
idf.py build
```

Recommended hardware configuration: 16 MB flash, 8 MB OPI PSRAM.

This example uses `partitions.csv` with a 3 MB factory app partition because the WiFi, HTTP captive portal, and LVGL firmware is larger than the ESP-IDF default 1 MB factory partition.

## Full Binary

The prebuilt full-flash image is included as:

```text
ONX2424G013_03_WiFi_test_esp_idf_factory.bin
```

It is a complete image intended to be written from address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esp_idf_factory.bin
```
