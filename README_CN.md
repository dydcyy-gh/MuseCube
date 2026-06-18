[![English](https://img.shields.io/badge/README-English-blue)](README.md)

# Muse Cube

**完全开源的多功能手持终端。**

硬件与软件全面开放、可二次开发，专为随身携带而设计。

[![Hardware](https://img.shields.io/badge/Hardware-OSHWHub-green)](https://oshwhub.com/dydcyy/mp3-based-on-stm32)
[![Software](https://img.shields.io/badge/Software-GitHub-black)](https://github.com/dydcyy-gh/MuseCube)

---

## 概述

Muse Cube 是一款基于 STM32F405RGT6 的超小型（8 × 5 × 2.2 cm）手持终端设备。它集成了高保真音乐播放器、视频播放器、NES 游戏模拟器、USB 从机/主机、桌面助手以及多种创意工具于一身。PCB、3D 打印外壳、固件全部开源。

### 硬件亮点

| 模块 | 规格 |
|------|------|
| **主控** | STM32F405RGT6 @ 168 MHz (Cortex-M4F)，128 KB SRAM + 64 KB CCM，1 MB Flash |
| **屏幕** | 1.54″ IPS，240×240 分辨率，色彩鲜艳 |
| **音频** | ES9018K2M DAC + 专用耳机放大器 (HIFI 级 3.5 mm 输出)；双扬声器 (MAX98357A 驱动) |
| **存储** | 16 MB 板载 SPI Flash (W25Q128) + TF 卡槽 (最高 32 GB，SDIO 24 MHz DMA) |
| **输入** | 双 Switch 摇杆 |
| **接口** | USB-C (充电 / 数据 / OTG 主机)，3.5 mm 耳机孔 |
| **电池** | 2000 mAh 锂电池，支持全天音乐播放或桌面使用 |
| **尺寸** | 8 × 5 × 2.2 cm，口袋大小，M1.6/M2 螺丝 + 热熔螺母组装 |

---

## 功能一览

### 🎵 音乐播放

基于高品质双扬声器 / HIFI 级 3.5 mm 输出，ES9018K2M DAC 所有芯片参数可自定义。

| 格式 | 最高规格 |
|------|----------|
| WAV | 32 bit / 96 kHz |
| MP3 | 16 bit / 48 kHz |
| FLAC | 24 bit / 96 kHz |
| AAC | 16 bit / 96 kHz |
| OGG | 16 bit / 32 kHz |
| APE | 16 bit / 48 kHz |
| AIFF | 32 bit / 96 kHz |

### 🔌 USB 功能 (Type-C 接口，基于 CherryUSB 1.60，USB FS DMA，主从自动识别/切换)

**USB Device (从机模式)：**

| 功能 | 说明 |
|------|------|
| **UF2** | 基于 CherryUSB UF2，无需第三方软件即可实现 USB 固件更新 |
| **UAC Device** | UAC 2.0 最高 32 bit / 96 kHz（多种采样率可选）；UAC 1.0 固定 16 bit / 48 kHz（兼容性模式），均开启反馈 |
| **CDC Device** | 虚拟串口，用于日志输出 / Letter Shell 调试 |
| **Display** | Windows 系统 USB 副屏功能（需安装驱动），约 10 fps @ 240×240 |
| **HID Device** | 模拟鼠标 / 模拟键盘 / 模拟 Xbox 摇杆 |
| **MSC Device** | 模拟 U 盘读取 SD 卡文件 |

**USB Host (主机模式)：**

| 功能 | 说明 |
|------|------|
| **HID Host** | 外接键盘 / 鼠标 / 摇杆 |
| **MSC Host** | U 盘与 U 盘、U 盘与 SD 卡之间文件访问 / 复制 / 粘贴 / 删除 |
| **Serial Host** | （实验性功能）串口助手上位机 |

### 🖼️ 流媒体功能

| 类型 | 格式 | 说明 |
|------|------|------|
| **图片** | BMP | 任意尺寸 |
| | PNG | 宽度 < 319 px |
| | GIF | 宽度 < 480 px（含动画） |
| | JPG | 任意尺寸 |
| **视频** | MJPEG | ~30 fps @ 240×240 @ Q3 |
| | RAW | ~25 fps @ 240×240 (RGB565) |
| | AVI | ~30 fps @ 240×240 @ Q3 |
| **文本** | .c / .h / .txt / .py 等 | — |
| **游戏** | NES | 支持大部分 NES 游戏 ROM |

### 🛠️ 其他系统功能

- 设置参数掉电不丢失（FlashDB KV 存储）
- 文件管理器（SD 卡热插拔自动挂载/卸载）
- 任务管理器 / 内存使用 / CPU 使用率监控
- 日历 / 时间 / 计时器 / 秒表 / 闹钟
- 计算器 / 背单词 / 像素画 / 每日抽签
- 拼音输入法
- 控制中心 / 通知中心
- Letter Shell 命令行终端 (CMD)
- 锁屏桌面小组件，左右滑动切换页面
- 开机动画

---

## 代码架构

固件分为两部分：

1. **Bootloader** (Flash 前 64 KB)
   - 基于 CherryUSB UF2 协议 — 拖拽式 USB 固件更新
   - 最小化 RTOS + 内存分配器 (FreeRTOS, TLSF)

2. **主程序**
   - **RTOS:** FreeRTOS (Cortex-M4F 移植)
   - **GUI:** LVGL v8.2
   - **内存:** TLSF 分配器 (128 KB SRAM + 64 KB CCM)
   - **文件系统:** FatFS (TF 卡)，FlashDB (KVDB + TSDB，片上 Flash)
   - **USB 协议栈:** CherryUSB (Device + Host)
   - **音频解码:** libhelix (MP3, AAC)、libfoxenflac (FLAC)、libdemac (APE)、libvorbis (OGG Vorbis) — 均含 ARM 汇编优化
   - **图像/视频:** PNGdec、Tjpgd、AnimatedGIF
   - **命令行:** Letter Shell (嵌入式交互式 Shell)
   - **单词库:** english-wordlists (TXT 格式词书)
   - **字体:** 文泉驿点阵、ChillBitmap、fusion-pixel
   - **看门狗:** IWDG (独立看门狗，死机自动恢复)
   - **随机数与 RTC:** 硬件 RNG，农历转换

外设尽可能使用 DMA 驱动（SPI 屏幕 @ 42 MHz、SDIO、I2S 音频、ADC 摇杆采样）。系统使用 STM32 标准外设库，Keil MDK + Arm Compiler 5 编译。

---

## 项目结构

```text
MuseCube/                          基于 STM32F405RGT6 的手持多媒体播放器固件
                                   (Keil MDK + AC5 + STM32 标准外设库)
│
├── 1-library/                         STM32F4xx 标准外设库 — 芯片级 HAL 驱动 (.c/.h)
│
├── 2-start/                            CMSIS 启动层：启动汇编、系统时钟初始化 (168 MHz)、
│                                       Cortex-M4 核心访问头文件、CMSIS-DSP
│
├── 3-main/                             应用入口层：main()、FreeRTOS 调度器启动、
│                                       全局变量 (variables.c)、任务配置宏 (defines.h)、
│                                       中断处理 (stm32f4xx_it.c)
│
├── 4-ware/                             固件核心代码 (21 个子目录)
│   ├── BSP/                            板级支持包：ADC、ES9018K2M DAC、I2S、SDIO SD 卡、
│   │                                   W25Q128 SPI Flash、LCD/背光、按键、RTC、IWDG、RNG
│   ├── Lib_FreeRTOS/                   FreeRTOS 实时操作系统内核 (Cortex-M4F 移植)
│   ├── Lib_LVGL/                       LVGL v8.2 嵌入式图形库
│   ├── Lib_Cherry_USB/                 CherryUSB 协议栈 (Device + Host)
│   ├── Lib_Fatfs/                      FatFs FAT 文件系统 (挂载 SD 卡和 USB 存储)
│   ├── Lib_FlashDB/                    FlashDB 嵌入式数据库：KVDB + TSDB，基于 FAL 抽象层
│   ├── Lib_TLSF/                       TLSF O(1) 动态内存分配器
│   ├── Lib_Music_lib/                  音频解码库：Helix MP3/AAC、foxen-flac FLAC、DEMAC APE、
│   │                                   libvorbis OGG Vorbis
│   ├── Lib_Tjpgd/                      Tiny JPEG Decompressor — 超轻量 JPEG 解码
│   ├── Lib_PNGdec/                     PNG 解码库 (含 zlib inflate)
│   ├── Lib_AnimatedGIF/                动态 GIF 解码库
│   ├── Lib_Letter_Shell/              Letter Shell — 嵌入式命令行交互外壳
│   ├── Apps_Music/                     音乐播放器应用层：播放状态机、I2S DMA 双缓冲
│   ├── Apps_Media/                     媒体解码应用层：BMP/PNG/GIF/JPG/MJPEG 解码封装
│   ├── Apps_NES/                       NES 模拟器：6502 CPU、PPU 图形、APU 音频、90+ Mapper
│   ├── Apps_Text/                      文本应用层：TXT 阅读器、LRC 歌词解析、单词管理器
│   ├── GUI_Page/                       全屏页面 (29+)：桌面、音乐、文件管理、设置、DAC、
│   │                                   日历、时钟、计算器、画板、NES 游戏、CMD 终端等
│   ├── GUI_Unit/                       可复用 GUI 组件 (22 组)：状态栏、导航栏、控制中心、
│   │                                   键盘、各页面专属组件
│   ├── GUI_Icons/                      GUI 图标资源：应用图标、Logo、背景图 (LVGL 位图格式)
│   ├── Tasks/                          FreeRTOS 任务实现：TaskManager + 7 个托管任务
│   └── Utils/                          工具函数：CPU 使用率、FFT 频谱、电池电量、拼音输入法、
│                                       农历转换、调试日志、FlashDB 封装
│
├── bootloader/                        USB UF2 独立引导程序 (占用 Flash 前 64 KB)
│   ├── 1-library/                     STM32F4 标准外设库子集 (DMA/Flash/GPIO/RCC/SPI/TIM/NVIC)
│   ├── 2-start/                       CMSIS 启动层 + 系统时钟初始化
│   ├── 3-main/                        引导入口：按键检测 → UF2 模式或跳转主程序
│   ├── 4-ware/                        UF2 协议引擎、Flash 烧写、LCD 进度条、按键检测、
│   │                                   TLSF (CCM)、引脚控制、SysTick 延时
│   ├── 5-freertos/                    FreeRTOS 内核 (LCD 刷新 + USB 线程)
│   └── 6-cherryusb/                   CherryUSB Device 栈：DWC2 OTG HS + MSC 大容量存储
│
├── bin2uf2/                           固件格式转换工具：Python 脚本，Keil .bin/.hex → UF2
│
└── TFcard_files/                      TF 卡系统资源文件 (音乐、图片、字体、游戏 ROM 等)
```

---

## 内存布局

```
Bootloader:  0x08000000 - 0x0800FFFF  (64 KB, scatter: bootloader/Objects/project.sct)
主程序:      0x08010000 - 0x080FFFFF  (960 KB, scatter: Objects/project.sct)
SRAM:        0x20000000 - 0x2001FFFF  (128 KB, 共享)
CCM:         0x10000000               (64 KB, 由 TLSF 管理)
```

主程序在 `main()` 中重定位向量表：`SCB->VTOR = 0x08010000`。Bootloader 跳转前关闭全局中断，主程序启动后立即重新开启。

---

## 编译

- **IDE:** Keil MDK 5
- **编译器:** Arm Compiler 5 (armcc)
- **依赖:** 所有第三方库均已包含在仓库中，无需额外安装。打开项目文件，分别编译 Bootloader 和 App 目标即可。

## 烧录

1. 使用 ST-Link（或任意 SWD 烧录器）烧录 **bootloader**。
2. 按住顶部按键开机 → 设备进入 UF2 模式。
3. 将生成的 `.uf2` 固件文件拖入出现的虚拟 U 盘即可。

---

## 许可证

| 组件 | 许可证 |
|------|--------|
| **硬件** (PCB、外壳、面板) | [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) |
| **作者原创软件代码** | MIT |
| **固件整体** (包含第三方库) | [GPL v3](https://www.gnu.org/licenses/gpl-3.0.html) |

| 第三方库 | 许可证 |
|----------|--------|
| FreeRTOS | MIT |
| LVGL | MIT |
| TLSF | 3-Clause BSD |
| FatFS | 1-Clause BSD |
| CherryUSB | Apache-2.0 |
| FlashDB | Apache-2.0 |
| libhelix-mp3 / aac | RPSL 1.0 |
| libfoxenflac | LGPL-2.1 |
| libdemac | GPLv2+ |
| libvorbis | 3-Clause BSD |
| PNGdec / AnimatedGIF | Apache-2.0 |
| Tjpgd | Custom (见源码) |
| Letter Shell | MIT |
| english-wordlists | — |
| 字体 (文泉驿 / ChillBitmap / fusion-pixel) | GPL-2.0 / OFL-1.1 / MIT |

壁纸经画师 [银锘](https://space.bilibili.com/1704688254) 授权使用（仅限非商业用途）。

---

## 致谢

- 感谢所有朋友和支持者
- 教程和参考来自 江协科技、正点原子，以及无数开源贡献者
- 特别感谢 [CherryUSB](https://github.com/cherry-embedded/CherryUSB) — 其优秀的可移植性让 USB 功能开发变得便利
- AI 工具加速了开发

---

**祝您使用 Muse Cube 愉快！**

有问题请联系作者 QQ：3793000877，或在 GitHub 提交 Issue。
