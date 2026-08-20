#pragma once

#include <cstring>
#include <string>
#include <unistd.h>

#include "esphome/core/component.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"

namespace esphome {
namespace wifi_portal {

namespace {
constexpr const char *TAG = "wifi_portal";
constexpr const char *WIFI_AP_SSID = "ONX2424G013";
constexpr const char *WIFI_PORTAL_IP = "192.168.4.1";
constexpr const char *CAPTIVE_PORTAL_URI = "http://192.168.4.1";
constexpr int WIFI_AP_CHANNEL = 1;
constexpr int WIFI_AP_MAX_CONN = 4;
constexpr int WIFI_SCAN_MAX_AP = 20;
}  // namespace

class WifiPortalComponent : public Component {
 public:
  void set_ui_ssid(globals::GlobalsComponent<std::string> *value) { this->ui_ssid_out_ = value; }
  void set_ui_status(globals::GlobalsComponent<std::string> *value) { this->ui_status_out_ = value; }
  void set_ui_ip(globals::GlobalsComponent<std::string> *value) { this->ui_ip_out_ = value; }
  void set_ui_hint(globals::GlobalsComponent<std::string> *value) { this->ui_hint_out_ = value; }
  void set_selected_ssid(globals::GlobalsComponent<std::string> *value) { this->selected_ssid_out_ = value; }
  void set_connection_status(globals::GlobalsComponent<std::string> *value) { this->connection_status_out_ = value; }
  void set_sta_ip(globals::GlobalsComponent<std::string> *value) { this->sta_ip_out_ = value; }

