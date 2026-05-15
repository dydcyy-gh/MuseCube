# Muse Cube V1.1

**Fully open-source, multi-function handheld terminal.**
Hardware and software are open, hackable, and designed to fit in your pocket.

[![Hardware](https://img.shields.io/badge/Hardware-OSHWHub-green)](https://oshwhub.com/dydcyy/mp3-based-on-stm32)
[![Software](https://img.shields.io/badge/Software-GitHub-black)](https://github.com/dydcyy-gh/MuseCube)

## Overview

Muse Cube is a tiny (8 × 5 × 2.2 cm) handheld device built around an STM32F405. It combines a music player, video player, retro game emulator, USB gadget, desktop assistant, and many creative tools into one pocket‑friendly gadget. Everything – PCB, 3D‑printed shell, firmware – is open source.

### Hardware highlights
- **MCU:** STM32F405 @ 168 MHz, 128 KB SRAM + 64 KB CCM, 1 MB Flash
- **Display:** 1.54″ IPS, 240×240, vibrant colors  
- **Audio:** ES9018K2M DAC + dedicated headphone amplifier; dual speakers driven by MAX98357A  
- **Storage:** 16 MB on‑board SPI Flash (W25Q128), TF card slot (up to 32 GB, SDIO 24 MHz DMA)  
- **Input:** two Switch‑style joysticks  
- **Connectivity:** USB‑C (charge / data / host), 3.5 mm headphone jack  
- **Battery:** 2000 mAh Li‑Po, all‑day music or desktop use  
- **Form factor:** 8 × 5 × 2.2 cm, pocketable, held together with M1.6/M2 screws and heat‑set inserts  

## Key features (firmware V1.1)
- **Home screen** — lock screen with desktop widgets, left/right swipe to flip pages for more app icons  
- **Music player** — WAV, MP3, FLAC, AAC, APE; DAC advanced settings  
- **Video player** — MJPEG (240×240 @ 30 fps), RAW RGB565  
- **Image viewer** — JPG, PNG, BMP, GIF (including animated); photo album for BMP browsing  
- **NES emulator** — play classic ROMs from TF card  
- **USB gadget** — CDC serial, MSC, UAC1/UAC2 audio, HID mouse/keyboard/gamepad, Display (screen mirroring)  
- **USB host** — read USB drives, connect serial devices, HID keyboard/mouse  
- **File manager** — refined drive letter display, SD card hot-plug auto mount/unmount  
- **System log**, task manager  
- **Notebook**, TXT reader, pixel‑art drawing  
- **Tools** — calculator, serial assistant, clock/timer/stopwatch/alarm, countdown, date & time settings  
- **CMD terminal** — Letter Shell embedded command-line interface  
- **Calendar** — day view and month view  
- **Study helpers** — vocabulary memorizer (TXT word‑list parsing, word/phonetic/definition display), timetable, lottery drawing  
- **Pinyin IME** for text input  
- **Control centre**, notification centre, about device page  
- Boot animation and splash screen  

> Many more features are planned and will land in monthly firmware updates (starting May 2026).

## Code architecture

The firmware is split into two parts:

1. **Bootloader** (first 64 KB of Flash)  
   - Based on CherryUSB UF2 protocol – drag‑and‑drop firmware update over USB  
   - Minimal RTOS + memory allocator (FreeRTOS, TLSF)

2. **Main application**  
   - **RTOS:** FreeRTOS  
   - **GUI:** LVGL (v8.2)  
   - **Memory:** TLSF allocator managing 128 KB RAM + 64 KB CCM  
   - **File systems:** FatFS (TF card), FlashDB (on‑chip flash key‑value & time‑series database)  
   - **USB stack:** CherryUSB (device & host)  
   - **Audio:** libhelix‑aac, libhelix‑mp3, libfoxenflac, libdemac  
   - **Image/video:** PNGdec, Tjpgd, AnimatedGIF  
   - **Shell:** Letter Shell (embedded command-line interactive shell)
   - **Vocabulary:** english-wordlists (TXT‑format word books)
   - **Fonts:** WenQuanYi bitmap, ChillBitmap, fusion‑pixel  
   - **Watchdog:** Independent watchdog (IWDG) for crash recovery  
   - **Random numbers & RTC:** hardware RNG, lunar calendar conversion  

Peripherals are driven with DMA wherever possible (SPI display @ 42 MHz, SDIO, I2S audio, ADC for joysticks). The system uses STM32 Standard Peripheral Library and compiles with Arm Compiler 5 in Keil MDK.

## Project structure

```text
MuseCube/                          Handheld multimedia player firmware project based on STM32F405RGT6 (Keil MDK + AC5 + STM32 Standard Library)
│
├── 1-library/                         STM32F4xx Standard Peripheral Library (StdPeriph_Driver V1.8.1), HAL drivers for all on-chip peripherals (.c/.h pairs)
│
├── 2-start/                            CMSIS startup layer: startup assembly, system clock init (168 MHz), Cortex-M4 core access headers, CMSIS-DSP library
│
├── 3-main/                             Application entry layer: main() function, FreeRTOS scheduler startup, global variables, task config macros, interrupt handlers
│
├── 4-ware/                             Firmware core code (21 subdirectories covering hardware drivers → middleware → application → GUI → FreeRTOS tasks)
│   ├── BSP/                            Board support package: ADC, ES9018K2M DAC, I2S audio interface, SDIO SD card, W25Q128 SPI flash, LCD/backlight, keypad drivers
│   ├── Lib_FreeRTOS/                   FreeRTOS real-time OS kernel (ARM Cortex-M4F port)
│   ├── Lib_LVGL/                       LVGL v8.x embedded graphics library (DMA2D GPU acceleration, TLSF memory allocator, rich widgets and fonts)
│   ├── Lib_Cherry_USB/                 CherryUSB protocol stack: supports USB Device/Host (CDC/MSC/UAC1/UAC2/HID/Display/Video/ADB and more)
│   ├── Lib_Fatfs/                      FatFs FAT file system (mounts SD card and USB storage)
│   ├── Lib_FlashDB/                    FlashDB embedded database: key-value database (KVDB) + time-series database (TSDB), built on FAL flash abstraction layer
│   ├── Lib_TLSF/                       TLSF O(1) dynamic memory allocator, replaces standard malloc/free
│   ├── Lib_Music_lib/                  Audio decode library: Helix MP3/AAC decoder, foxen-flac FLAC decoder, DEMAC APE decoder (all with ARM assembly optimizations)
│   ├── Lib_Tjpgd/                      Tiny JPEG Decompressor — ultra-lightweight JPEG decode library
│   ├── Lib_PNGdec/                     PNG decode library (includes zlib inflate decompression)
│   ├── Lib_AnimatedGIF/                Animated GIF decode library
│   ├── Lib_Letter_Shell/              Letter Shell — embedded command-line interactive shell
│   ├── Apps_Music/                     Music player application layer: playback state machine, I2S DMA double-buffering, MP3/AAC/FLAC/APE/WAV format decode wrappers
│   ├── Apps_Media/                     Media decode application layer: BMP/PNG/GIF/JPG/MJPEG video decode wrappers
│   ├── Apps_NES/                       NES emulator: 6502 CPU (assembly), PPU graphics rendering, APU audio, 90+ mapper support
│   ├── Apps_Text/                      Text application layer: TXT reader, LRC lyrics parser, vocabulary manager
│   ├── GUI_Page/                       Full-screen GUI pages (29+): desktop, music, file manager, settings, DAC, calendar, clock, calculator, sketchpad, NES games, CMD terminal, photo album, etc.
│   ├── GUI_Unit/                       Reusable GUI components (22 groups): status bar, navigation bar, control center, keyboard, page-specific widgets
│   ├── GUI_Icons/                      GUI icon resources: app icons, MuseCube logo, background images as LVGL-compatible bitmap data
│   ├── Tasks/                          FreeRTOS task implementations: TaskManager + 7 tasks (Basic/LVGL/Music/Video/Game/USB)
│   └── Utils/                          Utility functions: CPU usage, FFT spectrum analysis, battery level, Pinyin input method, lunar calendar conversion, debug logging
│
├── bootloader/                        USB UF2 bootloader (standalone Keil project, occupies first 64 KB of flash, drag-and-drop firmware update)
│   ├── 1-library/                     STM32F4 Standard Peripheral Library subset (DMA/Flash/GPIO/RCC/SPI/TIM/NVIC), only the peripheral drivers needed by the bootloader
│   ├── 2-start/                       CMSIS startup layer: startup assembly, system clock init (168 MHz), Cortex-M4 core access headers
│   ├── 3-main/                        Bootloader main entry: main() key detection → enter UF2 firmware update mode or jump to main app (0x08010000); ISRs and peripheral config header
│   ├── 4-ware/                        Bootloader BSP: UF2 protocol engine (emulated FAT16), flash programming driver (protects bootloader sectors), LCD progress bar, key detection, TLSF memory allocator (CCM), pin control, SysTick delay
│   ├── 5-freertos/                    FreeRTOS kernel (Cortex-M4F port), provides multitasking for LCD UI polling task and CherryUSB EP0/MSC threads
│   └── 6-cherryusb/                   CherryUSB Device stack: DWC2 OTG HS controller driver + MSC mass storage class, enumerates as a USB drive for drag-and-drop UF2 firmware files
│
├── bin2uf2/                           Firmware format conversion tool: Python script to convert Keil .bin/.hex output to UF2 format
│
└── TFcard_files/                      System resource files to be placed on the TF card (music, pictures, fonts, game ROMs, lyrics, text files, word lists, etc.)
```

### Building
- IDE: Keil MDK 5  
- Compiler: Arm Compiler 5 (armcc)  
- Dependencies: all third‑party libraries are included in the repository under their respective licenses. Open the project file and build the `Bootloader` and `App` targets.

### Flashing
1. Flash the **bootloader** with an ST‑Link (or any SWD programmer).  
2. Power on while holding the top button – the device enters UF2 mode.  
3. Copy the generated `.uf2` firmware file to the virtual USB drive that appears.  

## License
- **Hardware** (PCB, case, panel): [CC BY‑NC‑SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)  
- **My software code**: MIT  
- **Firmware as a whole** (includes third‑party libraries): [GPL v3](https://www.gnu.org/licenses/gpl-3.0.html)  

| Library | License |
|---------|---------|
| FreeRTOS | MIT |
| LVGL | MIT |
| TLSF | 3‑Clause BSD |
| FatFS | 1‑Clause BSD |
| CherryUSB | Apache‑2.0 |
| FlashDB | Apache‑2.0 |
| libhelix‑mp3 / aac | RPSL 1.0 |
| libfoxenflac | LGPL‑2.1 |
| libdemac | GPLv2+ |
| PNGdec / AnimatedGIF | Apache‑2.0 |
| Tjpgd | Custom (see source) |
| Letter Shell | MIT |
| english‑wordlists | — |
| Fonts | GPL‑2.0 / OFL‑1.1 / MIT |

Wallpapers are used with permission from the artist [银锘](https://space.bilibili.com/1704688254) (non‑commercial only).

## Acknowledgements
- Thanks to all friends and supporters  
- Tutorials and references from 江协科技, 正点原子, and the countless open‑source contributors  
- AI tools (GitHub Copilot) that sped up development  

---

**Enjoy building and hacking your Muse Cube!**  
For questions, contact the author via QQ: 3793000877, or open an issue on GitHub.
