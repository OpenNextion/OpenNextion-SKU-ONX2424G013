#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "lvgl.h"

#include "lcd_config.h"

static const char *TAG = "WiFi_test";
static esp_lcd_panel_handle_t lcd_panel;
static lv_disp_drv_t disp_drv;
static httpd_handle_t http_server;
static const char captive_portal_uri[] = "http://" WIFI_PORTAL_IP;

static lv_obj_t *title_label;
static lv_obj_t *ssid_label;
static lv_obj_t *status_label;
static lv_obj_t *ip_label;
static lv_obj_t *hint_label;

static SemaphoreHandle_t ui_mutex;
static char ui_ssid[48] = "AP: " WIFI_AP_SSID;
static char ui_status[64] = "Connect phone to AP";
static char ui_ip[48] = "Open: " WIFI_PORTAL_IP;
static char ui_hint[80] = "No password\nPopup/browser\n192.168.4.1";
static bool ui_dirty = true;

static SemaphoreHandle_t wifi_state_mutex;
static char selected_ssid[33] = "";
static char sta_ip[16] = "";
static char wifi_status[32] = "Waiting";

static void set_ui_text(const char *ssid, const char *status, const char *ip, const char *hint)
{
    if (ui_mutex == NULL) {
        return;
    }
    xSemaphoreTake(ui_mutex, portMAX_DELAY);
    if (ssid != NULL) {
        strlcpy(ui_ssid, ssid, sizeof(ui_ssid));
    }
    if (status != NULL) {
        strlcpy(ui_status, status, sizeof(ui_status));
    }
    if (ip != NULL) {
        strlcpy(ui_ip, ip, sizeof(ui_ip));
    }
    if (hint != NULL) {
        strlcpy(ui_hint, hint, sizeof(ui_hint));
    }
    ui_dirty = true;
    xSemaphoreGive(ui_mutex);
}

static void update_connection_state(const char *ssid, const char *status, const char *ip)
{
    xSemaphoreTake(wifi_state_mutex, portMAX_DELAY);
    if (ssid != NULL) {
        strlcpy(selected_ssid, ssid, sizeof(selected_ssid));
    }
    if (status != NULL) {
        strlcpy(wifi_status, status, sizeof(wifi_status));
    }
    if (ip != NULL) {
        strlcpy(sta_ip, ip, sizeof(sta_ip));
    }
    xSemaphoreGive(wifi_state_mutex);
}

static bool lvgl_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx)
{
    LV_UNUSED(panel_io);
    LV_UNUSED(event_data);
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

static void apply_ui_if_needed(void)
{
    char ssid[48];
    char status[64];
    char ip[48];
    char hint[80];
    bool dirty;

    xSemaphoreTake(ui_mutex, portMAX_DELAY);
    dirty = ui_dirty;
    if (dirty) {
        strlcpy(ssid, ui_ssid, sizeof(ssid));
        strlcpy(status, ui_status, sizeof(status));
        strlcpy(ip, ui_ip, sizeof(ip));
        strlcpy(hint, ui_hint, sizeof(hint));
        ui_dirty = false;
    }
    xSemaphoreGive(ui_mutex);

    if (!dirty) {
        return;
    }

    lv_label_set_text(ssid_label, ssid);
    lv_label_set_text(status_label, status);
    lv_label_set_text(ip_label, ip);
    lv_label_set_text(hint_label, hint);
}

static void create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "WiFi_test");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -80);

    ssid_label = lv_label_create(scr);
    lv_obj_set_width(ssid_label, 210);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x46C2FF), 0);
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(ssid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ssid_label, LV_ALIGN_CENTER, 0, -36);

    status_label = lv_label_create(scr);
    lv_obj_set_width(status_label, 210);
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -6);

    ip_label = lv_label_create(scr);
    lv_obj_set_width(ip_label, 210);
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0xF7C948), 0);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ip_label, LV_ALIGN_CENTER, 0, 26);

    hint_label = lv_label_create(scr);
    /* Keep the bottom hint narrow: the round LCD has less horizontal room near the lower edge. */
    lv_obj_set_width(hint_label, 150);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xB9C2CF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(hint_label, 2, 0);
    lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 66);

    apply_ui_if_needed();
}

