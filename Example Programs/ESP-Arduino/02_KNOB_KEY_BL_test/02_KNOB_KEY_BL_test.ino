#include <Arduino.h>
#include <math.h>
#include <Arduino_GFX_Library.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

// build_opt.h applies the same LVGL options to the library C files.
#define LV_CONF_SKIP
#include <lvgl.h>

#include "lcd_config.h"

#if KNOB_EDGES_PER_BRIGHTNESS_STEP < 1
#error "KNOB_EDGES_PER_BRIGHTNESS_STEP must be >= 1"
#endif

// Reuse the same standard Arduino_GFX GC9A01 path as 01_LCD_test.
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED, FSPI);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, LCD_PIN_RST, 0, true, LCD_H_RES, LCD_V_RES);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf1[LCD_H_RES * 40];
static lv_color_t lv_buf2[LCD_H_RES * 40];
static lv_obj_t *arc;
static lv_obj_t *current_label;
static lv_obj_t *target_label;

// current_brightness is the applied PWM duty; target_brightness is edited by the encoder.
static uint8_t current_brightness = LCD_BRIGHTNESS_PERCENT;
static uint8_t target_brightness = LCD_BRIGHTNESS_PERCENT;
// Keeps partial encoder movement until it reaches the configured sensitivity threshold.
static int32_t encoder_remainder;

static portMUX_TYPE encoder_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int32_t encoder_delta_edges;
static volatile uint8_t encoder_last_state;

static uint32_t last_key_change_ms;
static bool last_raw_pressed;
static bool stable_pressed;
static bool previous_key_pressed;

static void init_serial_log()
{
    // With "USB CDC On Boot: Enabled", Serial is the USB CDC/JTAG log stream.
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(50);
    Serial.println("[KNOB_KEY_BL_test] boot");
}

static void log_status(const char *message)
{
    Serial.println(message);
}

static uint8_t clamp_brightness(int32_t value)
{
    if (value < LCD_BRIGHTNESS_MIN_PERCENT) {
        return LCD_BRIGHTNESS_MIN_PERCENT;
    }
    if (value > 100) {
        return 100;
    }
    return (uint8_t)value;
}

