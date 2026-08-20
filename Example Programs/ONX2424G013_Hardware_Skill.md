# ONX2424G013 Hardware Skill

## Role

You are a firmware development assistant for ONX2424G013. Use this document to understand hardware resources, interface contracts, and bring-up constraints before writing firmware, drivers, tests, or application logic for this hardware.

This document describes hardware facts only. It is not tied to any specific software platform.

## Hardware Overview

ONX2424G013 is a 1.28-inch rotary display device based on ESP32-S3R8. It includes a round LCD, rotary encoder, KEY button, BOOT button, USB-C, physical UART, and an 8-pin GPIO expansion header.

Core capabilities:

- MCU: ESP32-S3R8
- Flash: 16MB
- PSRAM: 8MB
- Display: 1.28-inch round color LCD
- Display driver IC: GC9A01N
- Resolution: 240 x 240
- Display interface: SPI
- Pixel format: RGB565
- Knob: two-phase quadrature encoder
- Buttons: KEY and BOOT
- Communication: USB-C and physical UART
- Expansion: J4 8-pin GPIO header

## Pin Map

| Function | GPIO | Direction | Constraint |
| --- | ---: | --- | --- |
| LCD SCLK | GPIO5 | Output | SPI clock |
| LCD MOSI | GPIO1 | Output | SPI data output |
| LCD CS | GPIO2 | Output | LCD chip select |
| LCD DC | GPIO3 | Output | LCD data/command select |
| LCD RESET | GPIO8 | Output | LCD reset |
| LCD BL | GPIO6 | Output | PWM backlight |
| Encoder A | GPIO48 | Input | Pull-up, any edge |
| Encoder B | GPIO47 | Input | Pull-up, any edge |
| KEY | GPIO9 | Input | Active low, pull-up |
| BOOT | GPIO0 | Input | Active low, pull-up, boot strap pin |
| UART TX | GPIO43 | Output | Physical UART |
| UART RX | GPIO44 | Input | Physical UART |
| USB DM | GPIO19 | Bidirectional | USB D-, also exposed on J4 |
| USB DP | GPIO20 | Bidirectional | USB D+, also exposed on J4 |
| J4 IO11 | GPIO11 | Bidirectional | Expansion GPIO |
| J4 IO18 | GPIO18 | Bidirectional | Expansion GPIO |
| J4 IO17 | GPIO17 | Bidirectional | Expansion GPIO |
| J4 IO10 | GPIO10 | Bidirectional | Expansion GPIO |

## LCD Contract

The LCD uses SPI and has a 240 x 240 resolution.

The display driver IC is GC9A01N. GC9A01/GC9A01A-compatible software drivers may be used after validation on real hardware.

| LCD signal | GPIO |
| --- | ---: |
| SCLK | GPIO5 |
| MOSI | GPIO1 |
| CS | GPIO2 |
| DC | GPIO3 |
| RESET | GPIO8 |
| BL | GPIO6 |

LCD driver rules:

- Use SPI write-only display output.
- Use a GC9A01N/GC9A01-compatible initialization sequence or an official/mature platform panel driver.
- Do not rely on LCD MISO reads.
- Use RGB565 pixel format.
- Support full-screen 240 x 240 refresh.
- Use BGR color order.
- Enable display color inversion.
- Use SPI Mode 0.
- Start with 40MHz SPI for stability, then increase only after real hardware validation.
- Avoid hand-writing display controller registers when a platform driver already exposes a supported API for the required behavior. In particular, do not patch MADCTL directly in application code unless there is no driver API and the register sequence has been validated on hardware.

LCD initialization order:

1. Configure backlight PWM with duty 0 so the panel stays dark during initialization.
2. Initialize the SPI bus.
3. Configure LCD CS, DC, and RESET.
4. Reset the LCD.
5. Initialize the LCD controller.
6. Configure pixel format, color order, and inversion.
7. Configure orientation/mirroring through the platform driver's public API.
8. Enable display output.
9. Render or clear the first valid frame.
10. Turn on the backlight after the first valid frame is ready.

Backlight rules:

- Backlight is controlled by PWM on GPIO6.
- Recommended PWM frequency is 25kHz.
- Expose brightness as 0-100% in software.
- Higher duty means higher brightness.
- Keep the backlight off during early boot to avoid showing an uninitialized frame.
- Use 11-bit LEDC resolution with 25kHz where possible. Lower PWM frequencies can produce subtle viewing-angle-dependent vertical stripe artifacts on this LCD/backlight assembly.
- Avoid briefly driving GPIO6 as a plain HIGH output before PWM is configured; it can expose LCD controller initialization or stale frame contents as a visible flash.

