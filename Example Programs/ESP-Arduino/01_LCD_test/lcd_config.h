#pragma once

#define LCD_PIN_SCLK 5
#define LCD_PIN_MOSI 1
#define LCD_PIN_CS 2
#define LCD_PIN_DC 3
#define LCD_PIN_RST 8
#define LCD_PIN_BL 6
#define LCD_PIN_KEY 9

#define LCD_H_RES 240
#define LCD_V_RES 240
#define LCD_SPI_CLOCK_HZ 40000000
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100

/* Display orientation options. Tune these if the mounted LCD direction differs. */
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
