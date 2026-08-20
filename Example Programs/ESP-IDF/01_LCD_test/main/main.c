#include <stdbool.h>
#include <stdint.h>

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
#include "freertos/task.h"
#include "lvgl.h"

#include "lcd_config.h"

static const char *TAG = "LCD_test";
static esp_lcd_panel_handle_t lcd_panel;
static lv_disp_drv_t disp_drv;
static lv_obj_t *button_label;
static int64_t last_key_change_us;
static bool last_raw_key_pressed;
static bool stable_key_pressed;

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

static void key_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    LV_UNUSED(drv);
    bool raw_pressed = gpio_get_level(LCD_PIN_KEY) == 0;
    int64_t now = esp_timer_get_time();

    /* GPIO9 is an active-low key. Keep the debounce timing aligned with the ESPHome example. */
    if (raw_pressed != last_raw_key_pressed) {
        last_raw_key_pressed = raw_pressed;
        last_key_change_us = now;
    }

    if ((now - last_key_change_us) > 25000) {
        stable_key_pressed = raw_pressed;
    }

    data->key = LV_KEY_ENTER;
    data->state = stable_key_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lv_tick_task(void *arg)
{
    LV_UNUSED(arg);
    lv_tick_inc(1);
}

static void button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_label_set_text(button_label, "KEY OK");
    }
}

static void create_ui(void)
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

    /* Route the physical KEY through LVGL's native keypad ENTER path. */
    lv_group_t *group = lv_group_create();
    lv_group_add_obj(group, button);
    lv_group_focus_obj(button);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = key_read_cb;
    lv_indev_t *keypad = lv_indev_drv_register(&indev_drv);
    lv_indev_set_group(keypad, group);
}

static void init_gpio(void)
{
    gpio_config_t key_conf = {
        .pin_bit_mask = 1ULL << LCD_PIN_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&key_conf));
}

static void init_backlight(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        /* ESPHome selects 11-bit resolution at 25 kHz; match it for comparable brightness behavior. */
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
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void set_backlight(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    /* 11-bit LEDC duty range is 0..2047. */
    uint32_t duty = (2047 * percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
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
    /* Use Espressif's official GC9A01 panel component and its default vendor initialization. */
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
    ESP_LOGI(TAG, "Start ONX2424G013 LCD_test with esp_lcd_gc9a01");
    init_gpio();
    init_backlight();
    init_lcd_panel();
    init_lvgl();
    create_ui();

    lv_timer_handler();
    set_backlight(LCD_BRIGHTNESS_PERCENT);

    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
