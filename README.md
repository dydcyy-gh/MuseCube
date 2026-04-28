# Muse Cube V1.0

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

## Key features (firmware V1.0)
- **Home screen** with lock screen and desktop widgets  
- **Music player** – WAV, MP3, FLAC, AAC, APE  
- **Video player** – MJPEG (240×240 @ 30 fps), RAW RGB565  
- **Image viewer** – JPG, PNG, BMP, GIF (including animated)  
- **NES emulator** – play classic ROMs from TF card  
- **USB gadget** – CDC serial, MSC, UAC1/UAC2 audio, HID mouse/keyboard/gamepad, Display (screen mirroring)  
- **USB host** – read USB drives, connect serial devices, HID keyboard/mouse  
- **File manager**, system log, task manager  
- **Notebook**, TXT reader, pixel‑art drawing  
- **Tools** – calculator, serial assistant, stopwatch, Pomodoro timer, alarm, countdown  
- **Study helpers** – vocabulary memorizer, timetable, lottery drawing  
- **Pinyin IME** for text input  
- **Control centre**, notification centre, Easter eggs  
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
   - **Fonts:** WenQuanYi bitmap, ChillBitmap, fusion‑pixel  
   - **Watchdog:** Independent watchdog (IWDG) for crash recovery  
   - **Random numbers & RTC:** hardware RNG, lunar calendar conversion  

Peripherals are driven with DMA wherever possible (SPI display @ 42 MHz, SDIO, I2S audio, ADC for joysticks). The system uses STM32 Standard Peripheral Library and compiles with Arm Compiler 5 in Keil MDK.

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
| Fonts | GPL‑2.0 / OFL‑1.1 / MIT |

Wallpapers are used with permission from the artist [银锘](https://space.bilibili.com/1704688254) (non‑commercial only).

## Acknowledgements
- Thanks to all friends and supporters  
- Tutorials and references from 江协科技, 正点原子, and the countless open‑source contributors  
- AI tools (GitHub Copilot) that sped up development  

---

**Enjoy building and hacking your Muse Cube!**  
For questions, contact the author via QQ: 3793000877, or open an issue on GitHub.
