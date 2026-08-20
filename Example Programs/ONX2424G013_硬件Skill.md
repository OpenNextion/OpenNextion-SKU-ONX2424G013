# ONX2424G013 硬件 Skill

## 角色

你是 ONX2424G013 的固件开发助手。你需要基于本文档理解硬件资源、接口约束和 bring-up 规则，然后为该硬件编写固件、驱动、测试程序或应用逻辑。

本文档只描述硬件事实和开发约束，不绑定任何具体软件平台。

## 硬件概览

ONX2424G013 是一款基于 ESP32-S3R8 的 1.28 寸旋钮屏硬件，包含圆形 LCD、旋钮编码器、KEY 按键、BOOT 按键、USB-C、物理 UART 和 8Pin 扩展 GPIO。

核心硬件能力：

- 主控：ESP32-S3R8
- Flash：16MB
- PSRAM：8MB
- 屏幕：1.28 寸圆形彩色 LCD
- 屏幕驱动 IC：GC9A01N
- 分辨率：240 x 240
- 屏幕接口：SPI
- 颜色格式：RGB565
- 旋钮：两相信号正交编码器
- 按键：KEY、BOOT
- 通信：USB-C、物理 UART
- 扩展：J4 8Pin GPIO 排座

## 总引脚定义

| 功能 | GPIO | 方向 | 约束 |
| --- | ---: | --- | --- |
| LCD SCLK | GPIO5 | 输出 | SPI 时钟 |
| LCD MOSI | GPIO1 | 输出 | SPI 数据输出 |
| LCD CS | GPIO2 | 输出 | LCD 片选 |
| LCD DC | GPIO3 | 输出 | LCD 命令/数据选择 |
| LCD RESET | GPIO8 | 输出 | LCD 复位 |
| LCD BL | GPIO6 | 输出 | PWM 背光 |
| Encoder A | GPIO48 | 输入 | 上拉，任意边沿 |
| Encoder B | GPIO47 | 输入 | 上拉，任意边沿 |
| KEY | GPIO9 | 输入 | 低电平有效，上拉 |
| BOOT | GPIO0 | 输入 | 低电平有效，上拉，启动绑带脚 |
| UART TX | GPIO43 | 输出 | 物理 UART |
| UART RX | GPIO44 | 输入 | 物理 UART |
| USB DM | GPIO19 | 双向 | USB D-，也出现在 J4 |
| USB DP | GPIO20 | 双向 | USB D+，也出现在 J4 |
| J4 IO11 | GPIO11 | 双向 | 扩展 GPIO |
| J4 IO18 | GPIO18 | 双向 | 扩展 GPIO |
| J4 IO17 | GPIO17 | 双向 | 扩展 GPIO |
| J4 IO10 | GPIO10 | 双向 | 扩展 GPIO |

## LCD 接口契约

LCD 使用 SPI 接口，分辨率为 240 x 240。

屏幕驱动 IC 为 GC9A01N。软件中可使用 GC9A01/GC9A01A 兼容驱动，但必须在实物上验证。

| LCD 信号 | GPIO |
| --- | ---: |
| SCLK | GPIO5 |
| MOSI | GPIO1 |
| CS | GPIO2 |
| DC | GPIO3 |
| RESET | GPIO8 |
| BL | GPIO6 |

LCD 驱动规则：

- 使用 SPI 写屏。
- 使用 GC9A01N/GC9A01 兼容初始化流程，优先使用平台官方或成熟 panel 驱动。
- 不使用 MISO 读屏。
- 像素格式使用 RGB565。
- 需要支持 240 x 240 全屏刷新。
- 颜色顺序按 BGR 处理。
- 显示需要开启颜色反转。
- SPI 模式使用 Mode 0。
- SPI 频率建议先使用 40MHz 验证稳定性，再根据实测情况提高。
- 平台驱动已经提供公开 API 时，不要在应用层手写屏幕控制器寄存器。尤其不要为了镜像直接在业务代码里写 MADCTL，除非平台没有驱动 API 且寄存器序列已在实物验证。

LCD 初始化顺序：

1. 先配置背光 PWM，duty 为 0，确保初始化阶段屏幕不亮。
2. 初始化 SPI 总线。
3. 配置 LCD CS、DC、RESET。
4. 执行 LCD 复位。
5. 初始化 LCD 控制器。
6. 设置颜色格式、颜色顺序和反色。
7. 通过平台驱动公开 API 配置旋转/镜像。
8. 打开显示输出。
9. 渲染或清除首个有效画面。
10. 输出首帧画面后再点亮背光。

背光规则：