static void init_backlight(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_11_BIT,
        .freq_hz = LCD_BACKLIGHT_PWM_HZ,
        /* Use APB explicitly: 25 kHz with 11-bit duty is not achievable from every auto-selected source. */
        .clk_cfg = LEDC_USE_APB_CLK,
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

static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
        if (src[i] == '%' && hex_to_int(src[i + 1]) >= 0 && hex_to_int(src[i + 2]) >= 0) {
            dst[out++] = (char)((hex_to_int(src[i + 1]) << 4) | hex_to_int(src[i + 2]));
            i += 2;
        } else if (src[i] == '+') {
            dst[out++] = ' ';
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

static void get_form_value(const char *body, const char *key, char *out, size_t out_size)
{
    /* The captive portal posts application/x-www-form-urlencoded fields from plain browser JS. */
    char pattern[20];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *start = strstr(body, pattern);
    if (start == NULL) {
        out[0] = '\0';
        return;
    }
    start += strlen(pattern);
    const char *end = strchr(start, '&');
    size_t len = end == NULL ? strlen(start) : (size_t)(end - start);
    char encoded[128];
    if (len >= sizeof(encoded)) {
        len = sizeof(encoded) - 1;
    }
    memcpy(encoded, start, len);
    encoded[len] = '\0';
    url_decode(out, out_size, encoded);
}

static void json_escape(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
        if ((src[i] == '"' || src[i] == '\\') && out + 2 < dst_size) {
            dst[out++] = '\\';
            dst[out++] = src[i];
        } else if ((unsigned char)src[i] >= 0x20) {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

static const char portal_html[] =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ONX2424G013 WiFi Setup</title>"
"<style>"
"body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#101820;color:#eef3f8}"
".wrap{max-width:560px;margin:0 auto;padding:22px}"
"h1{font-size:24px;margin:10px 0 6px}.sub{color:#9fb0c2;margin:0 0 18px}"
"button,input{font-size:16px;border-radius:8px;border:0;box-sizing:border-box}"
"button{background:#46c2ff;color:#06131f;padding:12px 14px;font-weight:700}"
"button.secondary{background:#243242;color:#e9f1f8}"
".list{margin:14px 0}.ap{display:flex;justify-content:space-between;gap:12px;width:100%;margin:8px 0;text-align:left}"
"input{width:100%;padding:12px;margin:8px 0 12px;background:#172331;color:#fff;border:1px solid #314153}"
".card{background:#172331;border:1px solid #263649;border-radius:10px;padding:14px;margin-top:14px}"
".muted{color:#9fb0c2}.ok{color:#4ee287}.bad{color:#ff8b8b}"
"</style></head><body><div class='wrap'>"
"<h1>WiFi_test</h1><p class='sub'>AP: ONX2424G013 · Open 192.168.4.1 if this page did not pop up.</p>"
"<button class='secondary' onclick='scan()'>Refresh WiFi List</button>"
"<div id='list' class='list muted'>Scanning...</div>"
"<div class='card'><div class='muted'>Selected WiFi</div><h2 id='ssid' style='margin:6px 0 10px'>None</h2>"
"<input id='password' type='password' placeholder='WiFi password'>"
"<button onclick='connectWifi()'>Connect</button></div>"
"<div class='card'><div class='muted'>Device Status</div><p id='status'>Waiting</p></div>"
"</div><script>"
"let selected='';"
"function esc(s){return String(s).replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]));}"
"async function scan(){let box=document.getElementById('list');box.textContent='Scanning...';"
"try{let r=await fetch('/scan',{cache:'no-store'});let aps=await r.json();"
"if(!aps.length){box.textContent='No WiFi found. Tap refresh to scan again.';return;}"
"box.innerHTML=aps.map(ap=>`<button class='ap secondary' onclick='pick(${JSON.stringify(ap.ssid)})'><span>${esc(ap.ssid)}</span><span>${ap.rssi} dBm</span></button>`).join('');}"
"catch(e){box.textContent='Scan failed. Please retry.';}}"
"function pick(s){selected=s;document.getElementById('ssid').textContent=s;document.getElementById('password').focus();}"
"async function connectWifi(){if(!selected){alert('Select a WiFi first');return;}"
"let p=document.getElementById('password').value;"
"await fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(selected)+'&password='+encodeURIComponent(p)});poll();}"
"async function poll(){try{let r=await fetch('/status',{cache:'no-store'});let s=await r.json();"
"document.getElementById('status').innerHTML=`SSID: ${esc(s.ssid||'-')}<br>Status: <span class='${s.status=='Connected'?'ok':(s.status=='Failed'?'bad':'')}'>${esc(s.status)}</span><br>IP: ${esc(s.ip||'-')}`;}catch(e){}}"
"setInterval(poll,2000);scan();poll();"
"</script></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, portal_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    /* Keep scan records off the HTTP server stack; phone captive-portal probes can already be stack-heavy. */
    wifi_ap_record_t *records = calloc(WIFI_SCAN_MAX_AP, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "[]");
    }

    uint16_t ap_count = WIFI_SCAN_MAX_AP;
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    set_ui_text(NULL, "Scanning nearby WiFi", NULL, NULL);
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "503 Service Unavailable");
        free(records);
        return httpd_resp_sendstr(req, "[]");
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, records));
    set_ui_text(NULL, "Portal ready", NULL, "Select WiFi on phone");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    for (int i = 0; i < ap_count; i++) {
        char ssid[33];
        char escaped[80];
        char item[160];
        memcpy(ssid, records[i].ssid, 32);
        ssid[32] = '\0';
        json_escape(escaped, sizeof(escaped), ssid);
        snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                 i == 0 ? "" : ",", escaped, records[i].rssi, records[i].authmode);
        httpd_resp_sendstr_chunk(req, item);
    }
    httpd_resp_sendstr_chunk(req, "]");
    esp_err_t send_err = httpd_resp_sendstr_chunk(req, NULL);
    free(records);
    return send_err;
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char body[256];
    int received = 0;
    while (received < req->content_len && received < (int)sizeof(body) - 1) {
        int room = (int)sizeof(body) - 1 - received;
        int remaining = req->content_len - received;
        int ret = httpd_req_recv(req, body + received, remaining < room ? remaining : room);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += ret;
    }
    body[received] = '\0';

    char ssid[33];
    char password[65];
    get_form_value(body, "ssid", ssid, sizeof(ssid));
    get_form_value(body, "password", password, sizeof(password));

    if (ssid[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing ssid");
    }

    wifi_config_t sta_config = { 0 };
    strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_LOGI(TAG, "connecting to SSID: %s", ssid);
    esp_err_t disconnect_err = esp_wifi_disconnect();
    if (disconnect_err != ESP_OK && disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_ERROR_CHECK(disconnect_err);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    update_connection_state(ssid, "Connecting", "");

    char ssid_line[48];
    snprintf(ssid_line, sizeof(ssid_line), "WiFi: %s", ssid);
    set_ui_text(ssid_line, "Status: Connecting", "IP: -", "Keep phone on AP");
    ESP_ERROR_CHECK(esp_wifi_connect());

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char ssid[33];
    char status[32];
    char ip[16];
    char escaped_ssid[80];
    char response[160];

    xSemaphoreTake(wifi_state_mutex, portMAX_DELAY);
    strlcpy(ssid, selected_ssid, sizeof(ssid));
    strlcpy(status, wifi_status, sizeof(status));
    strlcpy(ip, sta_ip, sizeof(ip));
    xSemaphoreGive(wifi_state_mutex);

    json_escape(escaped_ssid, sizeof(escaped_ssid), ssid);
    snprintf(response, sizeof(response), "{\"ssid\":\"%s\",\"status\":\"%s\",\"ip\":\"%s\"}", escaped_ssid, status, ip);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, response);
}

