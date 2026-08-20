# 03_WiFi_test

本 ESP-Arduino 例程使用 LVGL 验证 ONX2424G013 的 WiFi 配网功能。

所需 Arduino 库：

- `lvgl` 8.x，推荐 8.3.11
- `GFX Library for Arduino`

## 功能

- 复用 ESP-Arduino 01/02 例程中已验证稳定的 LCD 和 LVGL 初始化流程。
- LCD 端 UI 布局与 ESP-IDF 03 例程保持一致。
- 设备开启无密码 AP 热点，热点名称为 `ONX2424G013`。
- 设备在 `192.168.4.1` 上运行配网页面。
- 使用通配 DNS，让手机 captive portal 探测解析到设备本机。
- 进入配网页面后主动扫描周围 WiFi，也可手动刷新。
- WiFi 扫描在 `/scan` 请求处理中使用 ESP-IDF WiFi 扫描 API，行为对齐 ESP-IDF 03 例程。
- 用户可选择 SSID、输入密码并点击连接。
- LCD 显示选中的 WiFi 名称、连接状态和 STA IP 地址。
- 通过 USB Serial/JTAG 端口输出 115200 baud 诊断日志。

## 关键配置

如需调整，可修改 `lcd_config.h`：

```c
#define WIFI_AP_SSID "ONX2424G013"
#define WIFI_PORTAL_IP "192.168.4.1"
#define WIFI_SCAN_MAX_AP 20
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 100
```

本例程为配网测试例程，AP 热点故意配置为无密码。

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
- `/connect` 接收 `ssid` 和 `password`
- `/status` 返回选中 SSID、连接状态和 STA IP
- `/js_start` 和 `/js_error` 仅用于 USB 串口排查页面脚本执行情况。

## Arduino IDE 配置

- 安装 `esp32` 开发板包，并选择 **ESP32S3 Dev Module**。
- 按 ONX2424G013 硬件配置开发板选项：
  - USB Mode：**Hardware CDC and JTAG**
  - USB CDC On Boot：**Enabled**
  - Upload Mode：**UART0 / Hardware CDC**
  - Flash Size：**16MB (128Mb)**
  - Partition Scheme：**16M Flash (3MB APP/9.9MB FATFS)**
  - PSRAM：**OPI PSRAM**
  - Core Debug Level：**Info**
- 打开 `ESP-Arduino/03_WiFi_test/03_WiFi_test.ino`。
- 保留与 sketch 同目录下的 `build_opt.h`。该文件用于启用本例程需要的 LVGL 编译选项，包括 `lv_font_montserrat_28`。
- 编译并上传。

Arduino CLI 命令：

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,DebugLevel=info" ESP-Arduino/03_WiFi_test
arduino-cli upload -p <PORT> --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,DebugLevel=info" ESP-Arduino/03_WiFi_test
```

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_03_WiFi_test_esp_arduino_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_03_WiFi_test_esp_arduino_factory.bin
```