## Rotary Encoder Contract

The knob is a two-phase quadrature encoder.

| Signal | GPIO |
| --- | ---: |
| A | GPIO48 |
| B | GPIO47 |

Encoder rules:

- Configure both A and B as input pull-ups.
- Use interrupts on any edge for both A and B.
- Use four-state quadrature decoding.
- Apply debounce or invalid-transition filtering.
- Treat detents as the business-level unit rather than treating every edge as one user step.
- Validate clockwise/counterclockwise direction on physical hardware; keep direction inversion possible in software.

Recommended decode constraints:

- Valid states: 00, 01, 11, 10.
- A single detent usually produces multiple state transitions.
- Read the initial A/B state before enabling interrupts.

## Button Contract

| Button | GPIO | Active level | Constraint |
| --- | ---: | --- | --- |
| KEY | GPIO9 | Low | General function button |
| BOOT | GPIO0 | Low | General button and boot mode strap |

Button rules:

- Use input pull-up for both buttons.
- Pressed state is low.
- Debounce is required.
- BOOT is a boot strap pin and must not be externally forced during reset.

## Physical UART Contract

| Signal | GPIO |
| --- | ---: |
| TX | GPIO43 |
| RX | GPIO44 |

UART rules:

- Use 8N1.
- Use no hardware flow control by default.
- 115200 is suitable for basic debug or ordinary communication.
- 921600 may be used for high-speed communication after validating cable length, adapter quality, and error rate.
- Physical UART and USB-C are separate interfaces.

## USB-C Contract

USB-C is connected to the ESP32-S3 native USB interface.

| USB signal | GPIO |
| --- | ---: |
| DM | GPIO19 |
| DP | GPIO20 |

USB rules:

- USB-C may be used for flashing, logs, or serial communication.
- GPIO19/GPIO20 are USB DP/DM and must not be driven as ordinary GPIO while USB is active.
- Any logic that drives GPIO19/GPIO20 must first ensure USB functionality is not required.

## J4 Expansion GPIO Contract

J4 is an 8-pin expansion header.

| J4 signal | Mapping |
| --- | --- |
| DP | GPIO20 / USB DP |
| DM | GPIO19 / USB DM |
| IO11 | GPIO11 |
| GND | GND |
| IO18 | GPIO18 |
| IO17 | GPIO17 |
| IO10 | GPIO10 |
| VCC | Power |

Reference J4 GPIO order:

1. IO11
2. DM(GPIO19)
3. DP(GPIO20)
4. IO18
5. IO17
6. IO10

J4 rules:

- Set all related IOs to a safe default state before use.
- Do not assume J4 VCC voltage or current capability without hardware confirmation.
- DP/DM on J4 conflict with USB-C usage.

## Wi-Fi Capability

Wi-Fi is provided by ESP32-S3.

Rules:

- Wi-Fi may be used for network connection, scanning, and RSSI reading.
- RSSI should be read after connecting to the target Wi-Fi network.
- Do not treat Wi-Fi connection failure as direct proof of hardware failure; distinguish scan miss, wrong password, authentication failure, weak signal, and timeout.

## Power-On And Boot Constraints

Hardware boot rules:

- GPIO0 participates in boot mode selection.
- GPIO3 is boot-sensitive and is also used as LCD DC; avoid disturbing it during reset.
- USB-C can be used for flashing and logs, but GPIO19/GPIO20 are shared with J4.
- LCD backlight should be enabled only after LCD initialization and a valid first frame.
- If PSRAM is enabled, confirm PSRAM initialization before allocating large display buffers.

## Bring-Up Order

Recommended bring-up order:

1. Configure ESP32-S3R8, 16MB Flash, and 8MB PSRAM.
2. Enable a reliable log output.
3. Initialize backlight PWM at 0% duty, then initialize LCD SPI and the panel.
4. Display black, white, red, green, and blue full-screen colors to validate color order, inversion, and refresh.
5. Validate KEY(GPIO9).
6. Validate BOOT(GPIO0).
7. Validate rotary encoder GPIO48/GPIO47 count and direction.
8. Validate physical UART GPIO43/GPIO44.
9. Validate Wi-Fi scan, connection, and RSSI.
10. Validate J4 GPIO only when USB-C usage does not conflict.

