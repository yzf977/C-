# 毕业设计 — 拾音频谱节奏灯

> 基于 RT-Thread 的 STM32F407 音频频谱分析 + WS2812B 灯带可视化项目。通过 ADC 采集音频信号，用 CMSIS-DSP 做 FFT 变换，在 8×32 LED 矩阵上实时显示频谱效果，同时 OLED 显示参数菜单。

**硬件：** STM32F407ZGT6，0.96 寸 SPI OLED 和 I2C OLED 各一块，8×32 WS2812B 灯带，8 位独立按键，5V 4A 外部电源供电。

**软件：** RT-Thread Studio 4.1.0，STM32CubeMX。

---

## 硬件连接

### 主控芯片
**STM32F407ZGT6**（168MHz / APB1 42MHz / APB2 84MHz）

### 接线表

| 器件            | 器件引脚    | STM32 引脚 | 功能说明            |
|----------------|------------|-----------|---------------------|
| **I2C OLED**   | SCL        | **PB6**   | I2C1 时钟线         |
| (SSD1306)      | SDA        | **PB7**   | I2C1 数据线         |
|                | VCC        | 3.3V      | 电源正极            |
|                | GND        | GND       | 电源地              |
| **SPI OLED**   | SCK        | **PA5**   | SPI1 时钟           |
|                | MOSI       | **PA7**   | SPI1 数据输出       |
|                | VCC        | 3.3V      | 电源正极            |
|                | GND        | GND       | 电源地              |
| **WS2812B 灯带** | DIN       | **PC3**   | SPI2 MOSI 数据输出  |
|                | VCC        | 5V        | 灯带电源（外接）     |
|                | GND        | GND       | 共地               |
| **音频输入**    | OUT        | **PA1**   | ADC1 通道1，模拟输入 |
|                | GND        | GND       | 共地               |
| **按键 1-5**   | —          | **PD1~PD5** | GPIO 输入，内部上拉  |
| **按键 6-8**   | —          | **PE7~PE9** | GPIO 输入，内部上拉  |

### 接线示意图

```
                       STM32F407ZGT6
                    ┌─────────────────┐
                    │                 │
  I2C OLED SCL ────┤ PB6 (I2C1_SCL)  │
  I2C OLED SDA ────┤ PB7 (I2C1_SDA)  │
                    │                 │
  SPI OLED SCK ────┤ PA5 (SPI1_SCK)  │
  SPI OLED MOSI ───┤ PA7 (SPI1_MOSI) │
                    │                 │
  WS2812B DIN ─────┤ PC3 (SPI2_MOSI) │
                    │                 │
  Audio IN ────────┤ PA1 (ADC1_IN1)  │
                    │                 │
  KEY1 ────────────┤ PD1             │
  KEY2 ────────────┤ PD2             │
  KEY3 ────────────┤ PD3             │
  KEY4 ────────────┤ PD4             │
  KEY5 ────────────┤ PD5             │
  KEY6 ────────────┤ PE7             │
  KEY7 ────────────┤ PE8             │
  KEY8 ────────────┤ PE9             │
                    │                 │
  外接 5V 4A ──────┤ 灯带 VCC        │
  GND ─────────────┴─ 共地！          │
                    └─────────────────┘
```

> ⚠️ **注意：**
> - WS2812B 灯带 256 颗 LED，**必须外接 5V 电源**（本设计使用 5V 4A）
> - 灯带 GND 必须和 STM32 GND **共地**
> - 音频输入是模拟信号，建议加隔直电容和偏置电路（中点电压 ~1.65V）
> - SPI2 用 5.25MHz 驱动 WS2812B，每个 bit 用 2 个 SPI 字节模拟（0xF0=1, 0xC0=0）

---

## LED 矩阵布局

8 行 × 32 列 = 256 颗 WS2812B，蛇形走线：

```
列: 0   1   2   3  ...  30  31
    ↓   ↑   ↓   ↑        ↑   ↓
行0  ●   ●   ●   ●  ...   ●   ●
行1  ●   ●   ●   ●  ...   ●   ●
行2  ●   ●   ●   ●  ...   ●   ●
...  ●   ●   ●   ●  ...   ●   ●
行7  ●   ●   ●   ●  ...   ●   ●
    ↓   ↑   ↓   ↑        ↑   ↓
```

---

## 按键功能