static void apply_orientation()
{
    /* Use Arduino_GFX public rotation API. The driver owns GC9A01 init and MADCTL writes. */
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
    uint32_t duty = 0;
    if (percent > 0) {
        percent = clamp_brightness(percent);
        // Match ESPHome's default gamma-corrected brightness curve while keeping the UI in percent.
        float normalized = (float)percent / 100.0f;
        duty = (uint32_t)((2047.0f * powf(normalized, LCD_BACKLIGHT_GAMMA)) + 0.5f);
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void init_backlight()
{
    // Match the ESP-IDF example: 25 kHz, 11-bit LEDC, duty 0 during startup.
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
        // Start dark and enable the configured brightness only after the first LVGL frame.
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
}

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;
    // Arduino_GFX owns the GC9A01 SPI transfer; LVGL only supplies the dirty rectangle.
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
    lv_disp_flush_ready(disp);
}

static void update_ui()
{
    char text[32];
    lv_arc_set_value(arc, target_brightness);

    snprintf(text, sizeof(text), "Current\n%u%%", current_brightness);
    lv_label_set_text(current_label, text);

    snprintf(text, sizeof(text), "Target\n%u%%", target_brightness);
    lv_label_set_text(target_label, text);
}

static void apply_target_brightness()
{
    // The encoder edits only the target. KEY release commits it to the real PWM duty.
    current_brightness = target_brightness;
    set_backlight(current_brightness);
    update_ui();
    Serial.printf("[KNOB_KEY_BL_test] confirmed target brightness: %u%%\n", current_brightness);
}

static void create_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    // Keep the Arc passive: the physical encoder is the only control for the target value.
    arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 220, 220);
    lv_obj_center(arc);
    lv_arc_set_range(arc, LCD_BRIGHTNESS_MIN_PERCENT, 100);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x2A3441), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x46C2FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);

    current_label = lv_label_create(scr);
    lv_obj_set_style_text_color(current_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(current_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(current_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(current_label, 4, 0);
    lv_obj_align(current_label, LV_ALIGN_CENTER, 0, -26);

    target_label = lv_label_create(scr);
    lv_obj_set_style_text_color(target_label, lv_color_hex(0x46C2FF), 0);
    lv_obj_set_style_text_font(target_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(target_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(target_label, 4, 0);
    lv_obj_align(target_label, LV_ALIGN_CENTER, 0, 34);

    update_ui();
}

static void IRAM_ATTR encoder_isr()
{
    // Quadrature state table filters illegal transitions and keeps ISR work small.
    static const int8_t transition_table[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0,
    };
    uint8_t state = ((gpio_get_level((gpio_num_t)LCD_PIN_ENCODER_A) & 1) << 1) |
                    (gpio_get_level((gpio_num_t)LCD_PIN_ENCODER_B) & 1);
    uint8_t index = (encoder_last_state << 2) | state;
    int8_t delta = transition_table[index];
    encoder_last_state = state;

#if KNOB_DIRECTION_INVERT
    delta = -delta;
#endif

    if (delta != 0) {
        portENTER_CRITICAL_ISR(&encoder_mux);
        encoder_delta_edges += delta;
        portEXIT_CRITICAL_ISR(&encoder_mux);
    }
}

static int32_t take_encoder_delta()
{
    portENTER_CRITICAL(&encoder_mux);
    int32_t delta = encoder_delta_edges;
    encoder_delta_edges = 0;
    portEXIT_CRITICAL(&encoder_mux);
    return delta;
}

static bool read_key_pressed()
{
    bool raw_pressed = digitalRead(LCD_PIN_KEY) == LOW;
    uint32_t now = millis();

    // GPIO9 is an active-low key; debounce here so short press confirmation is stable.
    if (raw_pressed != last_raw_pressed) {
        last_raw_pressed = raw_pressed;
        last_key_change_ms = now;
    }

    if ((now - last_key_change_ms) > 25) {
        stable_pressed = raw_pressed;
    }

    return stable_pressed;
}

static void process_inputs()
{
    encoder_remainder += take_encoder_delta();
    while (encoder_remainder >= KNOB_EDGES_PER_BRIGHTNESS_STEP) {
        uint8_t next_target = clamp_brightness(target_brightness + 1);
        encoder_remainder -= KNOB_EDGES_PER_BRIGHTNESS_STEP;
        if (next_target != target_brightness) {
            target_brightness = next_target;
            update_ui();
        }
    }
    while (encoder_remainder <= -KNOB_EDGES_PER_BRIGHTNESS_STEP) {
        uint8_t next_target = clamp_brightness(target_brightness - 1);
        encoder_remainder += KNOB_EDGES_PER_BRIGHTNESS_STEP;
        if (next_target != target_brightness) {
            target_brightness = next_target;
            update_ui();
        }
    }

    bool key_pressed = read_key_pressed();
    if (previous_key_pressed && !key_pressed) {
        // Apply on key release after a short press, matching the other platform examples.
        apply_target_brightness();
    }
    previous_key_pressed = key_pressed;
}

static void init_inputs()
{
    pinMode(LCD_PIN_KEY, INPUT_PULLUP);
    pinMode(LCD_PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(LCD_PIN_ENCODER_B, INPUT_PULLUP);
    encoder_last_state = ((digitalRead(LCD_PIN_ENCODER_A) & 1) << 1) | (digitalRead(LCD_PIN_ENCODER_B) & 1);
    attachInterrupt(digitalPinToInterrupt(LCD_PIN_ENCODER_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(LCD_PIN_ENCODER_B), encoder_isr, CHANGE);
}

void setup()
{
    init_serial_log();

    init_backlight();
    set_backlight(0);
    init_inputs();

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
    // The first frame is ready, so it is safe to bring the backlight up without a white flash.
    set_backlight(current_brightness);
    log_status("[KNOB_KEY_BL_test] LCD, LVGL, encoder, and KEY ready");
}

void loop()
{
    // Keep LVGL timing local to the sketch so no Arduino timer setup is required.
    lv_tick_inc(5);
    process_inputs();
    lv_timer_handler();
    delay(5);
}
