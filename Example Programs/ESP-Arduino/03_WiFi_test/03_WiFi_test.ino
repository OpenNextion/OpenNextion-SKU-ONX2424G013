#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// build_opt.h applies the same LVGL options to the library C files.
#define LV_CONF_SKIP
#include <lvgl.h>

#include "lcd_config.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCLK, LCD_PIN_MOSI, GFX_NOT_DEFINED, FSPI);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, LCD_PIN_RST, 0, true, LCD_H_RES, LCD_V_RES);

static DNSServer dns_server;
static WebServer web_server(80);
static const IPAddress portal_ip(192, 168, 4, 1);
static const IPAddress portal_gateway(192, 168, 4, 1);
static const IPAddress portal_subnet(255, 255, 255, 0);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf1[LCD_H_RES * 40];
static lv_color_t lv_buf2[LCD_H_RES * 40];
static lv_obj_t *ssid_label;
static lv_obj_t *status_label;
static lv_obj_t *ip_label;
static lv_obj_t *hint_label;

static String ui_ssid = "AP: " WIFI_AP_SSID;
static String ui_status = "Connect phone to AP";
static String ui_ip = "Open: " WIFI_PORTAL_IP;
static String ui_hint = "No password\nPopup/browser\n192.168.4.1";
static bool ui_dirty = true;

static String selected_ssid;
static String wifi_status = "Waiting";
static String sta_ip;

