# 01_LCD_test

本 ESP-IDF 例程用于驱动 ONX2424G013 的 240 x 240 圆形 SPI LCD，并使用 LVGL 绘制界面。

## 功能

- 使用 Espressif 官方 `esp_lcd_gc9a01` panel 组件初始化 GPIO5/GPIO1/GPIO2/GPIO3/GPIO8 上的 GC9A01A 兼容 SPI LCD。
- 配置 BGR 色序与显示反色。
- 使用 GPIO6 以 25 kHz PWM 控制背光，降低视角变化时可见的竖向纹理。
- 屏幕显示文本 `LCD_test`。
- 显示一个 LVGL 按钮组件。
- 将 GPIO9 的 KEY 物理按键映射为 LVGL keypad `ENTER`，按下/松开效果走 LVGL 原生按钮状态。
- 可在 `main/lcd_config.h` 通过宏定义配置屏幕镜像与旋转。

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

## 关键配置

```c
#define LCD_SPI_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_BACKLIGHT_PWM_HZ 25000
#define LCD_BRIGHTNESS_PERCENT 80
```

LCD 使用官方 `esp_lcd_gc9a01` 组件创建，本例程未提供自定义 vendor 初始化表。

## 方向配置

```c
#define LCD_MIRROR_X 1
#define LCD_MIRROR_Y 0
#define LCD_ROTATION_DEGREE 0 /* 0, 90, 180, 270 */
```

## 编译烧录

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

## 完整固件

例程目录中包含从 `0x0` 地址开始烧录的完整固件：

```text
ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

可使用以下命令直接烧录：

```bash
esptool.py --chip esp32s3 -p <PORT> -b 460800 write_flash 0x0 ONX2424G013_01_LCD_test_esp_idf_factory.bin
```

推荐硬件配置：16 MB Flash，8 MB OPI PSRAM。