static void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    /* Captive-portal probes can be bursty; keep enough stack for phone OS checks plus the UI page. */
    config.stack_size = 12288;
    /* Keep this within the default LWIP_MAX_SOCKETS limit. LRU purge handles captive-portal bursts. */
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_get_handler,
    };
    httpd_register_uri_handler(http_server, &scan_uri);

    httpd_uri_t connect_uri = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_post_handler,
    };
    httpd_register_uri_handler(http_server, &connect_uri);

    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
    };
    httpd_register_uri_handler(http_server, &status_uri);

    httpd_uri_t root_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(http_server, &root_uri);
}

static void configure_ap_dhcp_options(esp_netif_t *ap_netif)
{
    esp_netif_ip_info_t ap_ip;
    esp_netif_dns_info_t dns = { 0 };
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ap_ip));
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ap_ip.ip.addr;

    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_ERROR_CHECK(err);
    }

    /* Tell phones to use the device itself as DNS so all captive-portal probes land here. */
    uint8_t offer_dns = 1;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &offer_dns, sizeof(offer_dns)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns));

    /* DHCP option 114 is supported by newer clients as an explicit captive portal URL hint. */
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                           (void *)captive_portal_uri, strlen(captive_portal_uri)));

    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_ERROR_CHECK(err);
    }
}

