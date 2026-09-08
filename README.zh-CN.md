[English](README.md) | [简体中文](README.zh-CN.md)

# ONX2424G013

<img src="Product%20Images/ONX2424G013_01.jpg" alt="ONX2424G013" width="360">

## 产品概述

`ONX2424G013` 是 Open Nextion Genius 系列的 1.28 英寸智能旋钮屏，采用 `ESP32-S3R8`，集成圆形 IPS 显示屏、旋转编码器和实体按键，适合菜单导航、参数调节和联网控制界面。

本仓库收录硬件文档、产品图片、结构文件，以及 `ESP-IDF`、`ESP-Arduino`、`ESPHome` 三个平台共九个示例，提供源代码、中英文说明和合并后的完整固件。

## 购买链接

- 产品：[Open Nextion Genius 系列 1.28 英寸 ESP32-S3 智能旋钮屏](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/)

## 硬件规格

以下参数依据[官方售卖页面](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/)和仓库内的示例文档整理。更多硬件使用说明请参阅[官方 Wiki](https://nextion.tech/wiki/onx2424g013/)。

| 项目 | 规格 |
| --- | --- |
| 型号 | `ONX2424G013` |
| MCU | `ESP32-S3R8`，最高 `240 MHz` |
| Flash / PSRAM | `16 MB` Flash / `8 MB` OPI PSRAM |
| 显示屏 | `1.28 英寸`圆形 IPS，`240 x 240` |
| 色彩 / 亮度 | `262K` / `300 nit` |
| LCD 驱动 | `GC9A01` 系列，示例采用兼容 GC9A01 的驱动 |
| 交互方式 | 旋转编码器、实体 KEY 按键 |
| 无线连接 | `2.4 GHz` Wi-Fi、Bluetooth LE 5.0 |
| 供电 | `DC 5V/1A` |
| 接口 | USB-C、UART、六路 GPIO（`IO10`、`IO11`、`IO17`-`IO20`） |
| 工作温度 | `-20 至 70 °C` |
| 存储温度 | `-30 至 80 °C` |

## 主要特性

- 圆形显示屏配合旋转和按压操作，适合紧凑型图形交互界面。
- 支持 PWM 背光控制，示例演示旋转选择亮度、按键确认应用的交互方式。
- 提供 Wi-Fi 网页配网示例，在屏幕上显示连接状态。
- 支持三种开发方式，ESPHome Wi-Fi 示例支持 Home Assistant 自动发现。
- 附带预编译完整固件，可直接烧录体验示例。

## 仓库目录

| 目录 | 内容 |
| --- | --- |
| [Certifications/](Certifications/) | CE-RED、FCC ID、FCC SDOC 和 RoHS 认证资料 |
| [Datasheets/](Datasheets/) | ESP32-S3 数据手册和技术参考手册、GC9A01N 显示驱动数据手册 |
| [Drawings/](Drawings/) | 机械尺寸图、STEP 三维模型 |
| [Example Programs/](Example%20Programs/) | ESP-IDF、Arduino、ESPHome 示例、固件及硬件参考文档 |
| [LTA Announcement/](LTA%20Announcement/) | 产品公告文档 |
| [Product Images/](Product%20Images/) | 产品图片 |
| [Schematic & Layout/](Schematic%20%26%20Layout/) | 原理图和 PCB 位图 |

## 示例程序

三个平台均提供以下示例。每个示例目录包含 `README.md`、`README_cn.md` 和对应的 `*_factory.bin` 完整固件。

| 示例 | 功能 | ESP-IDF | Arduino | ESPHome |
| --- | --- | --- | --- | --- |
| `01_LCD_test` | LCD/LVGL 界面与实体 KEY 确认操作 | [查看](Example%20Programs/ESP-IDF/01_LCD_test/) | [查看](Example%20Programs/ESP-Arduino/01_LCD_test/) | [查看](Example%20Programs/ESPHome/01_LCD_test/) |
| `02_KNOB_KEY_BL_test` | 旋转选择背光亮度，按 KEY 应用 | [查看](Example%20Programs/ESP-IDF/02_KNOB_KEY_BL_test/) | [查看](Example%20Programs/ESP-Arduino/02_KNOB_KEY_BL_test/) | [查看](Example%20Programs/ESPHome/02_KNOB_KEY_BL_test/) |
| `03_WiFi_test` | 网页配网与 LCD 连接状态显示 | [查看](Example%20Programs/ESP-IDF/03_WiFi_test/) | [查看](Example%20Programs/ESP-Arduino/03_WiFi_test/) | [查看](Example%20Programs/ESPHome/03_WiFi_test/) |

## 快速开始

### 1. 连接设备

使用支持数据传输的 USB-C 线连接开发板与电脑。建议先运行 `01_LCD_test` 检查显示及 KEY 输入，再体验旋钮和 Wi-Fi 示例。以下命令中的 `<PORT>` 请替换为实际串口。

### 2. 烧录预编译固件

仓库中的 factory 固件是合并镜像，应从地址 `0x0` 写入。以下沿用示例文档中的 esptool.py 命令格式：

```bash
cd "Example Programs/ESP-IDF/01_LCD_test"
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

烧录会替换设备原有固件，请选用对应示例和开发平台的镜像。

### 3. 使用 ESP-IDF 编译

在已初始化 ESP-IDF 环境的终端中，从仓库根目录执行：

```bash
cd "Example Programs/ESP-IDF/01_LCD_test"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

项目通过 `main/idf_component.yml` 声明 `esp_lcd_gc9a01` 和 LVGL 依赖。请保留项目附带的配置文件；Wi-Fi 示例使用独立分区表，为应用分配更大的空间。

### 4. 使用 Arduino 编译

安装 ESP32 开发板包、`GFX Library for Arduino` 和 LVGL 8.x（示例推荐 `8.3.11`）。打开目标示例的 `.ino` 文件，按以下参数设置 Arduino IDE：

| 设置项 | 选项 |
| --- | --- |
| Board | `ESP32S3 Dev Module` |
| USB Mode | `Hardware CDC and JTAG` |
| USB CDC On Boot | `Enabled` |
| Upload Mode | `UART0 / Hardware CDC` |
| Flash Size | `16MB (128Mb)` |
| Partition Scheme | `16M Flash (3MB APP/9.9MB FATFS)` |
| PSRAM | `OPI PSRAM` |

保留与 sketch 同目录的 `build_opt.h`，然后编译上传。串口监视器波特率设为 `115200`。Arduino CLI 命令见各示例 README。

### 5. 使用 ESPHome 编译

安装 ESPHome 后，从仓库根目录执行：

```bash
cd "Example Programs/ESPHome/01_LCD_test"
esphome run 01_LCD_test.yaml
```

Wi-Fi 示例文档记录的验证版本为 ESPHome `2026.6.2`。如果 PlatformIO 拒绝含空格的路径，请将示例工作副本放到不含空格的路径下。Wi-Fi 示例的 `components/wifi_portal` 目录需与 YAML 文件一起保留。

### 6. 体验 Wi-Fi 配网

烧录 `03_WiFi_test` 后，用手机连接 `ONX2424G013` 热点。如果配网页面没有自动弹出，请打开 `http://192.168.4.1`。选择网络并输入密码后，LCD 会显示连接状态和分配的 IP 地址。

## 注意事项

- 示例使用旋钮和实体按键输入，LVGL 界面中的按钮通过 KEY 操作。
- Wi-Fi 仅支持 `2.4 GHz` 网络。
- 配网示例创建开放热点；ESPHome Wi-Fi 示例的 Native API 也未设置加密或密码。用于测试环境以外的场景前，请配置访问保护。
- GPIO19/GPIO20 是 ESP32-S3 的 USB 信号引脚。将扩展引脚用于其他功能前，请核对原理图及 USB 使用需求。
- 显示方向可在 `lcd_config.h`、`main/lcd_config.h` 或 YAML substitutions 中调整，具体取决于开发平台。
- 设计外壳前请查看[机械尺寸图](Drawings/ONX2424G013-Dimension.pdf)，连接外设前请查看[原理图和 PCB 文件](Schematic%20%26%20Layout/)。

## 参考资料

- [ONX2424G013 官方 Wiki](https://nextion.tech/wiki/onx2424g013/)
- [官方购买页面](https://itead.cc/product/open-nextion-genius-series-1-28-esp32-s3-smart-knob-screen/)
- [仓库内硬件参考文档](Example%20Programs/ONX2424G013_硬件Skill.md)
