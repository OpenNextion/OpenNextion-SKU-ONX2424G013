# 03_WiFi_test

本 ESP-IDF 例程使用 LVGL 验证 ONX2424G013 的 WiFi 配网功能。

## 功能

- 复用前面 ESP-IDF LCD 例程中已验证稳定的屏幕初始化流程。
- 上电后 LCD 显示 `WiFi_test`。
- 设备开启无密码 AP 热点，热点名称为 `ONX2424G013`。
- 设备在 `192.168.4.1` 上运行配网页面。
- 内置 DNS 响应任务，将手机 captive portal 探测解析到设备本机。
- 进入配网页面后主动扫描周围 WiFi，也可手动刷新。
- 用户可选择 SSID、输入密码并点击连接。
- LCD 显示选中的 WiFi 名称、连接状态和 STA IP 地址。
- LCD 底部提示使用短文本换行，避免超出圆屏边界。

## 使用方法

1. 编译并烧录固件。
2. 手机连接无密码热点 `ONX2424G013`。
3. 手机系统通常会自动弹出配网页面。
4. 如果未自动弹出，可手动访问 `http://192.168.4.1`。
5. 选择目标 WiFi，输入密码，点击 **Connect**。
6. 通过 LCD 查看连接状态和 IP 地址。

## 关键配置

如需调整，可修改 `main/lcd_config.h`：

```c
#define WIFI_AP_SSID "ONX2424G013"
#define WIFI_PORTAL_IP "192.168.4.1"
#define WIFI_SCAN_MAX_AP 20
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100
```

本例程为配网测试例程，AP 热点故意配置为无密码。

## 实现说明

- HTTP server 栈已加大，用于承受手机系统 captive portal 探测带来的请求突发。
- WiFi 扫描结果从 heap 分配，避免占用 HTTP server 任务栈。
- `max_open_sockets` 保持为 `7`，匹配 ESP-IDF/LwIP 默认 socket 限制。
- 背光使用 25 kHz LEDC PWM，并显式选择 APB 时钟源。

## 编译

```bash
idf.py set-target esp32s3
idf.py build
```

推荐硬件配置：16 MB Flash，8 MB OPI PSRAM。

本例程使用 `partitions.csv`，将 factory app 分区配置为 3 MB。原因是 WiFi、HTTP captive portal 和 LVGL 同时启用后，固件体积会超过 ESP-IDF 默认的 1 MB factory 分区。

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_03_WiFi_test_esp_idf_factory.bin
```

可使用以下命令直接烧录：

``` bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esp_idf_factory.bin
```
