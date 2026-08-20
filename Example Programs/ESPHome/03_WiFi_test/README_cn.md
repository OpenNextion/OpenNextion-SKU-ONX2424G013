# 03_WiFi_test

本 ESPHome 例程使用 LVGL 验证 ONX2424G013 的 WiFi 配网功能。

## 功能

- 复用 ESPHome 01/02 例程中已验证稳定的 LCD/LVGL 配置。
- LCD 端 UI 布局与 ESP-IDF 03 例程保持一致。
- 设备开启无密码 AP 热点，热点名称为 `ONX2424G013`。
- 设备在 `192.168.4.1` 上运行自定义配网页面。
- 使用 ESPHome `wifi` 组件管理 AP/STA 状态，保证 Native API、mDNS 和 Home Assistant 看到正确的联网状态。
- 使用 `components/wifi_portal` 本地 ESPHome external component 实现 DNS、HTTP、WiFi 扫描页面和配网提交逻辑。
- 进入配网页面后主动扫描周围 WiFi，也可手动刷新。
- 用户可选择 SSID、输入密码并点击连接。
- LCD 显示选中的 WiFi 名称、连接状态和 STA IP 地址。
- 启用无加密、无密码的 ESPHome Native API，支持 Home Assistant 自动发现和添加。

## 关键配置

如需调整 LCD 方向，可修改 YAML substitutions：

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
```

AP 和 captive portal 常量位于 `components/wifi_portal/wifi_portal.h`：

```cpp
constexpr const char *WIFI_AP_SSID = "ONX2424G013";
constexpr const char *WIFI_PORTAL_IP = "192.168.4.1";
```

本例程为配网测试例程，AP 热点故意配置为无密码。

Home Assistant 自动发现使用标准 ESPHome Native API：

```yaml
network:
  enable_ipv6: false

wifi:
  ap:
    ssid: "ONX2424G013"
    ap_timeout: 0s
  reboot_timeout: 0s
  power_save_mode: none

api:
  reboot_timeout: 0s
```

本例程未配置 `encryption` 密钥，也未配置 API `password`。配网成功并接入局域网后，ESPHome 的 `wifi` 组件会将网络状态更新为 connected，Home Assistant 可以自动发现并无密钥添加该设备。

## UI 一致性

LCD UI 对齐 ESP-IDF 03 例程：

- 标题：`WiFi_test`，Montserrat 28
- 状态文本：Montserrat 14
- 背景色：`0x101820`
- AP、状态、IP、提示文本的位置和颜色与 ESP-IDF 版本一致
- 底部提示拆成短文本换行，避免超出圆屏边界

网页配网页面使用与 ESP-IDF 版本一致的路由结构和视觉样式：

- `/` 和未知路径返回配网页面
- `/scan` 返回周围 WiFi AP 的 JSON 列表
- `/connect` 接收 `ssid` 和 `password`，保存到 ESPHome `wifi` 组件并发起 STA 连接
- `/status` 返回选中 SSID、连接状态和 STA IP

## 实现说明

ESPHome 版本不能像 ESP-IDF/ESP-Arduino 版本一样，在 HTTP `/scan` 处理函数里直接调用 `esp_wifi_scan_get_ap_records()` 读取扫描结果。ESPHome `wifi` 组件也会监听 `WIFI_EVENT_SCAN_DONE`，并在事件处理中读取 IDF 扫描结果；该读取动作会消费底层扫描结果缓存。

因此本例程在 `components/wifi_portal/__init__.py` 中调用 `wifi.request_wifi_scan_results()`，要求 ESPHome 保留完整扫描结果；`/scan` 触发底层扫描后，再从 `wifi::global_wifi_component->get_scan_result()` 读取缓存并生成网页 JSON 列表。这样既能保持配网页面的 WiFi 列表功能，又能让 ESPHome `wifi` 组件继续管理 STA 状态、mDNS 和 Native API。

提交配网信息时，`/connect` 通过 ESPHome `wifi` 组件保存并连接 STA，而不是直接绕过组件长期管理 WiFi。这样配网完成后 Home Assistant 能看到正确的在线状态。

## 编译和上传

```bash
esphome run 03_WiFi_test.yaml
```

如果 PlatformIO 不接受带空格的本地路径，可从不带空格的临时路径编译，或将例程复制到不带空格的路径。源码 YAML 本身不依赖生成文件。本例程已使用 ESPHome `2026.6.2` 编译验证。

推荐硬件配置：ESP32-S3，16 MB Flash，8 MB OPI PSRAM。

## 完整固件

编译完成后，将生成的 factory 镜像复制到本例程目录，命名为：

```text
ONX2424G013_03_WiFi_test_esphome_factory.bin
```

该 factory 镜像是从 Flash 地址 `0x0` 开始烧录的完整固件：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esphome_factory.bin
```

在 ESPHome/PlatformIO 编译目录中，源文件通常位于：

```text
.esphome/build/onx2424g013-03-wifi-test/.pioenvs/onx2424g013-03-wifi-test/firmware.factory.bin
```

例程目录只保留源码、文档和上述完整固件。`.esphome`、`.pioenvs`、`managed_components` 等目录均为可重新生成的编译产物，不应提交或随例程发布。