// Keep the portal script conservative for captive-portal WebViews: plain XMLHttpRequest and ES5 syntax.
static const char portal_html[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>ONX2424G013 WiFi Setup</title>
<style>
body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#101820;color:#eef3f8}
.wrap{max-width:560px;margin:0 auto;padding:22px}
h1{font-size:24px;margin:10px 0 6px}.sub{color:#9fb0c2;margin:0 0 18px}
button,input{font-size:16px;border-radius:8px;border:0;box-sizing:border-box}
button{background:#46c2ff;color:#06131f;padding:12px 14px;font-weight:700}
button.secondary{background:#243242;color:#e9f1f8}
.list{margin:14px 0}.ap{display:flex;justify-content:space-between;gap:12px;width:100%;margin:8px 0;text-align:left}
input{width:100%;padding:12px;margin:8px 0 12px;background:#172331;color:#fff;border:1px solid #314153}
.card{background:#172331;border:1px solid #263649;border-radius:10px;padding:14px;margin-top:14px}
.muted{color:#9fb0c2}.ok{color:#4ee287}.bad{color:#ff8b8b}
</style></head><body><div class='wrap'>
<h1>WiFi_test</h1><p class='sub'>AP: ONX2424G013 - Open 192.168.4.1 if this page did not pop up.</p>
<button class='secondary' onclick='scan()'>Refresh WiFi List</button>
<div id='list' class='list muted'>Scanning...</div>
<div class='card'><div class='muted'>Selected WiFi</div><h2 id='ssid' style='margin:6px 0 10px'>None</h2>
<input id='password' type='password' placeholder='WiFi password'>
<button onclick='connectWifi()'>Connect</button></div>
<div class='card'><div class='muted'>Device Status</div><p id='status'>Waiting</p></div>
</div><script>new Image().src='/js_start?ts='+(new Date().getTime());</script><script>
var selected='';
var apsCache=[];
window.onerror=function(msg){new Image().src='/js_error?m='+encodeURIComponent(msg);};
function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');}
function xhr(method,url,body,done,fail){var r=new XMLHttpRequest();r.onreadystatechange=function(){if(r.readyState==4){if(r.status>=200&&r.status<300){done(r.responseText);}else if(fail){fail();}}};r.open(method,url,true);if(method=='POST'){r.setRequestHeader('Content-Type','application/x-www-form-urlencoded');}r.send(body||null);}
function scan(){var box=document.getElementById('list');box.textContent='Scanning...';xhr('GET','/scan',null,function(text){apsCache=JSON.parse(text);if(!apsCache.length){box.textContent='No WiFi found. Tap refresh to scan again.';return;}var html='';for(var i=0;i<apsCache.length;i++){html+='<button class="ap secondary" onclick="pick('+i+')"><span>'+esc(apsCache[i].ssid)+'</span><span>'+apsCache[i].rssi+' dBm</span></button>';}box.innerHTML=html;},function(){box.textContent='Scan failed. Please retry.';});}
function pick(index){selected=apsCache[index].ssid;document.getElementById('ssid').textContent=selected;document.getElementById('password').focus();}
function connectWifi(){if(!selected){alert('Select a WiFi first');return;}var p=document.getElementById('password').value;xhr('POST','/connect','ssid='+encodeURIComponent(selected)+'&password='+encodeURIComponent(p),function(){poll();});}
function poll(){xhr('GET','/status',null,function(text){var s=JSON.parse(text);var cls=s.status=='Connected'?'ok':(s.status=='Failed'?'bad':'');document.getElementById('status').innerHTML='SSID: '+esc(s.ssid||'-')+'<br>Status: <span class="'+cls+'">'+esc(s.status)+'</span><br>IP: '+esc(s.ip||'-');});}
setInterval(poll,2000);scan();poll();
</script></body></html>
)rawliteral";

static void log_line(const char *message)
{
    Serial.println(message);
}

template <typename... Args>
static void log_printf(const char *format, Args... args)
{
    Serial.printf(format, args...);
    Serial.println();
}

static void set_ui_text(const String &ssid, const String &status, const String &ip, const String &hint)
{
    if (ssid.length() > 0) {
        ui_ssid = ssid;
    }
    if (status.length() > 0) {
        ui_status = status;
    }
    if (ip.length() > 0) {
        ui_ip = ip;
    }
    if (hint.length() > 0) {
        ui_hint = hint;
    }
    ui_dirty = true;
}

static String json_escape(const String &value)
{
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c == '"' || c == '\\') {
            escaped += '\\';
            escaped += c;
        } else if ((uint8_t)c >= 0x20) {
            escaped += c;
        }
    }
    return escaped;
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
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = (2047 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void init_backlight()
{
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
    // Arduino_GFX owns the GC9A01 SPI transfer; LVGL only supplies the dirty rectangle.
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
    lv_disp_flush_ready(disp);
}

static void apply_ui_if_needed()
{
    if (!ui_dirty) {
        return;
    }
    ui_dirty = false;
    lv_label_set_text(ssid_label, ui_ssid.c_str());
    lv_label_set_text(status_label, ui_status.c_str());
    lv_label_set_text(ip_label, ui_ip.c_str());
    lv_label_set_text(hint_label, ui_hint.c_str());
}

static void create_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *title_label = lv_label_create(scr);
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
    // Keep the bottom hint narrow: the round LCD has less horizontal room near the lower edge.
    lv_obj_set_width(hint_label, 150);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xB9C2CF), 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(hint_label, 2, 0);
    lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 66);

    apply_ui_if_needed();
}

static void update_connection_state(const String &ssid, const String &status, const String &ip)
{
    if (ssid.length() > 0) {
        selected_ssid = ssid;
    }
    if (status.length() > 0) {
        wifi_status = status;
    }
    sta_ip = ip;
}

static void handle_root()
{
    log_printf("[WiFi_test] GET %s", web_server.uri().c_str());
    web_server.sendHeader("Cache-Control", "no-store");
    web_server.send_P(200, "text/html; charset=utf-8", portal_html);
}

static void handle_scan()
{
    // Match the ESP-IDF example: each /scan request performs one blocking WiFi scan and returns JSON.
    log_line("[WiFi_test] GET /scan");
    set_ui_text("", "Scanning nearby WiFi", "", "");

    wifi_ap_record_t records[WIFI_SCAN_MAX_AP] = {};
    uint16_t count = WIFI_SCAN_MAX_AP;
    wifi_scan_config_t scan_config = {
        .ssid = nullptr,
        .bssid = nullptr,
        .channel = 0,
        .show_hidden = false,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err == ESP_OK) {
        err = esp_wifi_scan_get_ap_records(&count, records);
    }

    if (err != ESP_OK) {
        log_printf("[WiFi_test] scan failed: %s", esp_err_to_name(err));
        web_server.sendHeader("Cache-Control", "no-store");
        web_server.send(503, "application/json", "[]");
        return;
    }

    set_ui_text("", "Portal ready", "", "Select WiFi on phone");
    log_printf("[WiFi_test] GET /scan -> %u APs", count);
    web_server.sendHeader("Cache-Control", "no-store");
    web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    web_server.send(200, "application/json", "");
    web_server.sendContent("[");
    for (int i = 0; i < count; i++) {
        char ssid[33];
        memcpy(ssid, records[i].ssid, 32);
        ssid[32] = '\0';
        String item = (i == 0) ? "" : ",";
        item += "{\"ssid\":\"";
        item += json_escape(ssid);
        item += "\",\"rssi\":";
        item += records[i].rssi;
        item += ",\"auth\":";
        item += (int)records[i].authmode;
        item += "}";
        web_server.sendContent(item);
    }
    web_server.sendContent("]");
    web_server.sendContent("");
}

static void handle_js_start()
{
    // Diagnostic endpoint: confirms the phone captive-portal WebView executed the page script.
    log_line("[WiFi_test] GET /js_start");
    web_server.sendHeader("Cache-Control", "no-store");
    web_server.send(204, "text/plain", "");
}

static void handle_js_error()
{
    // Diagnostic endpoint: reports page script errors to the USB Serial/JTAG log.
    log_printf("[WiFi_test] GET /js_error m=%s", web_server.arg("m").c_str());
    web_server.sendHeader("Cache-Control", "no-store");
    web_server.send(204, "text/plain", "");
}

static void handle_connect()
{
    if (!web_server.hasArg("ssid")) {
        web_server.send(400, "text/plain", "missing ssid");
        return;
    }

    String ssid = web_server.arg("ssid");
    String password = web_server.arg("password");
    log_printf("[WiFi_test] POST /connect ssid=%s", ssid.c_str());
    selected_ssid = ssid;
    update_connection_state(ssid, "Connecting", "");

    String ssid_line = "WiFi: " + ssid;
    set_ui_text(ssid_line, "Status: Connecting", "IP: -", "Keep phone on AP");

    esp_wifi_scan_stop();
    WiFi.disconnect(false, false);
    WiFi.begin(ssid.c_str(), password.c_str());
    web_server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_status()
{
    String response = "{\"ssid\":\"";
    response += json_escape(selected_ssid);
    response += "\",\"status\":\"";
    response += json_escape(wifi_status);
    response += "\",\"ip\":\"";
    response += json_escape(sta_ip);
    response += "\"}";
    web_server.sendHeader("Cache-Control", "no-store");
    web_server.send(200, "application/json", response);
}

static void wifi_event(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        log_line("[WiFi_test] phone connected to AP");
        set_ui_text("", "Phone connected to AP", "", "Portal should open");
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        // Ignore the initial STA idle/disconnected event before the user submits credentials.
        if (selected_ssid.length() > 0) {
            log_printf("[WiFi_test] STA disconnected, reason=%u", (unsigned)info.wifi_sta_disconnected.reason);
            update_connection_state("", "Failed", "");
            set_ui_text("", "Status: Failed", "IP: -", "Check password and retry");
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        sta_ip = WiFi.localIP().toString();
        log_printf("[WiFi_test] STA got IP: %s", sta_ip.c_str());
        update_connection_state("", "Connected", sta_ip);
        set_ui_text("", "Status: Connected", "IP: " + sta_ip, "Provision done");
        break;
    default:
        break;
    }
}

static void start_wifi_portal()
{
    log_line("[WiFi_test] start WiFi portal");
    WiFi.persistent(false);
    WiFi.onEvent(wifi_event);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(portal_ip, portal_gateway, portal_subnet);
    bool ap_ok = WiFi.softAP(WIFI_AP_SSID, nullptr, WIFI_AP_CHANNEL, false, WIFI_AP_MAX_CONN);
    log_printf("[WiFi_test] softAP %s, ip=%s", ap_ok ? "ok" : "failed", WiFi.softAPIP().toString().c_str());

    // Wildcard DNS plus the catch-all HTTP route makes phone captive-portal probes land on this page.
    dns_server.start(53, "*", portal_ip);
    web_server.on("/", HTTP_GET, handle_root);
    web_server.on("/scan", HTTP_GET, handle_scan);
    web_server.on("/js_start", HTTP_GET, handle_js_start);
    web_server.on("/js_error", HTTP_GET, handle_js_error);
    web_server.on("/connect", HTTP_POST, handle_connect);
    web_server.on("/status", HTTP_GET, handle_status);
    web_server.onNotFound(handle_root);
    web_server.begin();
    log_line("[WiFi_test] HTTP and DNS servers started");
}

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(200);
    log_line("[WiFi_test] boot");

    init_backlight();
    set_backlight(0);

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
    set_backlight(LCD_BRIGHTNESS_PERCENT);

    start_wifi_portal();
}

void loop()
{
    lv_tick_inc(5);
    dns_server.processNextRequest();
    web_server.handleClient();
    apply_ui_if_needed();
    lv_timer_handler();
    delay(5);
}
