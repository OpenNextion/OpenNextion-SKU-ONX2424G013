#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "driver/ledc.h"

// Keep this sketch self-contained when the Arduino LVGL library has no global lv_conf.h.
// build_opt.h applies the same LVGL options to the library C files.
#define LV_CONF_SKIP
#include <lvgl.h>

#include "lcd_config.h"

// The Arduino version intentionally uses the standard Arduino_GFX GC9A01 driver.
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED, FSPI);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, LCD_PIN_RST, 0, true, LCD_H_RES, LCD_V_RES);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf1[LCD_H_RES * 40];
static lv_color_t lv_buf2[LCD_H_RES * 40];
static lv_obj_t *button_label;

static uint32_t last_key_change_ms = 0;
static bool last_raw_pressed = false;
static bool stable_pressed = false;

static void init_serial_log()
{
    // With "USB CDC On Boot: Enabled", Serial is the USB CDC/JTAG log stream.
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(50);
    Serial.println("[LCD_test] boot");
}

static void apply_orientation()
{
    /* Use the Arduino_GFX GC9A01 public API for panel orientation.
       The driver owns the controller init sequence and MADCTL register writes. */
#if LCD_ROTATION_DEGREE == 0
    gfx->setRotation(0);
#elif LCD_ROTATION_DEGREE == 90
    gfx->setRotation(1);
#elif LCD_ROTATION_DEGREE == 180
    gfx->setRotation(2);
#elif LCD_ROTATION_DEGREE == 270
    gfx->setRotation(3);
#else
#error "LCD_ROTATION_DEGREE must be 0, 90, 180, or 270"
#endif
}

static void set_backlight(uint8_t percent)
{
    percent = min<uint8_t>(percent, 100);
    // LEDC is configured with 11-bit resolution, matching the ESP-IDF example.
    uint32_t duty = (2047 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void init_backlight()
{
    // Use the ESP-IDF LEDC API inside Arduino so startup behavior matches ESP-IDF.
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_11_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = LCD_BACKLIGHT_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
}

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    // LVGL renders into RGB565 buffers; Arduino_GFX transfers the changed area to GC9A01.
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
    lv_disp_flush_ready(disp);
}

static void lvgl_key_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    bool raw_pressed = digitalRead(LCD_PIN_KEY) == LOW;
    uint32_t now = millis();

    // Debounce the active-low KEY input before handing it to LVGL.
    if (raw_pressed != last_raw_pressed) {
        last_raw_pressed = raw_pressed;
        last_key_change_ms = now;
    }

    if ((now - last_key_change_ms) > 25) {
        stable_pressed = raw_pressed;
    }

    // Expose GPIO9 as a native LVGL keypad ENTER key so button states/clicks stay LVGL-managed.
    data->key = LV_KEY_ENTER;
    data->state = stable_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_label_set_text(button_label, "KEY OK");
        Serial.println("[LCD_test] KEY confirmed");
    }
}

static void create_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LCD_test");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -42);

    lv_obj_t *button = lv_btn_create(scr);
    lv_obj_set_size(button, 118, 46);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 32);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, NULL);

    button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Confirm");
    lv_obj_center(button_label);

    // A keypad input device needs a group so LVGL can route ENTER to the button.
    lv_group_t *group = lv_group_create();
    lv_group_add_obj(group, button);
    lv_group_focus_obj(button);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = lvgl_key_read_cb;
    lv_indev_t *keypad = lv_indev_drv_register(&indev_drv);
    lv_indev_set_group(keypad, group);
}

void setup()
{
    init_serial_log();

    // Keep the backlight off while the panel and LVGL draw their first frame.
    init_backlight();
    set_backlight(0);

    pinMode(LCD_PIN_KEY, INPUT_PULLUP);

    gfx->begin(LCD_SPI_CLOCK_HZ);
    apply_orientation();
    gfx->fillScreen(RGB565_BLACK);
    delay(100);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, LCD_H_RES * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    create_ui();
    lv_timer_handler();
    // Turn on the backlight only after the first LVGL frame is available.
    set_backlight(LCD_BRIGHTNESS_PERCENT);
    Serial.println("[LCD_test] LCD and LVGL ready");
}

void loop()
{
    // Arduino has no built-in LVGL tick source in this sketch, so advance it here.
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
}