| 按键  | 功能                     | 范围/选项                             |
|-------|--------------------------|---------------------------------------|
| **KEY1** | 频谱显示模式切换        | BAR / PEAK / DOT                      |
| **KEY2** | 灯带效果模式切换        | FLOW / RAINBOW / BREATH / COL / DUAL / ROLL / LOW / HIGH |
| **KEY3** | 亮度调节                | 5% ~ 100%（回环）                     |
| **KEY4** | 灵敏度调节              | 0.5 ~ 2.0（步进 0.1）                |
| **KEY5** | 增益调节                | 0.5 ~ 3.0（步进 0.1）                |
| **KEY6** | 噪声阈值调节             | 0 ~ 50（步进 2）                      |
| **KEY7** | 一键恢复默认             | 复位所有参数                           |
| **KEY8** | 采样率切换              | 10k / 12k / … / 48kHz                |

---

## OLED 菜单界面

```
┌──────────────────────────────┐
│ LED:FLOW        SPC:BAR     │
│ BRT:  5%        SEN:100     │
│                 GAIN:100%   │
│                 THR:  0     │
│                 SR:48k      │
│                              │
│ 1SPC 2LED 3BRT 4SEN         │
│ 5GAIN 6THR 7RST 8SR         │
└──────────────────────────────┘
```

---

## 软件架构

```
main.c
├── adc_capture.c      — ADC1 采集 + TIM2 触发 (10~48kHz)
├── fft_processor.c    — CMSIS-DSP 512点 FFT → 64频段
├── spectrum_display.c — OLED 频谱绘制 (BAR/PEAK/DOT)
├── ws2812b_spi.c      — SPI2 驱动 WS2812B (8种灯效)
├── menu_display.c     — SSD1306 OLED 参数菜单
└── key_handler.c      — 8键扫描 + 回调注册

RT-Thread 线程:
  "spectrum_proc" — 频谱绘制 (优先级 15, 4KB 栈)
  "led_show"      — 灯带刷新   (优先级 12, 2KB 栈)
  "menu_update"   — 菜单刷新   (优先级 18, 2KB 栈)
  "key_scan"      — 按键扫描   (优先级  3, 1KB 栈)
  "audio_cap"     — 音频采集   (优先级 18, 8KB 栈)
```

---

## 编译与烧录

### 环境

- **RT-Thread Studio 4.1.0**
- STM32CubeMX（HAL 库）
- ARM GCC 工具链

### 编译

```bash
# RT-Thread Studio 中直接 Build
# 或命令行：
scons
```

### 烧录

使用 ST-Link / J-Link 在 RT-Thread Studio 中点击 Download，或命令行：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program rtthread.bin 0x08000000 verify reset exit"
```

---

## 依赖包

| 包名              | 用途            |
|-------------------|-----------------|
| **CMSIS-DSP**     | FFT 浮点运算    |
| **U8g2**          | OLED 图形库     |
| **SSD1306**       | OLED 驱动       |

---

## 性能参数

| 参数          | 值                |
|--------------|-------------------|
| FFT 点数      | 512               |
| 频率分辨率     | 93.75 Hz @ 48kHz  |
| 频率范围       | 0 ~ 24kHz         |
| 频谱频段       | 64 段             |
| LED 刷新率     | 60 FPS            |
| 采样率范围     | 10 ~ 48kHz（可调） |
| OLED 刷新周期  | 500ms             |

---

## 项目结构

```
├── applications/         — 应用层代码
│   ├── main.c                主程序入口
│   ├── adc_capture.*         ADC 音频采集
│   ├── fft_processor.*       FFT 频谱分析
│   ├── spectrum_display.*    OLED 频谱显示
│   ├── ws2812b_spi.*         WS2812B 灯带驱动
│   ├── menu_display.*        OLED 参数菜单
│   ├── key_handler.*         按键处理
│   └── tim2_trigger.*        TIM2 采样率触发器
├── cubemx/               — CubeMX 配置（HAL 库代码）
├── drivers/              — RT-Thread 板级驱动
├── libraries/            — STM32 HAL 库
├── packages/             — RT-Thread 软件包
│   ├── CMSIS-DSP-latest/
│   ├── ssd1306-latest/
│   └── u8g2-official-latest/
├── rt-thread/            — RT-Thread 内核源码
└── linkscripts/          — 链接脚本
```