- 背光由 GPIO6 PWM 控制。
- 建议 PWM 频率为 25kHz。
- 软件亮度建议抽象为 0-100%。
- duty 越高，背光越亮。
- 上电阶段应先关闭背光，避免首帧未准备好时闪白或闪屏。
- 条件允许时使用 25kHz、11-bit LEDC 配置。较低 PWM 频率在该屏幕/背光组合上可能出现随视角变化才能观察到的细微竖向条纹。
- 不要在 PWM 配置前短暂把 GPIO6 当普通 GPIO 拉高，否则可能把 LCD 初始化过程或旧帧内容显示出来，造成上电闪屏。

## 旋钮编码器契约

旋钮是 A/B 两相正交编码器。

| 信号 | GPIO |
| --- | ---: |
| A | GPIO48 |
| B | GPIO47 |

编码器规则：

- A/B 都配置为输入上拉。
- A/B 都建议开启任意边沿中断。
- 使用四状态正交解码，不要只依赖单边沿计数。
- 必须做机械消抖或非法跳变过滤。
- 建议以 detent 为业务单位，不要直接把每个边沿当作一格。
- 顺时针/逆时针方向必须以实物验证为准；如果方向相反，应交换 A/B 或在软件中反向。

推荐解码约束：

- 允许状态：00、01、11、10。
- 常见一格旋转会产生多个状态跳变。
- 初始状态应在启用中断前读取一次。

## 按键契约

| 按键 | GPIO | 有效电平 | 约束 |
| --- | ---: | --- | --- |
| KEY | GPIO9 | Low | 普通功能按键 |
| BOOT | GPIO0 | Low | 普通按键，同时影响启动模式 |

按键规则：

- 两个按键都使用输入上拉。
- 按下为低电平。
- 必须做消抖。
- BOOT 是启动绑带脚，固件和外设不能在上电复位阶段强行驱动该脚。

## 物理 UART 契约

| 信号 | GPIO |
| --- | ---: |
| TX | GPIO43 |
| RX | GPIO44 |

UART 规则：

- 使用 8N1。
- 默认无硬件流控。
- 调试或普通通信可使用 115200。
- 高速通信场景可使用 921600，需结合线长、转接器和误码率实测确认。
- 物理 UART 与 USB-C 是两个不同接口，不能混为同一个通道。

## USB-C 契约

USB-C 连接 ESP32-S3 原生 USB。

| USB 信号 | GPIO |
| --- | ---: |
| DM | GPIO19 |
| DP | GPIO20 |

USB 规则：

- USB-C 可用于下载、日志或串口通信。
- GPIO19/GPIO20 同时也是 USB DP/DM，不应在 USB 活跃时作为普通 GPIO 强驱动。
- 如果某个测试流程需要驱动 GPIO19/GPIO20，必须先确认 USB 功能不被依赖。

## J4 扩展 GPIO 契约

J4 是 8Pin 扩展排座。

| J4 信号 | GPIO/电源 |
| --- | --- |
| DP | GPIO20 / USB DP |
| DM | GPIO19 / USB DM |
| IO11 | GPIO11 |
| GND | GND |
| IO18 | GPIO18 |
| IO17 | GPIO17 |
| IO10 | GPIO10 |
| VCC | 电源 |

J4 GPIO 顺序参考：

1. IO11
2. DM(GPIO19)
3. DP(GPIO20)
4. IO18
5. IO17
6. IO10

J4 规则：

- GPIO 测试前应先把相关 IO 设置为安全默认电平。
- VCC 的电压和可供电能力不得在没有硬件确认时假设。
- DP/DM 作为 J4 GPIO 使用时会与 USB-C 功能冲突。

## Wi-Fi 相关硬件能力

Wi-Fi 由 ESP32-S3 提供。

规则：

- 可用于联网、扫描和 RSSI 读取。
- RSSI 测试应在连接目标 Wi-Fi 后读取信号强度。
- 不应把 Wi-Fi 连接失败直接等同于硬件故障；需要区分未扫描到、密码错误、认证失败、信号弱和超时。

## 上电与启动约束

硬件启动相关规则：

- GPIO0 参与启动模式选择。
- GPIO3 是启动敏感 GPIO，虽然用于 LCD DC，但上电阶段不应被外部电路错误拉动。
- USB-C 可用于下载模式和日志，但 GPIO19/GPIO20 与 J4 复用，需要避免测试冲突。
- LCD 背光应晚于 LCD 初始化和首帧画面打开。
- 如果固件启用 PSRAM，应在分配大屏幕缓冲前确认 PSRAM 初始化成功。

## Bring-Up 顺序

新固件建议按以下顺序验证：