## Platform Development Guidance

Firmware for this hardware does not need to implement every low-level driver from scratch. Prefer existing peripheral frameworks, display drivers, LVGL ports, GPIO/encoder/UART/Wi-Fi components, and add custom code only where platform support is missing or behavior is insufficient.

### ESP-IDF

Recommended building blocks:

- LCD: Use the `esp_lcd` framework with a GC9A01N-compatible round SPI LCD panel driver.
- UI: Use LVGL with an LCD flush callback through the platform LVGL port.
- Backlight: Use LEDC PWM.
- Buttons: Use GPIO input, optionally with a button component library.
- Rotary encoder: Use GPIO interrupts with four-state quadrature decoding, or PCNT/RMT-assisted logic if appropriate; keep direction calibration possible.
- UART: Use the UART driver on GPIO43/GPIO44.
- Wi-Fi: Use ESP-IDF station/scanner APIs.

ESP-IDF configuration notes:

- Target chip must be ESP32-S3.
- Flash size must be 16MB.
- PSRAM should be configured as 8MB Octal PSRAM.
- Prefer Espressif's managed/official GC9A01 panel component with the `esp_lcd` framework. Do not replace it with a handwritten display driver unless the platform component is unavailable.
- Treat the LCD as SPI write-only and do not depend on MISO.
- Start LCD SPI at 40MHz and increase only after real hardware validation.
- Configure the panel for BGR element order, RGB565, and color inversion.
- Apply mirroring/rotation with `esp_lcd_panel_mirror()` and `esp_lcd_panel_swap_xy()` after panel initialization.
- Configure GPIO6 backlight with LEDC at 25kHz and 11-bit duty resolution; turn it on only after LVGL has produced the first frame.
- Place large display buffers in PSRAM when useful, but keep ISR and timing-critical paths out of PSRAM.
- If USB Serial/JTAG logging is enabled, do not drive GPIO19/GPIO20 as ordinary GPIO outputs at the same time.

### ESP-Arduino

Recommended building blocks:

- LCD: Use a mature graphics library with GC9A01N-compatible round SPI LCD support, such as Arduino_GFX, LovyanGFX, or TFT_eSPI, instead of writing the whole SPI display stack from scratch.
- UI: Use LVGL or the native drawing API of the selected graphics library.
- Backlight: Use LEDC PWM.
- Buttons: Use GPIO input with debounce.
- Rotary encoder: Use a mature encoder library or interrupt-based four-state decoding.
- UART: Use `HardwareSerial` mapped to GPIO43/GPIO44.
- Wi-Fi: Use the Arduino WiFi API.

ESP-Arduino configuration notes:

- Select an ESP32-S3 board/target.
- Set Flash size to 16MB.
- Enable OPI/Octal PSRAM and confirm it is available at runtime.
- For Arduino IDE with the ESP32 board package, use an ESP32-S3 board profile configured for 16MB Flash and OPI/Octal PSRAM. Choose a partition scheme that matches the application storage needs and the 16MB flash size.
- Keep USB CDC On Boot disabled unless the application explicitly needs it and the startup/display behavior has been revalidated.
- Choose USB CDC/USB Serial/JTAG mode according to the product need; USB use conflicts with driving GPIO19/GPIO20 as ordinary GPIO.
- Configure the graphics library for 240x240 resolution, SPI pins, CS/DC/RESET, BGR color order, and color inversion.
- When using Arduino_GFX on ESP32-S3, use the library's standard `Arduino_GC9A01` driver and an ESP32-S3-compatible SPI host such as `FSPI`.
- Do not directly call private or unavailable display methods such as `writeCommand()` on the high-level `Arduino_GFX` object. Use the graphics library's public API.
- Do not write GC9A01 MADCTL or other panel registers from application code to force mirroring; Arduino_GFX does not expose a public mirror API for this driver. If mirror support is required, use a library/driver that exposes it or validate a proper driver-level implementation.
- Use ESP-IDF LEDC APIs inside Arduino when precise startup behavior is needed: configure the LEDC timer/channel at 25kHz and 11-bit resolution, duty 0 first, then enable brightness after the first LVGL frame.
- Keep LVGL configuration project-local and make sure Arduino IDE and Arduino CLI compile LVGL C/C++ files with the same options, including any fonts required by the application UI.
- If colors are wrong, check BGR/RGB and inversion before assuming hardware damage.

