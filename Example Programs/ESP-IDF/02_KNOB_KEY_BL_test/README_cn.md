# 02_KNOB_KEY_BL_test

本 ESP-IDF 例程使用 LVGL 验证 ONX2424G013 的 LCD 背光、旋转编码器和 KEY 按键功能。

## 功能

- 复用 01 例程的 LCD 初始化流程，使用 Espressif 官方 `esp_lcd_gc9a01` panel 组件。
- 使用 GPIO6 以 25 kHz、11-bit LEDC PWM 控制 LCD 背光，并支持 Gamma 亮度映射。
- 屏幕外围显示 220 px LVGL Arc，用于表示目标亮度。
- 屏幕中间显示当前亮度和目标亮度。
- GPIO48/GPIO47 上的旋转编码器调节目标亮度，范围为 10-100。
- 短按 GPIO9 上的 KEY 按键确认目标亮度，并将 LCD 背光切换到目标值。
- UI 几何尺寸、颜色、字体大小和交互行为与 ESP-Arduino、ESPHome 版本保持一致。

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

可在 `main/lcd_config.h` 中调整硬件和交互配置：

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

## 编译烧录

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_02_KNOB_KEY_BL_test_esp_idf_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_02_KNOB_KEY_BL_test_esp_idf_factory.bin
```

推荐硬件配置：16 MB Flash，8 MB OPI PSRAM。
