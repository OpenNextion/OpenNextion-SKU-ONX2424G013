#pragma once

#define LCD_PIN_SCLK 5
#define LCD_PIN_MOSI 1
#define LCD_PIN_CS 2
#define LCD_PIN_DC 3
#define LCD_PIN_RST 8
#define LCD_PIN_BL 6

#define LCD_H_RES 240
#define LCD_V_RES 240
#define LCD_SPI_CLOCK_HZ 40000000

/* 25 kHz keeps the backlight PWM above the range that caused visible angle-dependent striping. */
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100

/* Captive portal AP settings. The AP is intentionally open for provisioning tests. */
#define WIFI_AP_SSID "ONX2424G013"
#define WIFI_PORTAL_IP "192.168.4.1"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONN 4
#define WIFI_SCAN_MAX_AP 20

/* Display orientation options. Arduino_GFX exposes rotation but not a public mirror API. */
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
