/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_gamepad.h"
#include "usbd_hid.h"
#include "systick_conf.h"
#include "variables.h"
#include "defines.h"

// ==========================================
// 全局及通用定义
// ==========================================
#define GAMEPAD_IN_EP  0x81
#define GAMEPAD_OUT_EP 0x02

#define MOUSE_IN_EP           0x81
#define MOUSE_INT_EP_SIZE     4
#define MOUSE_INT_EP_INTERVAL 1

#define KBD_IN_EP             0x81
#define KBD_INT_EP_SIZE       8
#define KBD_INT_EP_INTERVAL   10

#define USBD_MAX_POWER 500

// --- LVGL 键盘与 USB 任务通信的全局变量 ---
volatile uint8_t g_usb_kbd_modifier = 0; // Shift, Ctrl, Alt 等修饰键
volatile uint8_t g_usb_kbd_key = 0;      // 实际的键码 (Keycode)
volatile uint8_t g_usb_kbd_trigger = 0;  // 状态机：0=空闲, 1=请求按下, 2=请求松开

// ==========================================
// 手柄描述符定义 (XInput)
// ==========================================
static const uint8_t xinput_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, XINPUT_VID, XINPUT_PID, XINPUT_BCD_DEVICE, 0x01)
};
static const uint8_t xinput_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT((9 + XINPUT_DESCRIPTOR_LEN), 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    XINPUT_DESCRIPTOR_INIT(0x00, GAMEPAD_OUT_EP, GAMEPAD_IN_EP)
};
static const char *xinput_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, "Microsoft", "XInput STANDARD GAMEPAD", "1.0",
};
static const uint8_t *gp_device_descriptor_callback(uint8_t speed) { return xinput_device_descriptor; }
static const uint8_t *gp_config_descriptor_callback(uint8_t speed) { return xinput_config_descriptor; }
static const uint8_t *gp_device_quality_descriptor_callback(uint8_t speed) { return NULL; }
static const char *gp_string_descriptor_callback(uint8_t speed, uint8_t index) {
    if (index > 3) return NULL; return xinput_string_descriptors[index];
}
const struct usb_descriptor gamepad_descriptor = {
    .device_descriptor_callback = gp_device_descriptor_callback,
    .config_descriptor_callback = gp_config_descriptor_callback,
    .device_quality_descriptor_callback = gp_device_quality_descriptor_callback,
    .string_descriptor_callback = gp_string_descriptor_callback
};

// ==========================================
// 鼠标描述符定义
// ==========================================
#define MOUSE_VID 0xffff
#define MOUSE_PID 0xffff
#define MOUSE_CONFIG_SIZE 34
#define HID_MOUSE_REPORT_DESC_SIZE 74

static const uint8_t mouse_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, MOUSE_VID, MOUSE_PID, 0x0002, 0x01)
};
static const uint8_t mouse_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(MOUSE_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    HID_MOUSE_DESCRIPTOR_INIT(0x00, 0x01, HID_MOUSE_REPORT_DESC_SIZE, MOUSE_IN_EP, MOUSE_INT_EP_SIZE, MOUSE_INT_EP_INTERVAL),
};
static const uint8_t mouse_device_quality_descriptor[] = { 0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 };
static const char *mouse_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, "CherryUSB", "CherryUSB HID Mouse", "2026123456",
};
static const uint8_t *ms_device_descriptor_callback(uint8_t speed) { return mouse_device_descriptor; }
static const uint8_t *ms_config_descriptor_callback(uint8_t speed) { return mouse_config_descriptor; }
static const uint8_t *ms_device_quality_descriptor_callback(uint8_t speed) { return mouse_device_quality_descriptor; }
static const char *ms_string_descriptor_callback(uint8_t speed, uint8_t index) {
    if (index > 3) return NULL; return mouse_string_descriptors[index];
}
const struct usb_descriptor mouse_descriptor = {
    .device_descriptor_callback = ms_device_descriptor_callback,
    .config_descriptor_callback = ms_config_descriptor_callback,
    .device_quality_descriptor_callback = ms_device_quality_descriptor_callback,
    .string_descriptor_callback = ms_string_descriptor_callback
};
static const uint8_t hid_mouse_report_desc[HID_MOUSE_REPORT_DESC_SIZE] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03,
    0x81, 0x06, 0xC0, 0x09, 0x3c, 0x05, 0xff, 0x09, 0x01, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95,
    0x02, 0xb1, 0x22, 0x75, 0x06, 0x95, 0x01, 0xb1, 0x01, 0xC0
};
struct hid_mouse { uint8_t buttons; int8_t x; int8_t y; int8_t wheel; };

// ==========================================
// 键盘描述符定义
// ==========================================
#define KBD_VID 0xffff
#define KBD_PID 0xffff
#define KBD_CONFIG_SIZE 34
#define HID_KEYBOARD_REPORT_DESC_SIZE 63

