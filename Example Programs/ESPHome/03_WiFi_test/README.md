# 03_WiFi_test

This ESPHome example uses LVGL to test WiFi provisioning on ONX2424G013.

## Features

- Reuses the stable ESPHome LCD/LVGL configuration from the 01/02 examples.
- Shows the same LCD UI layout as the ESP-IDF 03 example.
- Starts an open AP named `ONX2424G013`.
- Runs a custom captive portal on `192.168.4.1`.
- Uses the ESPHome `wifi` component to own AP/STA state so Native API, mDNS, and Home Assistant see the correct connected state.
- Uses the local ESPHome external component in `components/wifi_portal` for DNS, HTTP, the WiFi scan page, and provisioning submit logic.
- The portal scans nearby WiFi networks on entry and supports manual refresh.
- The user can select an SSID, enter the password, and connect the device.
- The LCD shows the selected WiFi name, connection state, and assigned STA IP.
- Enables ESPHome Native API without encryption or password for Home Assistant discovery.

## Key Settings

Edit the YAML substitutions if the LCD orientation needs adjustment:

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
```

The AP and captive portal constants are in `components/wifi_portal/wifi_portal.h`:

```cpp
constexpr const char *WIFI_AP_SSID = "ONX2424G013";
constexpr const char *WIFI_PORTAL_IP = "192.168.4.1";
```

The AP is intentionally open because this is a provisioning test example.

Home Assistant discovery is enabled with the standard ESPHome Native API:

```yaml
network:
  enable_ipv6: false

wifi:
  ap:
    ssid: "ONX2424G013"
    ap_timeout: 0s
  reboot_timeout: 0s
  power_save_mode: none

api:
  reboot_timeout: 0s
```

No `encryption` key and no API `password` are configured. After provisioning, the ESPHome `wifi` component reports the connected network state, so Home Assistant can discover and add the device without an API key.

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
- `/connect` accepts `ssid` and `password`, saves them through the ESPHome `wifi` component, then starts the STA connection
- `/status` returns selected SSID, connection state, and STA IP

## Implementation Notes

The ESPHome version must not read scan records in the HTTP `/scan` handler the same way as the ESP-IDF and ESP-Arduino versions. ESPHome's `wifi` component also listens for `WIFI_EVENT_SCAN_DONE` and reads the IDF scan records in its event handler; that read consumes the low-level scan result buffer.

For that reason, `components/wifi_portal/__init__.py` calls `wifi.request_wifi_scan_results()` so ESPHome keeps the full scan result cache. The `/scan` handler starts a normal low-level scan, then reads `wifi::global_wifi_component->get_scan_result()` and converts that cached list into the web page JSON response. This keeps the portal WiFi list working while still allowing the ESPHome `wifi` component to own STA state, mDNS, and Native API connectivity.

When the user submits credentials, `/connect` saves and starts the STA connection through the ESPHome `wifi` component instead of permanently bypassing it. After provisioning, Home Assistant can therefore see the correct online state.

## Build And Upload

```bash
esphome run 03_WiFi_test.yaml
```

If PlatformIO rejects a local path that contains spaces, compile from a temporary path without spaces or move/copy the example to a path without spaces. The source YAML itself does not require any generated files. This example was verified with ESPHome `2026.6.2`.

Recommended hardware configuration: ESP32-S3, 16 MB flash, 8 MB OPI PSRAM.

## Full Binary

After compiling, copy the generated factory image into this example directory as:

```text
ONX2424G013_03_WiFi_test_esphome_factory.bin
```

The factory image is a complete image starting at flash address `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esphome_factory.bin
```

In an ESPHome/PlatformIO build directory, the source image is usually:

```text
.esphome/build/onx2424g013-03-wifi-test/.pioenvs/onx2424g013-03-wifi-test/firmware.factory.bin
```

The example directory should only keep source files, documentation, and the full factory image above. `.esphome`, `.pioenvs`, `managed_components`, and similar folders are generated build outputs and should be regenerated when needed instead of being kept with the example.
