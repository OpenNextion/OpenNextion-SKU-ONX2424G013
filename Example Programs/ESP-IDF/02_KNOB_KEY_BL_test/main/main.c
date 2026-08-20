#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "lcd_config.h"

#if KNOB_EDGES_PER_BRIGHTNESS_STEP < 1
#error "KNOB_EDGES_PER_BRIGHTNESS_STEP must be >= 1"
#endif

static const char *TAG = "KNOB_KEY_BL";
static esp_lcd_panel_handle_t lcd_panel;
static lv_disp_drv_t disp_drv;
static lv_obj_t *arc;
static lv_obj_t *current_label;
static lv_obj_t *target_label;

/* current_brightness is the applied PWM duty; target_brightness is edited by the encoder. */
static uint8_t current_brightness = LCD_BRIGHTNESS_PERCENT;
static uint8_t target_brightness = LCD_BRIGHTNESS_PERCENT;
/* Keeps partial encoder movement until it reaches the configured sensitivity threshold. */
static int32_t encoder_remainder;

static portMUX_TYPE encoder_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int32_t encoder_delta_edges;
static volatile uint8_t encoder_last_state;

static int64_t last_key_change_us;
static bool last_raw_key_pressed;
static bool stable_key_pressed;
static bool previous_key_pressed;

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

static bool lvgl_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx)
{
    LV_UNUSED(panel_io);
    LV_UNUSED(event_data);
    /* SPI color transfers are queued asynchronously; notify LVGL only after the panel IO callback fires. */
    lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map));
    LV_UNUSED(drv);
}

static void lv_tick_task(void *arg)
{
    LV_UNUSED(arg);
    lv_tick_inc(1);
}

static void set_backlight(uint8_t percent)
{
    uint32_t duty = 0;
    if (percent > 0) {
        percent = clamp_brightness(percent);
        /* Match ESPHome's default gamma-corrected brightness curve while keeping the UI in percent. */
        float normalized = (float)percent / 100.0f;
        duty = (uint32_t)((2047.0f * powf(normalized, LCD_BACKLIGHT_GAMMA)) + 0.5f);
    }
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

static void update_ui(void)
{
    char text[32];
    lv_arc_set_value(arc, target_brightness);

    snprintf(text, sizeof(text), "Current\n%u%%", current_brightness);
    lv_label_set_text(current_label, text);

    snprintf(text, sizeof(text), "Target\n%u%%", target_brightness);
    lv_label_set_text(target_label, text);
}

static void apply_target_brightness(void)
{
    current_brightness = target_brightness;
    set_backlight(current_brightness);
    update_ui();
}

static void create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    /* Keep the Arc passive: the physical encoder is the only control for the target value. */
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

static void IRAM_ATTR encoder_isr(void *arg)
{
    LV_UNUSED(arg);
    /* Quadrature state table filters illegal transitions and keeps ISR work small. */
    static const int8_t transition_table[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0,
    };
    uint8_t state = ((gpio_get_level(LCD_PIN_ENCODER_A) & 1) << 1) | (gpio_get_level(LCD_PIN_ENCODER_B) & 1);
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

static int32_t take_encoder_delta(void)
{
    portENTER_CRITICAL(&encoder_mux);
    int32_t delta = encoder_delta_edges;
    encoder_delta_edges = 0;
    portEXIT_CRITICAL(&encoder_mux);
    return delta;
}

static bool read_key_pressed(void)
{
    bool raw_pressed = gpio_get_level(LCD_PIN_KEY) == 0;
    int64_t now = esp_timer_get_time();

    /* GPIO9 is an active-low key; debounce here so short press confirmation is stable. */
    if (raw_pressed != last_raw_key_pressed) {
        last_raw_key_pressed = raw_pressed;
        last_key_change_us = now;
    }

    if ((now - last_key_change_us) > 25000) {
        stable_key_pressed = raw_pressed;
    }

    return stable_key_pressed;
}

static void process_inputs(void)
{
    encoder_remainder += take_encoder_delta();
    while (encoder_remainder >= KNOB_EDGES_PER_BRIGHTNESS_STEP) {
        target_brightness = clamp_brightness(target_brightness + 1);
        encoder_remainder -= KNOB_EDGES_PER_BRIGHTNESS_STEP;
        update_ui();
    }
    while (encoder_remainder <= -KNOB_EDGES_PER_BRIGHTNESS_STEP) {
        target_brightness = clamp_brightness(target_brightness - 1);
        encoder_remainder += KNOB_EDGES_PER_BRIGHTNESS_STEP;
        update_ui();
    }

    bool key_pressed = read_key_pressed();
    if (previous_key_pressed && !key_pressed) {
        /* Apply on key release after a short press, matching the other platform examples. */
        apply_target_brightness();
    }
    previous_key_pressed = key_pressed;
}

static void init_gpio(void)
{
    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << LCD_PIN_KEY) | (1ULL << LCD_PIN_ENCODER_A) | (1ULL << LCD_PIN_ENCODER_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_conf));

    encoder_last_state = ((gpio_get_level(LCD_PIN_ENCODER_A) & 1) << 1) | (gpio_get_level(LCD_PIN_ENCODER_B) & 1);
    gpio_set_intr_type(LCD_PIN_ENCODER_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(LCD_PIN_ENCODER_B, GPIO_INTR_ANYEDGE);
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(LCD_PIN_ENCODER_A, encoder_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(LCD_PIN_ENCODER_B, encoder_isr, NULL));
}

static void init_backlight(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_11_BIT,
        .freq_hz = LCD_BACKLIGHT_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        /* Start dark and enable the configured brightness only after the first LVGL frame. */
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void apply_panel_orientation(void)
{
    bool mirror_x = LCD_MIRROR_X;
    bool mirror_y = LCD_MIRROR_Y;
    bool swap_xy = false;

#if LCD_ROTATION_DEGREE == 90
    swap_xy = true;
    mirror_x = !mirror_x;
#elif LCD_ROTATION_DEGREE == 180
    mirror_x = !mirror_x;
    mirror_y = !mirror_y;
#elif LCD_ROTATION_DEGREE == 270
    swap_xy = true;
    mirror_y = !mirror_y;
#elif LCD_ROTATION_DEGREE != 0
#error "LCD_ROTATION_DEGREE must be 0, 90, 180, or 270"
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, mirror_x, mirror_y));
}

static void init_lcd_panel(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = lvgl_flush_ready_cb,
        .user_ctx = &disp_drv,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    /* Use Espressif's official GC9A01 panel component instead of a handwritten init table. */
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    apply_panel_orientation();
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));
}

static void init_lvgl(void)
{
    lv_init();

    static lv_color_t buf1[LCD_H_RES * 40];
    static lv_color_t buf2[LCD_H_RES * 40];
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_H_RES * 40);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_task,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Start ONX2424G013 KNOB_KEY_BL_test");
    init_gpio();
    init_backlight();
    init_lcd_panel();
    init_lvgl();
    create_ui();

    lv_timer_handler();
    /* The first frame is ready, so it is safe to bring the backlight up without a white flash. */
    set_backlight(current_brightness);

    while (true) {
        process_inputs();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