1. 配置 ESP32-S3R8、16MB Flash、8MB PSRAM。
2. 打通日志输出。
3. 先以 0% duty 初始化背光 PWM，再初始化 LCD SPI 和 panel。
4. 显示黑、白、红、绿、蓝纯色，验证颜色顺序、反色和刷新。
5. 验证 KEY(GPIO9)。
6. 验证 BOOT(GPIO0)。
7. 验证旋钮 GPIO48/GPIO47 的计数和方向。
8. 验证物理 UART GPIO43/GPIO44。
9. 验证 Wi-Fi 扫描、连接和 RSSI。
10. 在 USB-C 不冲突的条件下验证 J4 GPIO。

## 不同平台开发建议

本硬件不要求所有底层驱动从零开发。开发时应优先使用平台现有外设框架、显示驱动、LVGL 适配层、GPIO/编码器/UART/Wi-Fi 组件，只在平台缺失或行为不满足时再补自定义驱动。

### ESP-IDF

推荐使用：

- LCD：优先使用 `esp_lcd` 框架和 GC9A01N 兼容的圆屏 SPI panel 驱动。
- UI：可使用 LVGL，并通过平台的 LVGL port 对接 LCD flush。
- 背光：使用 LEDC PWM。
- 按键：使用 GPIO input，也可使用官方 button 组件类库。
- 旋钮：可使用 GPIO 中断自行做四状态解码，也可使用 PCNT/RMT 等外设辅助，但要保留方向校准。
- UART：使用 UART driver，GPIO43/GPIO44。
- Wi-Fi：使用 ESP-IDF Wi-Fi station/scanner API。

ESP-IDF 配置注意：

- 目标芯片选择 ESP32-S3。
- Flash 配置为 16MB。
- PSRAM 配置为 8MB Octal PSRAM。
- 优先使用 Espressif managed/official GC9A01 panel 组件并接入 `esp_lcd` 框架，不要在官方组件可用时改写为手写屏幕驱动。
- LCD 使用 SPI write-only，不要配置 MISO 依赖。
- LCD 建议先以 40MHz SPI 验证，稳定后再提高。
- panel 配置应使用 BGR、RGB565 和颜色反转。
- panel 初始化后通过 `esp_lcd_panel_mirror()`、`esp_lcd_panel_swap_xy()` 配置镜像/旋转。
- GPIO6 背光使用 LEDC，建议 25kHz、11-bit duty；LVGL 首帧准备好后再点亮。
- 大尺寸屏幕缓冲优先放 PSRAM，但关键 ISR 和实时路径不要依赖 PSRAM。
- 若启用 USB Serial/JTAG 日志，不要把 GPIO19/GPIO20 同时作为普通 GPIO 输出。

### ESP-Arduino

推荐使用：

- LCD：优先使用成熟图形库中兼容 GC9A01N 圆形 SPI LCD 的驱动，例如 Arduino_GFX、LovyanGFX 或 TFT_eSPI 这类库，而不是从零写 SPI 刷屏。
- UI：可直接用 LVGL，也可用图形库原生绘图 API。
- 背光：使用 LEDC PWM。
- 按键：使用 GPIO input + debounce。
- 旋钮：可使用成熟 encoder 库，或用中断方式做四状态解码。
- UART：使用 `HardwareSerial` 映射到 GPIO43/GPIO44。
- Wi-Fi：使用 Arduino WiFi API。

ESP-Arduino 配置注意：

- 开发板/编译目标必须选择 ESP32-S3。
- Flash size 选择 16MB。
- PSRAM 选择 OPI/Octal PSRAM，并确认运行时可用。
- Arduino IDE 使用 ESP32 开发板包时，ESP32-S3 开发板配置应包含 16MB Flash 和 OPI/Octal PSRAM。分区方案应根据应用存储需求选择，但必须匹配 16MB Flash。
- 除非应用明确需要并重新验证显示启动行为，否则保持 USB CDC On Boot 关闭。
- USB 模式要按需求选择 CDC/USB Serial/JTAG；使用 USB 时注意 GPIO19/GPIO20 冲突。
- 图形库中需要设置 LCD 分辨率 240x240、SPI 引脚、CS/DC/RESET、BGR、反色。
- 使用 Arduino_GFX 时，优先使用标准 `Arduino_GC9A01` 驱动；ESP32-S3 上 SPI host 可使用 `FSPI`。
- 不要在高层 `Arduino_GFX` 对象上调用不可用或私有的 `writeCommand()` 类方法，应使用图形库公开 API。
- 不要在应用层直接写 GC9A01 MADCTL 等寄存器强行做镜像；Arduino_GFX 的该驱动没有公开 mirror API。如确需镜像，换用支持该能力的库/驱动，或在驱动层实现并实物验证。
- 若需要稳定的背光启动行为，可在 Arduino 中直接使用 ESP-IDF LEDC API：25kHz、11-bit、初始 duty 0，LVGL 首帧完成后再设置目标亮度。
- LVGL 配置应保持项目级可复现，并确保 Arduino IDE 与 Arduino CLI 对 LVGL 的 C/C++ 文件使用一致编译选项，包括应用 UI 所需字体。
- 如果颜色显示不对，优先检查 BGR/RGB 和 invert color，不要先怀疑硬件损坏。

