# 02_KNOB_KEY_BL_test

本 ESP-Arduino 例程使用 LVGL 验证 ONX2424G013 的 LCD 背光、旋转编码器和 KEY 按键功能。

所需 Arduino 库：

- `lvgl` 8.x，推荐 8.3.11
- `GFX Library for Arduino`

## 功能

- 复用 01 例程的 LCD 实现，使用 `GFX Library for Arduino` 中的标准 `Arduino_GC9A01` 驱动。
- 使用 ESP-IDF LEDC API，以 25 kHz、11-bit 分辨率控制 GPIO6 上的 LCD 背光，并支持 Gamma 亮度映射。
- 屏幕外围显示 220 px LVGL Arc，用于表示目标亮度。
- 屏幕中间显示当前亮度和目标亮度。
- GPIO48/GPIO47 上的旋转编码器调节目标亮度，范围为 10-100。
- 短按 GPIO9 上的 KEY 按键确认目标亮度，并将 LCD 背光切换到目标值。
- 通过 USB Hardware CDC/JTAG 输出 115200 baud 调试日志，包含启动、LCD/LVGL/输入就绪和 KEY 确认后的亮度事件。旋转编码器只更新 UI，不打印中间过程数值。
- UI 几何尺寸、颜色、字体大小和交互行为与 ESP-IDF、ESPHome 版本保持一致。

## 引脚分配

| 功能 | GPIO |
| --- | --- |
| LCD SCLK | GPIO5 |
| LCD MOSI | GPIO1 |
| LCD CS | GPIO2 |
| LCD DC | GPIO3 |
| LCD RST | GPIO8 |
| LCD BL | GPIO6 |
| KEY | GPIO9 |
| Encoder A | GPIO48 |
| Encoder B | GPIO47 |

## 关键配置

可在 `lcd_config.h` 中调整硬件和交互配置：

```c
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_MIN_PERCENT 10
#define LCD_BRIGHTNESS_PERCENT 100
#define LCD_BACKLIGHT_GAMMA 2.8f
#define KNOB_EDGES_PER_BRIGHTNESS_STEP KNOB_SENSITIVITY_MEDIUM
#define KNOB_DIRECTION_INVERT 0
```

`KNOB_EDGES_PER_BRIGHTNESS_STEP` 用于调节旋钮灵敏度，默认使用中档。预设值为高档 `1`、中档 `2`、低档 `4`。

`LCD_BRIGHTNESS_MIN_PERCENT` 限制 Arc 上显示和用户可调的亮度范围。背光设置函数内部仍保留 `0` 作为启动阶段灭屏/关闭背光用途，避免首帧绘制完成前提前亮屏。

## UI 一致性

三个平台例程使用相同的 LVGL 界面参数：

- 背景色：`0x101820`
- Arc 尺寸：`220 x 220`
- Arc 范围：`10..100`
- Arc 线宽：`12`
- 目标亮度颜色：`0x46C2FF`
- 文字字体：`montserrat_14`
- 文字行距：`4`
- 背光 Gamma：`2.8`

`build_opt.h` 必须保留在 sketch 同目录下，它用于启用本例程所需的 LVGL 配置和共享字体。

## Arduino IDE 配置

- 安装 `esp32` 开发板包，并选择 **ESP32S3 Dev Module**。
- 按 ONX2424G013 硬件配置开发板：
  - USB Mode：**Hardware CDC and JTAG**
  - USB CDC On Boot：**Enabled**
  - Upload Mode：**UART0 / Hardware CDC**
  - Flash Size：**16MB (128Mb)**
  - Partition Scheme：**16M Flash (3MB APP/9.9MB FATFS)**
  - PSRAM：**OPI PSRAM**
- 打开 `ESP-Arduino/02_KNOB_KEY_BL_test/02_KNOB_KEY_BL_test.ino`。
- 编译并上传。
- 如需查看日志，打开 Arduino IDE Serial Monitor 或其他串口工具，波特率设为 `115200`。
- 注意：本例程使用 `USB CDC On Boot: Enabled`，因此 Arduino `Serial` 会映射到 USB CDC/JTAG 日志口。
- 旋转编码器不打印中间过程数值，短按 KEY 确认时打印最终确认的亮度。

用于验证的 Arduino CLI 命令：

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" ESP-Arduino/02_KNOB_KEY_BL_test
```

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_02_KNOB_KEY_BL_test_esp_arduino_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_02_KNOB_KEY_BL_test_esp_arduino_factory.bin
```

该固件包含 bootloader、partition table、`boot_app0` 和应用程序镜像。

例程目录只保留源码、文档和完整合并固件。Arduino/PlatformIO 重新编译可生成的 `build`、`.pio`、临时输出目录等产物不应保留在例程目录中。
