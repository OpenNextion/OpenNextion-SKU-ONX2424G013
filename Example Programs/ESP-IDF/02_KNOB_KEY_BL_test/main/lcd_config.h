#pragma once

#define LCD_HOST SPI2_HOST

#define LCD_PIN_SCLK 5
#define LCD_PIN_MOSI 1
#define LCD_PIN_CS 2
#define LCD_PIN_DC 3
#define LCD_PIN_RST 8
#define LCD_PIN_BL 6
#define LCD_PIN_KEY 9
#define LCD_PIN_ENCODER_A 48
#define LCD_PIN_ENCODER_B 47

#define LCD_H_RES 240
#define LCD_V_RES 240
#define LCD_SPI_CLOCK_HZ (40 * 1000 * 1000)

/* 25 kHz keeps the backlight PWM above the range that caused visible angle-dependent striping. */
#define LCD_BACKLIGHT_PWM_HZ 25000
/* User-adjustable brightness range. A value of 0 is still reserved internally for startup/off. */
#define LCD_BRIGHTNESS_MIN_PERCENT 10
#define LCD_BRIGHTNESS_PERCENT 100
/* Match ESPHome's default light gamma so the same percentage looks consistent across platforms. */
#define LCD_BACKLIGHT_GAMMA 2.8f

/* Encoder sensitivity presets. Lower values are more sensitive; higher values require more rotation. */
#define KNOB_SENSITIVITY_HIGH 1
#define KNOB_SENSITIVITY_MEDIUM 2
#define KNOB_SENSITIVITY_LOW 4

/* Default to medium sensitivity. */
#define KNOB_EDGES_PER_BRIGHTNESS_STEP KNOB_SENSITIVITY_MEDIUM
#define KNOB_DIRECTION_INVERT 0

/* Display orientation options. Tune these if the mounted LCD direction differs. */
#define LCD_MIRROR_X 1
#define LCD_MIRROR_Y 0
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