static void dns_server_task(void *arg)
{
    LV_UNUSED(arg);
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket create failed");
        vTaskDelete(NULL);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
    }

    uint8_t query[256];
    uint8_t response[300];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (true) {
        client_len = sizeof(client_addr);
        int len = recvfrom(sock, query, sizeof(query), 0, (struct sockaddr *)&client_addr, &client_len);
        if (len < 12) {
            continue;
        }

        int question_end = 12;
        while (question_end < len && query[question_end] != 0) {
            question_end += query[question_end] + 1;
        }
        question_end += 5;
        if (question_end > len || question_end + 16 > (int)sizeof(response)) {
            continue;
        }

        memcpy(response, query, question_end);
        /* Answer every A-record query with the portal IP so captive-portal checks resolve locally. */
        response[2] = 0x81;
        response[3] = 0x80;
        response[6] = 0x00;
        response[7] = 0x01;
        response[8] = response[9] = response[10] = response[11] = 0x00;

        int pos = question_end;
        response[pos++] = 0xC0;
        response[pos++] = 0x0C;
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        response[pos++] = 0x00;
        response[pos++] = 0x01;
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x00;
        response[pos++] = 0x3C;
        response[pos++] = 0x00;
        response[pos++] = 0x04;
        response[pos++] = 192;
        response[pos++] = 168;
        response[pos++] = 4;
        response[pos++] = 1;

        sendto(sock, response, pos, 0, (struct sockaddr *)&client_addr, client_len);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    LV_UNUSED(arg);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        set_ui_text(NULL, "Phone connected to AP", NULL, "Portal should open");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "STA disconnected, reason=%d", event->reason);
        update_connection_state(NULL, "Failed", "");
        set_ui_text(NULL, "Status: Failed", "IP: -", "Check password and retry");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip[16];
        char ip_line[48];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(ip_line, sizeof(ip_line), "IP: %s", ip);
        update_connection_state(NULL, "Connected", ip);
        set_ui_text(NULL, "Status: Connected", ip_line, "Provision done");
    }
}

static void init_wifi_portal(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    /* DHCP options must be applied before WiFi starts so clients receive them on first association. */
    configure_ap_dhcp_options(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = WIFI_AP_MAX_CONN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    update_connection_state("", "Waiting", "");
    start_http_server();
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Start ONX2424G013 WiFi provisioning test");
    ui_mutex = xSemaphoreCreateMutex();
    wifi_state_mutex = xSemaphoreCreateMutex();

    init_backlight();
    init_lcd_panel();
    init_lvgl();
    create_ui();
    lv_timer_handler();
    set_backlight(LCD_BRIGHTNESS_PERCENT);

    init_wifi_portal();

    while (true) {
        apply_ui_if_needed();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