  void setup() override {
    // The ESPHome wifi component owns the WiFi driver and connection state so
    // Native API/Home Assistant can see a real connected network. This portal
    // component only adds the matching HTTP/DNS provisioning surface.
    this->state_mutex_ = xSemaphoreCreateMutex();
    this->set_ui_text_("AP: ONX2424G013", "Connect phone to AP", "Open: 192.168.4.1",
                       "No password\nPopup/browser\n192.168.4.1");
    this->set_connection_state_("", "Waiting", "");
    this->init_wifi_portal_();
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void loop() override { this->publish_pending_state_(); }

 protected:
  globals::GlobalsComponent<std::string> *ui_ssid_out_{nullptr};
  globals::GlobalsComponent<std::string> *ui_status_out_{nullptr};
  globals::GlobalsComponent<std::string> *ui_ip_out_{nullptr};
  globals::GlobalsComponent<std::string> *ui_hint_out_{nullptr};
  globals::GlobalsComponent<std::string> *selected_ssid_out_{nullptr};
  globals::GlobalsComponent<std::string> *connection_status_out_{nullptr};
  globals::GlobalsComponent<std::string> *sta_ip_out_{nullptr};

  SemaphoreHandle_t state_mutex_{nullptr};
  httpd_handle_t http_server_{nullptr};
  char ui_ssid_[48] = "AP: ONX2424G013";
  char ui_status_[64] = "Connect phone to AP";
  char ui_ip_[48] = "Open: 192.168.4.1";
  char ui_hint_[80] = "No password\nPopup/browser\n192.168.4.1";
  char selected_ssid_[33] = "";
  char wifi_status_[32] = "Waiting";
  char sta_ip_[16] = "";
  bool dirty_{true};

  static const char *portal_html_() {
    return R"rawliteral(<!doctype html><html><head><meta charset='utf-8'>
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
<h1>WiFi_test</h1><p class='sub'>AP: ONX2424G013 · Open 192.168.4.1 if this page did not pop up.</p>
<button class='secondary' onclick='scan()'>Refresh WiFi List</button>
<div id='list' class='list muted'>Scanning...</div>
<div class='card'><div class='muted'>Selected WiFi</div><h2 id='ssid' style='margin:6px 0 10px'>None</h2>
<input id='password' type='password' placeholder='WiFi password'>
<button onclick='connectWifi()'>Connect</button></div>
<div class='card'><div class='muted'>Device Status</div><p id='status'>Waiting</p></div>
</div><script>
let selected='';
function esc(s){return String(s).replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));}
async function scan(){let box=document.getElementById('list');box.textContent='Scanning...';
try{let r=await fetch('/scan',{cache:'no-store'});let aps=await r.json();
if(!aps.length){box.textContent='No WiFi found. Tap refresh to scan again.';return;}
box.innerHTML=aps.map(ap=>`<button class='ap secondary' onclick='pick(${JSON.stringify(ap.ssid)})'><span>${esc(ap.ssid)}</span><span>${ap.rssi} dBm</span></button>`).join('');}
catch(e){box.textContent='Scan failed. Please retry.';}}
function pick(s){selected=s;document.getElementById('ssid').textContent=s;document.getElementById('password').focus();}
async function connectWifi(){if(!selected){alert('Select a WiFi first');return;}
let p=document.getElementById('password').value;
await fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(selected)+'&password='+encodeURIComponent(p)});poll();}
async function poll(){try{let r=await fetch('/status',{cache:'no-store'});let s=await r.json();
document.getElementById('status').innerHTML=`SSID: ${esc(s.ssid||'-')}<br>Status: <span class='${s.status=='Connected'?'ok':(s.status=='Failed'?'bad':'')}'>${esc(s.status)}</span><br>IP: ${esc(s.ip||'-')}`;}catch(e){}}
setInterval(poll,2000);scan();poll();
</script></body></html>)rawliteral";
  }

  static void copy_if_present_(char *dst, size_t dst_size, const char *src) {
    if (src != nullptr) {
      strlcpy(dst, src, dst_size);
    }
  }

  void set_ui_text_(const char *ssid, const char *status, const char *ip, const char *hint) {
    if (this->state_mutex_ == nullptr) {
      return;
    }
    xSemaphoreTake(this->state_mutex_, portMAX_DELAY);
    copy_if_present_(this->ui_ssid_, sizeof(this->ui_ssid_), ssid);
    copy_if_present_(this->ui_status_, sizeof(this->ui_status_), status);
    copy_if_present_(this->ui_ip_, sizeof(this->ui_ip_), ip);
    copy_if_present_(this->ui_hint_, sizeof(this->ui_hint_), hint);
    this->dirty_ = true;
    xSemaphoreGive(this->state_mutex_);
  }

  void set_connection_state_(const char *ssid, const char *status, const char *ip) {
    if (this->state_mutex_ == nullptr) {
      return;
    }
    xSemaphoreTake(this->state_mutex_, portMAX_DELAY);
    copy_if_present_(this->selected_ssid_, sizeof(this->selected_ssid_), ssid);
    copy_if_present_(this->wifi_status_, sizeof(this->wifi_status_), status);
    copy_if_present_(this->sta_ip_, sizeof(this->sta_ip_), ip);
    this->dirty_ = true;
    xSemaphoreGive(this->state_mutex_);
  }

  void publish_pending_state_() {
    char ui_ssid[48];
    char ui_status[64];
    char ui_ip[48];
    char ui_hint[80];
    char selected_ssid[33];
    char wifi_status[32];
    char sta_ip[16];
    bool dirty = false;

    if (this->state_mutex_ == nullptr) {
      return;
    }
    xSemaphoreTake(this->state_mutex_, portMAX_DELAY);
    dirty = this->dirty_;
    if (dirty) {
      strlcpy(ui_ssid, this->ui_ssid_, sizeof(ui_ssid));
      strlcpy(ui_status, this->ui_status_, sizeof(ui_status));
      strlcpy(ui_ip, this->ui_ip_, sizeof(ui_ip));
      strlcpy(ui_hint, this->ui_hint_, sizeof(ui_hint));
      strlcpy(selected_ssid, this->selected_ssid_, sizeof(selected_ssid));
      strlcpy(wifi_status, this->wifi_status_, sizeof(wifi_status));
      strlcpy(sta_ip, this->sta_ip_, sizeof(sta_ip));
      this->dirty_ = false;
    }
    xSemaphoreGive(this->state_mutex_);

    if (!dirty) {
      return;
    }

    // Only the ESPHome main loop writes the shared globals. HTTP and WiFi
    // callbacks update private buffers protected by state_mutex_.
    if (this->ui_ssid_out_ != nullptr) {
      this->ui_ssid_out_->value() = ui_ssid;
    }
    if (this->ui_status_out_ != nullptr) {
      this->ui_status_out_->value() = ui_status;
    }
    if (this->ui_ip_out_ != nullptr) {
      this->ui_ip_out_->value() = ui_ip;
    }
    if (this->ui_hint_out_ != nullptr) {
      this->ui_hint_out_->value() = ui_hint;
    }
    if (this->selected_ssid_out_ != nullptr) {
      this->selected_ssid_out_->value() = selected_ssid;
    }
    if (this->connection_status_out_ != nullptr) {
      this->connection_status_out_->value() = wifi_status;
    }
    if (this->sta_ip_out_ != nullptr) {
      this->sta_ip_out_->value() = sta_ip;
    }
  }

  static int hex_to_int_(char c) {
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

  static void url_decode_(char *dst, size_t dst_size, const char *src) {
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
      if (src[i] == '%' && hex_to_int_(src[i + 1]) >= 0 && hex_to_int_(src[i + 2]) >= 0) {
        dst[out++] = static_cast<char>((hex_to_int_(src[i + 1]) << 4) | hex_to_int_(src[i + 2]));
        i += 2;
      } else if (src[i] == '+') {
        dst[out++] = ' ';
      } else {
        dst[out++] = src[i];
      }
    }
    dst[out] = '\0';
  }

  static void get_form_value_(const char *body, const char *key, char *out, size_t out_size) {
    // The portal form posts application/x-www-form-urlencoded fields, same as the ESP-IDF example.
    char pattern[20];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *start = strstr(body, pattern);
    if (start == nullptr) {
      out[0] = '\0';
      return;
    }
    start += strlen(pattern);
    const char *end = strchr(start, '&');
    size_t len = end == nullptr ? strlen(start) : static_cast<size_t>(end - start);
    char encoded[128];
    if (len >= sizeof(encoded)) {
      len = sizeof(encoded) - 1;
    }
    memcpy(encoded, start, len);
    encoded[len] = '\0';
    url_decode_(out, out_size, encoded);
  }

  static void json_escape_(char *dst, size_t dst_size, const char *src) {
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_size; i++) {
      if ((src[i] == '"' || src[i] == '\\') && out + 2 < dst_size) {
        dst[out++] = '\\';
        dst[out++] = src[i];
      } else if (static_cast<unsigned char>(src[i]) >= 0x20) {
        dst[out++] = src[i];
      }
    }
    dst[out] = '\0';
  }

  static esp_err_t root_get_handler_(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, portal_html_(), HTTPD_RESP_USE_STRLEN);
  }

  static esp_err_t ensure_apsta_mode_for_scan_() {
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_get_mode failed before scan: %s", esp_err_to_name(err));
      return err;
    }

    ESP_LOGI(TAG, "scan requested, wifi mode before scan=%d", static_cast<int>(mode));
    if (mode != WIFI_MODE_APSTA) {
      // Keep the phone connected to the provisioning AP while enabling the STA
      // side for active scans. This mirrors the verified ESP-IDF example.
      err = esp_wifi_set_mode(WIFI_MODE_APSTA);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to switch to APSTA before scan: %s", esp_err_to_name(err));
      }
      return err;
    }

    return ESP_OK;
  }

  static esp_err_t scan_get_handler_(httpd_req_t *req) {
    auto *self = static_cast<WifiPortalComponent *>(req->user_ctx);
    if (wifi::global_wifi_component == nullptr) {
      ESP_LOGE(TAG, "ESPHome wifi component is not available for scan");
      httpd_resp_set_status(req, "500 Internal Server Error");
      return httpd_resp_sendstr(req, "[]");
    }

    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = true;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 100;
    scan_config.scan_time.active.max = 300;

    self->set_ui_text_(nullptr, "Scanning nearby WiFi", nullptr, nullptr);
    esp_wifi_scan_stop();
    esp_err_t err = ensure_apsta_mode_for_scan_();
    if (err == ESP_OK) {
      err = esp_wifi_scan_start(&scan_config, true);
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
      httpd_resp_set_status(req, "503 Service Unavailable");
      return httpd_resp_sendstr(req, "[]");
    }

    // ESPHome's WiFi component also handles WIFI_EVENT_SCAN_DONE and calls
    // esp_wifi_scan_get_ap_records(), which consumes the IDF result buffer.
    // Requesting keep_scan_results() in __init__.py makes ESPHome cache the
    // full list; read that cache instead of racing the framework for records.
    const auto *results = &wifi::global_wifi_component->get_scan_result();
    for (int i = 0; i < 20 && results->empty(); i++) {
      vTaskDelay(pdMS_TO_TICKS(25));
      results = &wifi::global_wifi_component->get_scan_result();
    }

    ESP_LOGI(TAG, "scan done, ESPHome cache has %u APs", static_cast<unsigned>(results->size()));
    self->set_ui_text_(nullptr, "Portal ready", nullptr, "Select WiFi on phone");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    int emitted = 0;
    for (const auto &result : *results) {
      if (emitted >= WIFI_SCAN_MAX_AP || result.get_is_hidden()) {
        continue;
      }
      char escaped[80];
      char item[160];
      auto ssid = result.get_ssid();
      char ssid_buf[33];
      size_t ssid_len = ssid.size() < sizeof(ssid_buf) - 1 ? ssid.size() : sizeof(ssid_buf) - 1;
      memcpy(ssid_buf, ssid.c_str(), ssid_len);
      ssid_buf[ssid_len] = '\0';
      json_escape_(escaped, sizeof(escaped), ssid_buf);
      snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}", emitted == 0 ? "" : ",", escaped,
               result.get_rssi(), result.get_with_auth() ? 1 : 0);
      httpd_resp_sendstr_chunk(req, item);
      emitted++;
    }
    httpd_resp_sendstr_chunk(req, "]");
    esp_err_t send_err = httpd_resp_sendstr_chunk(req, nullptr);
    return send_err;
  }

  static esp_err_t connect_post_handler_(httpd_req_t *req) {
    auto *self = static_cast<WifiPortalComponent *>(req->user_ctx);
    char body[256];
    int received = 0;
    while (received < req->content_len && received < static_cast<int>(sizeof(body)) - 1) {
      int room = static_cast<int>(sizeof(body)) - 1 - received;
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
    get_form_value_(body, "ssid", ssid, sizeof(ssid));
    get_form_value_(body, "password", password, sizeof(password));
    if (ssid[0] == '\0') {
      httpd_resp_set_status(req, "400 Bad Request");
      return httpd_resp_sendstr(req, "missing ssid");
    }

    ESP_LOGI(TAG, "connecting to SSID: %s", ssid);
    if (wifi::global_wifi_component == nullptr) {
      ESP_LOGE(TAG, "ESPHome wifi component is not available");
      httpd_resp_set_status(req, "500 Internal Server Error");
      return httpd_resp_sendstr(req, "{\"ok\":false}");
    }

    wifi::WiFiAP sta_ap;
    sta_ap.set_ssid(ssid);
    sta_ap.set_password(password);

    // Keep the entered credentials inside ESPHome's WiFi component. Its state
    // machine then reports connected=true to network::is_connected(), which is
    // required for the Native API connection used by Home Assistant. Saving also
    // allows the example to reconnect after a reboot.
    wifi::global_wifi_component->save_wifi_sta(ssid, password);
    wifi::global_wifi_component->start_connecting(sta_ap);

    self->set_connection_state_(ssid, "Connecting", "");

    char ssid_line[48];
    snprintf(ssid_line, sizeof(ssid_line), "WiFi: %s", ssid);
    self->set_ui_text_(ssid_line, "Status: Connecting", "IP: -", "Keep phone on AP");
    ESP_ERROR_CHECK(esp_wifi_connect());

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
  }

  static esp_err_t status_get_handler_(httpd_req_t *req) {
    auto *self = static_cast<WifiPortalComponent *>(req->user_ctx);
    char ssid[33];
    char status[32];
    char ip[16];
    char escaped_ssid[80];
    char response[160];

    xSemaphoreTake(self->state_mutex_, portMAX_DELAY);
    strlcpy(ssid, self->selected_ssid_, sizeof(ssid));
    strlcpy(status, self->wifi_status_, sizeof(status));
    strlcpy(ip, self->sta_ip_, sizeof(ip));
    xSemaphoreGive(self->state_mutex_);

    json_escape_(escaped_ssid, sizeof(escaped_ssid), ssid);
    snprintf(response, sizeof(response), "{\"ssid\":\"%s\",\"status\":\"%s\",\"ip\":\"%s\"}", escaped_ssid, status, ip);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, response);
  }

  void start_http_server_() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Keep these limits aligned with ESP-IDF 03. Phone captive-portal probes can
    // open several short-lived sockets before the real page request arrives.
    config.stack_size = 12288;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    ESP_ERROR_CHECK(httpd_start(&this->http_server_, &config));

    httpd_uri_t scan_uri = {.uri = "/scan", .method = HTTP_GET, .handler = scan_get_handler_, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(this->http_server_, &scan_uri));
    httpd_uri_t connect_uri = {.uri = "/connect", .method = HTTP_POST, .handler = connect_post_handler_, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(this->http_server_, &connect_uri));
    httpd_uri_t status_uri = {.uri = "/status", .method = HTTP_GET, .handler = status_get_handler_, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(this->http_server_, &status_uri));
    httpd_uri_t root_uri = {.uri = "/*", .method = HTTP_GET, .handler = root_get_handler_, .user_ctx = this};
    ESP_ERROR_CHECK(httpd_register_uri_handler(this->http_server_, &root_uri));
  }

  void configure_ap_dhcp_options_(esp_netif_t *ap_netif) {
    esp_netif_ip_info_t ap_ip;
    esp_netif_dns_info_t dns = {};
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ap_ip));
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ap_ip.ip.addr;

    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
      ESP_ERROR_CHECK(err);
    }

    // DHCP DNS option points clients back to the device so captive-portal probes land here.
    uint8_t offer_dns = 1;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns,
                                           sizeof(offer_dns)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns));

