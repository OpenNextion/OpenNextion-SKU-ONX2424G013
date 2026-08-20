# 02_KNOB_KEY_BL_test

本 ESPHome 例程使用 LVGL 验证 ONX2424G013 的 LCD 背光、旋转编码器和 KEY 按键功能。

## 功能

- 复用 01 例程的 LCD 配置，使用 ESPHome `ili9xxx` GC9A01A 兼容显示驱动。
- 使用 GPIO6 以 25 kHz LEDC PWM 控制 LCD 背光。
- 使用 ESPHome 默认的 Light Gamma 修正值 `2.8` 进行亮度映射。
- 屏幕外围显示 220 px LVGL Arc，用于表示目标亮度。
- 屏幕中间显示当前亮度和目标亮度。
- GPIO48/GPIO47 上的旋转编码器调节目标亮度，范围为 10-100。
- 短按 GPIO9 上的 KEY 按键确认目标亮度，并将 LCD 背光切换到目标值。
- UI 几何尺寸、颜色、字体大小和交互行为与 ESP-IDF、ESP-Arduino 版本保持一致。

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

可修改 YAML substitutions 调整方向和旋钮灵敏度：

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
  knob_edges_per_brightness_step: "2"  # 中档
```

`knob_edges_per_brightness_step` 用于调节旋钮灵敏度，默认使用中档。推荐值为高档 `"1"`、中档 `"2"`、低档 `"4"`。

用户可调亮度范围为 `10..100`。ESPHome light 组件内部仍支持在启动阶段或服务调用中将背光完全关闭。

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
esphome run 02_KNOB_KEY_BL_test.yaml
```

如果 PlatformIO 因本地路径包含空格而拒绝编译，可将例程复制到无空格的临时路径编译，或移动到无空格路径。源 YAML 本身不依赖任何生成文件。

推荐硬件配置：ESP32-S3，16 MB Flash，8 MB OPI PSRAM。

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_02_KNOB_KEY_BL_test_esphome_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_02_KNOB_KEY_BL_test_esphome_factory.bin
```
