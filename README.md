[![中文版](https://img.shields.io/badge/README-中文版-red)](README_CN.md)

# Muse Cube

**Fully open-source, multi-function handheld terminal.**

Hardware and software are open, hackable, and designed to fit in your pocket.

[![Hardware](https://img.shields.io/badge/Hardware-OSHWHub-green)](https://oshwhub.com/dydcyy/mp3-based-on-stm32)
[![Software](https://img.shields.io/badge/Software-GitHub-black)](https://github.com/dydcyy-gh/MuseCube)

---

## Overview

Muse Cube is a tiny (8 × 5 × 2.2 cm) handheld device built around an STM32F405RGT6. It combines a high-fidelity music player, video player, NES game emulator, USB gadget/host, desktop assistant, and many creative tools into one pocket-friendly gadget. Everything – PCB, 3D-printed shell, firmware – is open source.

### Hardware Highlights

| Module | Specification |
|--------|---------------|
| **MCU** | STM32F405RGT6 @ 168 MHz (Cortex-M4F), 128 KB SRAM + 64 KB CCM, 1 MB Flash |
| **Display** | 1.54″ IPS, 240×240, vibrant colors |
| **Audio** | ES9018K2M DAC + dedicated headphone amplifier (Hi-Fi 3.5 mm output); dual speakers driven by MAX98357A |
| **Storage** | 16 MB on-board SPI Flash (W25Q128) + TF card slot (up to 32 GB, SDIO 24 MHz DMA) |
| **Input** | Two Switch-style joysticks |
| **Connectivity** | USB-C (charge / data / OTG host), 3.5 mm headphone jack |
| **Battery** | 2000 mAh Li-Po, all-day music or desktop use |
| **Form Factor** | 8 × 5 × 2.2 cm, pocketable, held together with M1.6/M2 screws and heat-set inserts |

---

## Features

### 🎵 Music Player

High-fidelity playback via dual quality speakers and Hi-Fi 3.5 mm output. All ES9018K2M DAC chip parameters are user-configurable.

| Format | Max Specification |
|--------|-------------------|
| WAV | 32 bit / 96 kHz |
| MP3 | 16 bit / 48 kHz |
| FLAC | 24 bit / 96 kHz |
| AAC | 16 bit / 96 kHz |
| OGG | 16 bit / 32 kHz |
| APE | 16 bit / 48 kHz |
| AIFF | 32 bit / 96 kHz |

### 🔌 USB (Type-C, CherryUSB 1.60, USB FS DMA, automatic host/device role detection)

**USB Device Mode:**

| Class | Description |
|-------|-------------|
| **UF2** | CherryUSB UF2 — drag-and-drop firmware update, no third-party software required |
| **UAC Device** | UAC 2.0 up to 32 bit / 96 kHz (multiple sample rates); UAC 1.0 fixed 16 bit / 48 kHz (compatibility), both with feedback mode |
| **CDC Device** | Virtual serial port for log output / Letter Shell debugging |
| **Display** | Windows USB secondary display (driver required), ~10 fps @ 240×240 |
| **HID Device** | Mouse / keyboard / Xbox gamepad emulation |
| **MSC Device** | USB mass storage — access SD card files as a virtual USB drive |

**USB Host Mode:**

| Class | Description |
|-------|-------------|
| **HID Host** | Connect external keyboard / mouse / gamepad |
| **MSC Host** | File browse / copy / paste / delete between USB drives or USB ↔ SD card |
| **Serial Host** | (Experimental) Serial terminal host |

### 🖼️ Media

| Type | Format | Notes |
|------|--------|-------|
| **Image** | BMP | Any size |
| | PNG | Width < 319 px |
| | GIF | Width < 480 px (including animated) |
| | JPG | Any size |
| **Video** | MJPEG | ~30 fps @ 240×240 @ Q3 |
| | RAW | ~25 fps @ 240×240 (RGB565) |
| | AVI | ~30 fps @ 240×240 @ Q3 |
| **Text** | .c / .h / .txt / .py etc. | — |
| **Game** | NES | Most NES ROMs supported |

### 🛠️ System Tools

- Persistent settings (FlashDB KV storage, survives power loss)
- File manager (SD card hot-plug auto mount / unmount)
- Task manager / memory usage / CPU usage monitor
- Calendar / clock / timer / stopwatch / alarm
- Calculator / vocabulary memorizer / pixel-art drawing / daily lottery
- Pinyin IME for text input
- Control center / notification center
- Letter Shell command-line terminal (CMD)
- Lock screen with desktop widgets, left/right swipe to switch pages
- Boot animation

---

## Code Architecture

The firmware is split into two parts:

1. **Bootloader** (first 64 KB of Flash)
   - Based on CherryUSB UF2 protocol — drag-and-drop firmware update over USB
   - Minimal RTOS + memory allocator (FreeRTOS, TLSF)

2. **Main Application**
   - **RTOS:** FreeRTOS (Cortex-M4F port)
   - **GUI:** LVGL v8.2
   - **Memory:** TLSF allocator (128 KB SRAM + 64 KB CCM)
   - **File Systems:** FatFS (TF card), FlashDB (KVDB + TSDB on on-chip flash)
   - **USB Stack:** CherryUSB (device + host)
   - **Audio Decoders:** libhelix (MP3, AAC), libfoxenflac (FLAC), libdemac (APE), libvorbis (OGG Vorbis) — all with ARM assembly optimizations
   - **Image/Video:** PNGdec, Tjpgd, AnimatedGIF
   - **Shell:** Letter Shell (embedded command-line interactive shell)
   - **Vocabulary:** english-wordlists (TXT-format word books)
   - **Fonts:** WenQuanYi bitmap, ChillBitmap, fusion-pixel
   - **Watchdog:** IWDG (independent watchdog for crash recovery)
   - **RNG & RTC:** Hardware RNG, lunar calendar conversion

Peripherals are driven with DMA wherever possible (SPI display @ 42 MHz, SDIO, I2S audio, ADC for joysticks). The system uses STM32 Standard Peripheral Library and compiles with Arm Compiler 5 in Keil MDK.

---

## Project Structure

```text
MuseCube/                          Handheld multimedia player firmware based on STM32F405RGT6
                                   (Keil MDK + AC5 + STM32 Standard Peripheral Library)
│
├── 1-library/                         STM32F4xx StdPeriph Driver — chip-level HAL (.c/.h pairs)
│
├── 2-start/                            CMSIS startup: startup assembly, system clock init (168 MHz),
│                                       Cortex-M4 core headers, CMSIS-DSP
│
├── 3-main/                             Application entry: main(), FreeRTOS scheduler startup,
│                                       global variables (variables.c), task config macros (defines.h),
│                                       interrupt handlers (stm32f4xx_it.c)
│
├── 4-ware/                             Firmware core (21 subdirectories)
│   ├── BSP/                            Board Support Package: ADC, ES9018K2M DAC, I2S, SDIO SD card,
│   │                                   W25Q128 SPI flash, LCD/backlight, keypad, RTC, IWDG, RNG
│   ├── Lib_FreeRTOS/                   FreeRTOS kernel (ARM Cortex-M4F port)
│   ├── Lib_LVGL/                       LVGL v8.2 embedded graphics library
│   ├── Lib_Cherry_USB/                 CherryUSB protocol stack (Device + Host)
│   ├── Lib_Fatfs/                      FatFs FAT file system (SD card + USB storage)
│   ├── Lib_FlashDB/                    FlashDB: KVDB + TSDB on FAL flash abstraction layer
│   ├── Lib_TLSF/                       TLSF O(1) dynamic memory allocator
│   ├── Lib_Music_lib/                  Audio decoders: Helix MP3/AAC, foxen-flac FLAC, DEMAC APE,
│   │                                   libvorbis OGG Vorbis
│   ├── Lib_Tjpgd/                      Tiny JPEG Decompressor
│   ├── Lib_PNGdec/                     PNG decoder (with zlib inflate)
│   ├── Lib_AnimatedGIF/                Animated GIF decoder
│   ├── Lib_Letter_Shell/              Letter Shell — embedded CLI
│   ├── Apps_Music/                     Music player state machine, I2S DMA double-buffering
│   ├── Apps_Media/                     Image/video decode wrappers (BMP/PNG/GIF/JPG/MJPEG)
│   ├── Apps_NES/                       NES emulator: 6502 CPU, PPU, APU, 90+ mappers
│   ├── Apps_Text/                      TXT reader, LRC lyrics parser, vocabulary manager
│   ├── GUI_Page/                       Full-screen pages (29+): desktop, music, file manager,
│   │                                   settings, DAC, calendar, clock, calculator, NES, CMD, etc.
│   ├── GUI_Unit/                       Reusable widgets (22 groups): status bar, nav bar,
│   │                                   control center, keyboard, per-page widgets
│   ├── GUI_Icons/                      Icon & image bitmaps as LVGL-compatible C arrays
│   ├── Tasks/                          FreeRTOS tasks: TaskManager + 7 managed tasks
│   └── Utils/                          CPU usage, FFT spectrum, battery, Pinyin IME,
│                                       lunar calendar, debug logging, FlashDB wrappers
│
├── bootloader/                        Standalone USB UF2 bootloader (occupies first 64 KB of flash)
│   ├── 1-library/                     StdPeriph subset (DMA/Flash/GPIO/RCC/SPI/TIM/NVIC)
│   ├── 2-start/                       CMSIS startup + system clock init
│   ├── 3-main/                        Bootloader entry: key detection → UF2 mode or jump to app
│   ├── 4-ware/                        UF2 protocol engine, flash programmer, LCD progress bar,
│   │                                   key detection, TLSF (CCM), pin control, SysTick delay
│   ├── 5-freertos/                    FreeRTOS kernel for LCD polling + USB threads
│   └── 6-cherryusb/                   CherryUSB Device stack: DWC2 OTG HS + MSC class
│
├── bin2uf2/                           Python script: convert Keil .bin/.hex → UF2 format
│
└── TFcard_files/                      TF card resource files (music, images, fonts, ROMs, etc.)
```

---

## Memory Layout

```
Bootloader:  0x08000000 - 0x0800FFFF  (64 KB, scatter: bootloader/Objects/project.sct)
Main App:    0x08010000 - 0x080FFFFF  (960 KB, scatter: Objects/project.sct)
SRAM:        0x20000000 - 0x2001FFFF  (128 KB, shared)
CCM:         0x10000000               (64 KB, managed by TLSF)
```

The main app relocates the vector table in `main()`: `SCB->VTOR = 0x08010000`. The bootloader disables global interrupts before jumping; the app re-enables them immediately.

---

## Building

- **IDE:** Keil MDK 5
- **Compiler:** Arm Compiler 5 (armcc)
- **Dependencies:** All third-party libraries are included in the repository. Open the project file and build the `Bootloader` and `App` targets.

## Flashing

1. Flash the **bootloader** with an ST-Link (or any SWD programmer).
2. Power on while holding the top button — the device enters UF2 mode.
3. Copy the generated `.uf2` firmware file to the virtual USB drive that appears.

---

## License

| Component | License |
|-----------|---------|
| **Hardware** (PCB, case, panel) | [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) |
| **My software code** | MIT |
| **Firmware as a whole** (includes third-party libraries) | [GPL v3](https://www.gnu.org/licenses/gpl-3.0.html) |

| Third-party Library | License |
|---------------------|---------|
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
| Tjpgd | Custom (see source) |
| Letter Shell | MIT |
| english-wordlists | — |
| Fonts (WenQuanYi / ChillBitmap / fusion-pixel) | GPL-2.0 / OFL-1.1 / MIT |

Wallpapers are used with permission from the artist [银锘](https://space.bilibili.com/1704688254) (non-commercial only).

---

## Acknowledgements

- Thanks to all friends and supporters
- Tutorials and references from 江协科技, 正点原子, and countless open-source contributors
- Special thanks to [CherryUSB](https://github.com/cherry-embedded/CherryUSB) — its excellent portability made USB feature development a breeze
- AI tools that sped up development

---

**Happy building with Muse Cube!**

For questions, contact the author via QQ: 3793000877, or open an issue on GitHub.