### ESPHome

Recommended building blocks:

- LCD: Use an existing GC9A01N/GC9A01-compatible SPI LCD display component or a compatible custom component; avoid implementing the full display driver from scratch when a compatible component exists.
- UI: Use LVGL for pages and widgets.
- Backlight: Use LEDC output or a light abstraction.
- Buttons: Use GPIO binary sensors.
- Rotary encoder: Use the rotary encoder component; if direction is reversed, swap A/B or invert the logic layer.
- UART: Use the UART component on GPIO43/GPIO44.
- Wi-Fi: Use platform Wi-Fi configuration and RSSI/status interfaces.

ESPHome configuration notes:

- The `esp32` target should be ESP32-S3.
- `flash_size` should be 16MB.
- PSRAM should be configured as 8MB Octal PSRAM.
- Keep LCD SPI pins fixed as defined in this document.
- Start LCD SPI at 40MHz.
- Display configuration should use 240x240, BGR, and color inversion.
- Configure LCD transform as needed by the mounted panel. The validated base orientation uses mirror_x enabled, mirror_y disabled, and swap_xy disabled.
- Configure LVGL with a full-size buffer when possible for stable refresh behavior on this small display.
- Configure KEY(GPIO9) as an active-low binary sensor. In LVGL applications, route the key through an LVGL input device so LVGL manages focus, press state, and click handling rather than manually changing UI state from raw GPIO callbacks.
- Use 25kHz LEDC PWM for the backlight. Lower PWM frequencies may show subtle viewing-angle-dependent vertical stripe artifacts.
- Keep the backlight off by default and enable it after the first valid frame.
- USB logging/communication configuration affects GPIO19/GPIO20; do not drive DP/DM as J4 GPIO while USB is active.

### Touch Input Note

This hardware Skill does not define a touch input. Do not enable a touch driver just because a platform provides one.

If a future hardware variant needs touch:

- First confirm the touch controller, I2C/SPI pins, interrupt pin, and reset pin.
- Then select an existing platform touch component.
- Until confirmed, do not reserve GPIO7/GPIO8 or any defined pin for touch.

## AI Development Rules

Future AI coding must obey:

- Do not reassign defined hardware pins.
- Do not treat USB-C and physical UART as the same interface.
- Do not drive GPIO19/GPIO20 while USB-C is active.
- Do not assume J4 VCC voltage or current capability.
- Do not assume LCD readback is available; treat the LCD as SPI write-only.
- Do not replace a working official/platform display driver with a handwritten one.
- Do not manually write display controller registers from application code when a public driver API exists.
- Do not change board memory configuration to 4MB Flash or disabled PSRAM; this hardware is ESP32-S3R8 with 16MB Flash and 8MB Octal PSRAM.
- Do not change Arduino USB CDC or partition options during display debugging unless the change itself is being tested.
- Do not assume rotary direction; keep direction calibration possible.
- Do not turn on the backlight before the first valid display frame is ready.
- Do not use backlight GPIO HIGH/LOW probing as a final implementation. Configure LEDC PWM at 25kHz, duty 0 during startup, then set the target brightness.
- Do not treat BOOT(GPIO0) as an unconstrained ordinary GPIO.
- Do not introduce platform-specific concepts as hardware facts.

## Minimal Pin Constants

```c
#define ONX_LCD_SCLK_GPIO      5
#define ONX_LCD_MOSI_GPIO      1
#define ONX_LCD_CS_GPIO        2
#define ONX_LCD_DC_GPIO        3
#define ONX_LCD_RESET_GPIO     8
#define ONX_LCD_BACKLIGHT_GPIO 6

#define ONX_ENCODER_A_GPIO     48
#define ONX_ENCODER_B_GPIO     47

#define ONX_KEY_GPIO           9
#define ONX_BOOT_GPIO          0

#define ONX_UART_TX_GPIO       43
#define ONX_UART_RX_GPIO       44

#define ONX_USB_DM_GPIO        19
#define ONX_USB_DP_GPIO        20

#define ONX_J4_IO11_GPIO       11
#define ONX_J4_IO18_GPIO       18
#define ONX_J4_IO17_GPIO       17
#define ONX_J4_IO10_GPIO       10
```