#ifdef ESP_NETIF_CAPTIVEPORTAL_URI
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                           (void *) CAPTIVE_PORTAL_URI, strlen(CAPTIVE_PORTAL_URI)));
#endif

    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
      ESP_ERROR_CHECK(err);
    }
  }

  static void dns_server_task_(void *arg) {
    (void) arg;
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
      ESP_LOGE(TAG, "DNS socket create failed");
      vTaskDelete(nullptr);
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(sock, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) != 0) {
      ESP_LOGE(TAG, "DNS bind failed");
      ::close(sock);
      vTaskDelete(nullptr);
    }

    uint8_t query[256];
    uint8_t response[300];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (true) {
      client_len = sizeof(client_addr);
      int len = ::recvfrom(sock, query, sizeof(query), 0, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
      if (len < 12) {
        continue;
      }

      int question_end = 12;
      while (question_end < len && query[question_end] != 0) {
        question_end += query[question_end] + 1;
      }
      question_end += 5;
      if (question_end > len || question_end + 16 > static_cast<int>(sizeof(response))) {
        continue;
      }

      memcpy(response, query, question_end);
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

      ::sendto(sock, response, pos, 0, reinterpret_cast<struct sockaddr *>(&client_addr), client_len);
    }
  }

  static void wifi_event_handler_(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    auto *self = static_cast<WifiPortalComponent *>(arg);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
      self->set_ui_text_(nullptr, "Phone connected to AP", nullptr, "Portal should open");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
      ESP_LOGW(TAG, "STA disconnected, reason=%d", event->reason);
      if (self->has_selected_ssid_()) {
        self->set_connection_state_(nullptr, "Failed", "");
        self->set_ui_text_(nullptr, "Status: Failed", "IP: -", "Check password and retry");
      }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      auto *event = static_cast<ip_event_got_ip_t *>(event_data);
      char ip[16];
      char ip_line[48];
      snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
      snprintf(ip_line, sizeof(ip_line), "IP: %s", ip);
      self->set_connection_state_(nullptr, "Connected", ip);
      self->set_ui_text_(nullptr, "Status: Connected", ip_line, "HA ready on LAN");
    }
  }

  bool has_selected_ssid_() {
    xSemaphoreTake(this->state_mutex_, portMAX_DELAY);
    bool has_ssid = this->selected_ssid_[0] != '\0';
    xSemaphoreGive(this->state_mutex_);
    return has_ssid;
  }

  void init_wifi_portal_() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ESP_ERROR_CHECK(nvs_flash_init());
    } else if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      ESP_ERROR_CHECK(ret);
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      ESP_ERROR_CHECK(ret);
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler_, this, nullptr));

    esp_err_t mode_err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (mode_err != ESP_OK) {
      ESP_LOGW(TAG, "failed to keep WiFi in APSTA mode for portal: %s", esp_err_to_name(mode_err));
    }
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif != nullptr) {
      this->configure_ap_dhcp_options_(ap_netif);
    } else {
      ESP_LOGW(TAG, "AP netif not ready; captive portal DHCP hints skipped");
    }

    this->start_http_server_();
    xTaskCreate(dns_server_task_, "dns_server", 4096, nullptr, 5, nullptr);
  }
};

}  // namespace wifi_portal
}  // namespace esphome