static const uint8_t kbd_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, KBD_VID, KBD_PID, 0x0002, 0x01)
};
static const uint8_t kbd_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(KBD_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    HID_KEYBOARD_DESCRIPTOR_INIT(0x00, 0x01, HID_KEYBOARD_REPORT_DESC_SIZE, KBD_IN_EP, KBD_INT_EP_SIZE, KBD_INT_EP_INTERVAL),
};
static const uint8_t kbd_device_quality_descriptor[] = { 0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 };
static const char *kbd_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, "CherryUSB", "CherryUSB HID Keyboard", "2026123456",
};
static const uint8_t *kbd_device_descriptor_callback(uint8_t speed) { return kbd_device_descriptor; }
static const uint8_t *kbd_config_descriptor_callback(uint8_t speed) { return kbd_config_descriptor; }
static const uint8_t *kbd_device_quality_descriptor_callback(uint8_t speed) { return kbd_device_quality_descriptor; }
static const char *kbd_string_descriptor_callback(uint8_t speed, uint8_t index) {
    if (index > 3) return NULL; return kbd_string_descriptors[index];
}
const struct usb_descriptor kbd_descriptor = {
    .device_descriptor_callback = kbd_device_descriptor_callback,
    .config_descriptor_callback = kbd_config_descriptor_callback,
    .device_quality_descriptor_callback = kbd_device_quality_descriptor_callback,
    .string_descriptor_callback = kbd_string_descriptor_callback
};
static const uint8_t hid_keyboard_report_desc[HID_KEYBOARD_REPORT_DESC_SIZE] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x05, 0x75, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0xFF, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0
};

// ==========================================
// 核心逻辑与事件回调
// ==========================================
uint8_t gamepad_mode = USBD_GAMEPAD_MODE_XINPUT;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t gamepad_read_buffer[64];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX struct hid_mouse g_mouse_report;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_keyboard_report[8]; // 键盘的数据缓冲

volatile uint8_t ep_state_idle = 1;

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_CONFIGURED:
            ep_state_idle = 1;
            if (g_usb_function == USBD_GMPD) {
                usbd_ep_start_read(busid, GAMEPAD_OUT_EP, gamepad_read_buffer, usbd_get_ep_mps(busid, GAMEPAD_OUT_EP));
            }
            break;
        default:
            break;
    }
}

static void usbd_ep_int_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    ep_state_idle = 1;
}

void usbd_gamepad_int_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_read(busid, GAMEPAD_OUT_EP, gamepad_read_buffer, usbd_get_ep_mps(busid, GAMEPAD_OUT_EP));
}

static struct usbd_endpoint ep_in = {
    .ep_cb = usbd_ep_int_in_callback,
    .ep_addr = 0x81 // Gamepad, Mouse, KBD IN 都可以复用 0x81
};

static struct usbd_endpoint ep_out = {
    .ep_cb = usbd_gamepad_int_out_callback,
    .ep_addr = GAMEPAD_OUT_EP
};

static struct usbd_interface intf0;

