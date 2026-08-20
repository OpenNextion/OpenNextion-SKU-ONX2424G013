# 01_LCD_test

本 ESPHome 例程用于驱动 ONX2424G013 的 240 x 240 圆形 SPI LCD，并使用 LVGL 绘制界面。

## 功能

- 初始化 GPIO5/GPIO1/GPIO2/GPIO3/GPIO8 上的 GC9A01A 兼容 SPI LCD。
- 配置 BGR 色序与显示反色。
- 使用 GPIO6 以 25 kHz PWM 控制背光，降低视角变化时可见的竖向纹理。
- 屏幕显示文本 `LCD_test`。
- 显示一个 LVGL 按钮组件。
- 将 GPIO9 的 KEY 物理按键映射为 LVGL keypad `ENTER`。
- 开机后自动聚焦按钮，使 KEY 按下/松开和点击效果走 LVGL 原生按钮状态。
- 可通过 YAML substitutions 配置屏幕镜像与旋转。

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

## 方向配置

```yaml
substitutions:
  lcd_mirror_x: "true"
  lcd_mirror_y: "false"
  lcd_swap_xy: "false"
```

旋转说明：

- ONX2424G013 默认 0 度：`mirror_x: true`，`mirror_y: false`，`swap_xy: false`
- 90/270 度：启用 `swap_xy`，再按实际安装方向调整 `mirror_x` 和 `mirror_y`
- 180 度：同时将 `mirror_x` 和 `mirror_y` 设为 `true`

## 编译烧录

```bash
esphome run 01_LCD_test.yaml
```

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_01_LCD_test_esphome_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esphome_factory.bin
```

推荐硬件配置：ESP32-S3，16 MB Flash，8 MB OPI PSRAM。
