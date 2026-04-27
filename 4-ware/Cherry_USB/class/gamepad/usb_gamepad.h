/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_GAMEPAD_H
#define USB_GAMEPAD_H

#include "usb_hid.h"

/*
 *  GAMEPAD BUTTON LAYOUT
 *
 *            ____________________________              __
 *           / [__L2__]          [__R2__] \               |
 *          / [__ L1 __]        [__ R1 __] \              | Triggers
 *       __/________________________________\__         __|
 *      /                                  _   \          |
 *     /      /\           __             (B4)  \         |
 *    /       ||      __  |A1|  __     _       _ \        | Main Pad
 *   |    <===DP===> |S1|      |S2|  (B3) -|- (B2)|       |
 *    \       ||      ¯¯        ¯¯       _       /        |
 *    /\      \/   /   \        /   \    (B1)   /\       __|
 *   /  \________ | LS  | ____ | RS  | _______/  \        |
 *  |         /  \ \___/ /    \ \___/ /  \         |      | Sticks
 *  |        /    \_____/      \_____/    \        |    __|
 *  |       /       L3            R3       \       |
 *   \_____/                                \_____/
 *
 *     |________|______|    |______|___________|
 *       D-Pad    Left       Right    Face
 *               Stick      Stick    Buttons
 *
 *  Extended: A2=Touchpad/Capture  A3=Mute  L4/R4=Paddles
 */

// W3C Gamepad API standard button order
#define USB_GAMEPAD_BUTTON_B1 (1 << 0) // A 
#define USB_GAMEPAD_BUTTON_B2 (1 << 1) // B 
#define USB_GAMEPAD_BUTTON_B3 (1 << 2) // X 
#define USB_GAMEPAD_BUTTON_B4 (1 << 3) // Y 

#define USB_GAMEPAD_BUTTON_L1 (1 << 4) // LB 
#define USB_GAMEPAD_BUTTON_R1 (1 << 5) // RB 
#define USB_GAMEPAD_BUTTON_L2 (1 << 6) // LT 
#define USB_GAMEPAD_BUTTON_R2 (1 << 7) // RT 

#define USB_GAMEPAD_BUTTON_S1 (1 << 8) // Back 
#define USB_GAMEPAD_BUTTON_S2 (1 << 9) // Start

#define USB_GAMEPAD_BUTTON_L3 (1 << 10) // LS 
#define USB_GAMEPAD_BUTTON_R3 (1 << 11) // RS 

#define USB_GAMEPAD_BUTTON_DU (1 << 12) // D-Up 
#define USB_GAMEPAD_BUTTON_DD (1 << 13) // D-Down
#define USB_GAMEPAD_BUTTON_DL (1 << 14) // D-Left
#define USB_GAMEPAD_BUTTON_DR (1 << 15) // D-Right

#define USB_GAMEPAD_BUTTON_A1 (1 << 16) // Guide 
#define USB_GAMEPAD_BUTTON_A2 (1 << 17) // - 
#define USB_GAMEPAD_BUTTON_A3 (1 << 18) // - 
#define USB_GAMEPAD_BUTTON_A4 (1 << 19) // - 

#define USB_GAMEPAD_BUTTON_L4 (1 << 20) // P1 
#define USB_GAMEPAD_BUTTON_R4 (1 << 21) // P2 

#define XINPUT_VID        0x045E // Microsoft
#define XINPUT_PID        0x028E // Xbox 360 Controller
#define XINPUT_BCD_DEVICE 0x0114 // v1.14

// XInput Interface Class/Subclass/Protocol
#define XINPUT_INTERFACE_CLASS    0xFF
#define XINPUT_INTERFACE_SUBCLASS 0x5D
#define XINPUT_INTERFACE_PROTOCOL 0x01

#define XINPUT_BUTTON_MASK_UP    (1U << 0)
#define XINPUT_BUTTON_MASK_DOWN  (1U << 1)
#define XINPUT_BUTTON_MASK_LEFT  (1U << 2)
#define XINPUT_BUTTON_MASK_RIGHT (1U << 3)
#define XINPUT_BUTTON_MASK_START (1U << 4)
#define XINPUT_BUTTON_MASK_BACK  (1U << 5)
#define XINPUT_BUTTON_MASK_L3    (1U << 6)
#define XINPUT_BUTTON_MASK_R3    (1U << 7)
#define XINPUT_BUTTON_MASK_LB    (1U << 8)
#define XINPUT_BUTTON_MASK_RB    (1U << 9)
#define XINPUT_BUTTON_MASK_GUIDE (1U << 10)
#define XINPUT_BUTTON_MASK_A     (1U << 12)
#define XINPUT_BUTTON_MASK_B     (1U << 13)
#define XINPUT_BUTTON_MASK_X     (1U << 14)
#define XINPUT_BUTTON_MASK_Y     (1U << 15)

// LED patterns for report_id 0x01
#define XINPUT_LED_OFF          0x00
#define XINPUT_LED_BLINK        0x01

struct xinput_in_report {
    uint8_t report_id;   /* Always 0x00 */
    uint8_t report_size; /* Always 0x14 (20) */
    uint16_t buttons;    /* DPAD, Start, Back, L3, R3, LB, RB, Guide, A, B, X, Y */
    uint8_t lt;          /* Left trigger (0-255) */
    uint8_t rt;          /* Right trigger (0-255) */
    int16_t lx;          /* Left stick X (-32768 to 32767) */
    int16_t ly;          /* Left stick Y (-32768 to 32767) */
    int16_t rx;          /* Right stick X (-32768 to 32767) */
    int16_t ry;          /* Right stick Y (-32768 to 32767) */
    uint8_t reserved[6]; /* Reserved/padding */
} __PACKED;

struct xinput_out_report {
    uint8_t report_id;   // 0x00 = rumble, 0x01 = LED
    uint8_t report_size; // 0x08
    uint8_t led;         // LED pattern (0x00 for rumble)
    uint8_t rumble_l;    // Left motor (large, 0-255)
    uint8_t rumble_r;    // Right motor (small, 0-255)
    uint8_t reserved[3]; // Padding
} __PACKED;

// clang-format off
#define XINPUT_DESCRIPTOR_LEN (9 + 16 + 7 + 7)

#define XINPUT_DESCRIPTOR_INIT(bInterfaceNumber, out_ep, in_ep)                                                                     \
    USB_INTERFACE_DESCRIPTOR_INIT(bInterfaceNumber, 0x00, 0x02, 0xff, 0x5d, 0x01, 0x00), /* XInput proprietary descriptor (0x21) */ \
    16, 0x21, 0x00, 0x01, 0x01, 0x24, in_ep, 0x14, 0x03, 0x00, 0x03, 0x13, out_ep, 0x00, 0x03, 0x00,                               \
    USB_ENDPOINT_DESCRIPTOR_INIT(in_ep, 0x03, 32, 0x01),                                                                        \
    USB_ENDPOINT_DESCRIPTOR_INIT(out_ep, 0x03, 32, 0x08)
// clang-format on

struct usb_gamepad_report {
    uint32_t buttons;
    uint8_t lt;
    uint8_t rt;
    int8_t lx;   // 修改为 int8_t, 范围 -128 到 +127, 完美对应修改后的摇杆
    int8_t ly;
    int8_t rx;
    int8_t ry;
};

#endif /* USB_GAMEPAD_H */