void usbd_hid_init(uint8_t busid, uintptr_t reg_base)
{
    if (g_usb_function == USBD_GMPD) {
        usbd_desc_register(busid, &gamepad_descriptor);
        usbd_add_interface(busid, usbd_gamepad_xinput_init_intf(&intf0));
        usbd_add_endpoint(busid, &ep_in);
        usbd_add_endpoint(busid, &ep_out);
    } else if (g_usb_function == USBD_MOU) {
        usbd_desc_register(busid, &mouse_descriptor);
        usbd_add_interface(busid, usbd_hid_init_intf(busid, &intf0, hid_mouse_report_desc, HID_MOUSE_REPORT_DESC_SIZE));
        usbd_add_endpoint(busid, &ep_in);
    } else if (g_usb_function == USBD_KBD) {
        usbd_desc_register(busid, &kbd_descriptor);
        usbd_add_interface(busid, usbd_hid_init_intf(busid, &intf0, hid_keyboard_report_desc, HID_KEYBOARD_REPORT_DESC_SIZE));
        usbd_add_endpoint(busid, &ep_in);
    }

    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void usbd_hid_deinit(void)
{
    usbd_ep_close(0, 0x81);
    usbd_ep_close(0, GAMEPAD_OUT_EP); 
	usbd_deinitialize(0);
}

void gamepad_change_mode(uint8_t mode, uintptr_t reg_base)
{
    gamepad_mode = USBD_GAMEPAD_MODE_XINPUT;
}

#define MOUSE_SPEED_DIVIDER 10     
#define WHEEL_DELAY_TICKS   10     

void usbd_hid_task(void)
{
    static TickType_t xLastWakeTime = 0;
    static uint8_t is_configured = 0;
    static uint8_t current_hw_mode = 0;
    static uint8_t last_wkp_state = 0;
    
    const int16_t ACTION_THRESHOLD = 64;

    if (!usb_device_is_configured(0)) {
        is_configured = 0;
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    if (!is_configured) {
        xLastWakeTime = xTaskGetTickCount();
        is_configured = 1;
    }

    if (g_usb_function == USBD_GMPD) {
        struct usb_gamepad_report report;
        
        if (g_key_WKP_RT == 1 && last_wkp_state == 0) {
            current_hw_mode = (current_hw_mode + 1) % 3;
        }
        last_wkp_state = g_key_WKP_RT;

        memset(&report, 0, sizeof(report)); 
        
        if (g_key_L_M_RT) report.buttons |= USB_GAMEPAD_BUTTON_L3;
        if (g_key_R_M_RT) report.buttons |= USB_GAMEPAD_BUTTON_R3;

        report.lx = g_key_L_X;
        report.ly = g_key_L_Y;

        switch (current_hw_mode) 
        {
            case 0: 
                report.rx = g_key_R_X;
                report.ry = g_key_R_Y;
                break;
            case 1:
                if (g_key_R_Y > ACTION_THRESHOLD)  report.buttons |= USB_GAMEPAD_BUTTON_B4; 
                else if (g_key_R_Y < -ACTION_THRESHOLD) report.buttons |= USB_GAMEPAD_BUTTON_B1; 

                if (g_key_R_X > ACTION_THRESHOLD) report.buttons |= USB_GAMEPAD_BUTTON_B2; 
                else if (g_key_R_X < -ACTION_THRESHOLD) report.buttons |= USB_GAMEPAD_BUTTON_B3; 
                break;
            case 2:
                if (g_key_R_Y > 0) report.rt = (uint8_t)(g_key_R_Y * 2);
                else if (g_key_R_Y < 0) report.lt = (uint8_t)((-g_key_R_Y) * 2);

                if (g_key_R_X > ACTION_THRESHOLD) report.buttons |= USB_GAMEPAD_BUTTON_R1;
                else if (g_key_R_X < -ACTION_THRESHOLD) report.buttons |= USB_GAMEPAD_BUTTON_L1;
                break;
        }
        
        if (ep_state_idle) {
            ep_state_idle = 0;
            usbd_gamepad_xinput_send_report(GAMEPAD_IN_EP, &report);
        }
    } 
    else if (g_usb_function == USBD_MOU) 
	{
        static uint8_t wheel_delay_cnt = 0;

        memset(&g_mouse_report, 0, sizeof(g_mouse_report));
        
        g_mouse_report.x = g_key_R_X / MOUSE_SPEED_DIVIDER;
        g_mouse_report.y = -(g_key_R_Y / MOUSE_SPEED_DIVIDER); 
        
        if (g_key_L_X > 80) g_mouse_report.buttons |= 0x01; 
        else if (g_key_L_X < -80) g_mouse_report.buttons |= 0x02; 
        
        if (g_key_L_M_RT) g_mouse_report.buttons |= 0x04; 
        
        if (g_key_L_Y > 80 || g_key_L_Y < -80) {
            if (wheel_delay_cnt == 0) {
                g_mouse_report.wheel = (g_key_L_Y > 80) ? -1 : 1;
                wheel_delay_cnt = WHEEL_DELAY_TICKS; 
            } else {
                wheel_delay_cnt--;
            }
        } else {
            wheel_delay_cnt = 0; 
        }
        
        if (ep_state_idle) {
            ep_state_idle = 0;
            usbd_ep_start_write(0, MOUSE_IN_EP, (uint8_t *)&g_mouse_report, sizeof(g_mouse_report));
        }
    }
    else if (g_usb_function == USBD_KBD)
    {
        // 键盘按键发送状态机
        if (g_usb_kbd_trigger == 1) { // 1 表示有键需要按下
            if (ep_state_idle) {
                ep_state_idle = 0;
                memset(g_keyboard_report, 0, sizeof(g_keyboard_report));
                g_keyboard_report[0] = g_usb_kbd_modifier; // Byte 0: 控制键 (Shift, Ctrl等)
                g_keyboard_report[2] = g_usb_kbd_key;      // Byte 2: 物理按键码
                usbd_ep_start_write(0, KBD_IN_EP, g_keyboard_report, 8);
                g_usb_kbd_trigger = 2; // 进入待松开状态
            }
        } 
        else if (g_usb_kbd_trigger == 2) { // 2 表示需要发送松开全零报告
            if (ep_state_idle) {
                ep_state_idle = 0;
                memset(g_keyboard_report, 0, sizeof(g_keyboard_report));
                usbd_ep_start_write(0, KBD_IN_EP, g_keyboard_report, 8);
                g_usb_kbd_trigger = 0; // 回到空闲
            }
        }
    }
    
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
}
