# 01_LCD_test

本 ESP-Arduino 例程用于驱动 ONX2424G013 的 240 x 240 圆形 SPI LCD，并使用 LVGL 绘制界面。

所需 Arduino 库：

- `lvgl` 8.x，推荐 8.3.11
- `GFX Library for Arduino`

功能：

- 初始化 GPIO5/GPIO1/GPIO2/GPIO3/GPIO8 上的 GC9A01A 兼容 SPI LCD。
- 使用 `GFX Library for Arduino` 提供的标准 `Arduino_GC9A01` 驱动。
- 使用 25 kHz、11-bit LEDC PWM 控制 GPIO6 背光。
- 屏幕显示文本 `LCD_test`。
- 显示一个 LVGL 按钮组件。
- 将 GPIO9 的 KEY 物理按键映射为 LVGL keypad ENTER，短按可确认当前聚焦按钮。
- 通过 USB Hardware CDC/JTAG 输出 115200 baud 调试日志，包含启动、LCD/LVGL 就绪和 KEY 确认事件。
- 可在 `lcd_config.h` 通过宏定义配置旋转。标准 Arduino_GFX GC9A01 驱动路径保持与已验证固件一致。

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

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_01_LCD_test_esp_arduino_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esp_arduino_factory.bin
```

该固件包含 bootloader、partition table、`boot_app0` 和应用程序镜像。

方向配置：

```c
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
```

Arduino IDE 配置：

- 安装 `esp32` 开发板包，并选择 **ESP32S3 Dev Module**。
- 按 ONX2424G013 硬件配置开发板选项：
  - USB Mode：**Hardware CDC and JTAG**
  - USB CDC On Boot：**Enabled**
  - Upload Mode：**UART0 / Hardware CDC**
  - Flash Size：**16MB (128Mb)**
  - Partition Scheme：**16M Flash (3MB APP/9.9MB FATFS)**
  - PSRAM：**OPI PSRAM**
- 打开 `ESP-Arduino/01_LCD_test/01_LCD_test.ino`。
- 保留与 sketch 同目录下的 `build_opt.h`。该文件用于启用本例程需要的 LVGL 编译选项，包括 `lv_font_montserrat_28`。
- 编译并上传。
- 如需查看日志，打开 Arduino IDE Serial Monitor 或其他串口工具，波特率设为 `115200`。
- 注意：本例程使用 `USB CDC On Boot: Enabled`，因此 Arduino `Serial` 会映射到 USB CDC/JTAG 日志口。

验证使用的 Arduino CLI 命令：

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" ESP-Arduino/01_LCD_test
arduino-cli upload -p <PORT> --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" ESP-Arduino/01_LCD_test
```

例程目录只保留源码、文档和完整合并固件。Arduino/PlatformIO 重新编译可生成的 `build`、`.pio`、临时输出目录等产物不应保留在例程目录中。