### ESPHome

推荐使用：

- LCD：优先使用平台现有的 GC9A01N/GC9A01 兼容 SPI LCD 显示组件或兼容的自定义组件，不建议从零实现完整显示驱动。
- UI：优先使用 LVGL 组件做页面和控件渲染。
- 背光：使用 LEDC output 或 light 抽象。
- 按键：使用 GPIO binary sensor。
- 旋钮：使用 rotary encoder 组件；如方向相反，调整 A/B 或在逻辑层反向。
- UART：使用 UART 组件绑定 GPIO43/GPIO44。
- Wi-Fi：使用平台 Wi-Fi 配置和 RSSI 传感器/状态接口。

ESPHome 配置注意：

- `esp32` 目标应使用 ESP32-S3。
- `flash_size` 应配置为 16MB。
- PSRAM 应按 8MB Octal PSRAM 配置。
- LCD SPI 引脚按本文档固定，不要重分配。
- LCD 推荐先使用 40MHz SPI。
- 显示配置需要设置 240x240、BGR、invert color。
- 根据屏幕装配方向配置 transform。已验证基础方向为 mirror_x 开启、mirror_y 关闭、swap_xy 关闭。
- 小屏 LVGL 刷新建议尽量使用全屏 buffer，以保持刷新稳定。
- KEY(GPIO9) 应配置为低电平有效 binary sensor。LVGL 应用中应通过 LVGL 输入设备接入按键，让 LVGL 管理焦点、按下态和点击态，不要用裸 GPIO 回调手动伪造 UI 状态。
- 背光 LEDC 建议使用 25kHz，较低频率可能出现随视角变化的细微竖向条纹。
- 背光应默认关闭，首帧准备好后再打开。
- USB 日志与 USB 通信配置会影响 GPIO19/GPIO20，不要同时把 DP/DM 当 J4 GPIO 输出。

### 触摸输入说明

当前硬件 Skill 未定义触摸输入。不要因为平台提供触摸驱动组件就默认启用触摸。

如果后续硬件版本需要触摸：

- 先确认触摸控制器、I2C/SPI 引脚、中断脚和复位脚。
- 再选择平台现成触摸组件。
- 未确认前，不要占用 GPIO7/GPIO8 或其他已定义引脚作为触摸接口。

## AI 开发规则

后续 AI 编程时必须遵守：

- 不要重新分配已定义的硬件引脚。
- 不要把 USB-C 和物理 UART 当成同一个接口。
- 不要在 USB-C 活跃时随意驱动 GPIO19/GPIO20。
- 不要假设 J4 VCC 的电压和供电能力。
- 不要假设 LCD 可以读数据；当前硬件按无 MISO 写屏处理。
- 不要把已经验证可用的官方/平台显示驱动替换成手写驱动。
- 平台驱动存在公开 API 时，不要在应用层直接写屏幕控制器寄存器。
- 不要把板卡内存配置改成 4MB Flash 或关闭 PSRAM；本硬件是 ESP32-S3R8，16MB Flash，8MB Octal PSRAM。
- 调试显示问题时，不要随意修改 Arduino USB CDC 或分区选项，除非这正是待验证变量。
- 不要假设旋钮方向，必须保留方向校准能力。
- 不要在上电第一时间打开背光显示未初始化画面。
- 不要把背光 GPIO HIGH/LOW 探测逻辑作为最终实现。最终应使用 25kHz LEDC PWM，启动 duty 为 0，首帧后设置目标亮度。
- 不要把 BOOT(GPIO0) 当普通无约束 GPIO 使用。
- 不要引入与硬件无关的平台概念作为硬件事实。

## 最小引脚常量

```c
#define ONX_LCD_SCLK_GPIO      5
#define ONX_LCD_MOSI_GPIO      1
#define ONX_LCD_CS_GPIO        2
#define ONX_LCD_DC_GPIO        3
#define ONX_LCD_RESET_GPIO     8
#define ONX_LCD_BACKLIGHT_GPIO 6

#define ONX_ENCODER_A_GPIO     48
#define ONX_ENCODER_B_GPIO     47

#define ONX_KEY_GPIO           9
#define ONX_BOOT_GPIO          0

#define ONX_UART_TX_GPIO       43
#define ONX_UART_RX_GPIO       44

#define ONX_USB_DM_GPIO        19
#define ONX_USB_DP_GPIO        20

#define ONX_J4_IO11_GPIO       11
#define ONX_J4_IO18_GPIO       18
#define ONX_J4_IO17_GPIO       17
#define ONX_J4_IO10_GPIO       10
```
