/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "systick_conf.h"
#include "malloc.h"
#include "string.h"
#include "variables.h"

#define MAX_HID_INTERFACES 4
static uint8_t *hid_buffer[MAX_HID_INTERFACES] = {NULL};

static volatile uint8_t g_usbh_hid_connected = 0;

/* 死区过滤宏：滤除回弹不到位引起的漂移 */
#define DEADZONE(val) ( (val > 20 || val < -20) ? val : 0 )

/* USB 接收中断回调函数 */
void usbh_hid_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;
    uint8_t id = hid_class->minor;
    
    if (id >= MAX_HID_INTERFACES || hid_buffer[id] == NULL) return;

    if (!g_usbh_hid_connected) return;

    if (nbytes > 0) 
    {    
        uint8_t *buf = hid_buffer[id];

        if (hid_class->protocol == 1) // 1 是键盘
        {
            static uint8_t last_kbd_key = 0;
            if (nbytes >= 8) 
            {
                uint8_t mod = buf[0];
                uint8_t key = buf[2]; 
                
                if (key == 0x00 || key >= 0x04) {
                    if (key != last_kbd_key) {
                        if (key != 0x00) { 
                            g_host_kbd_mod = mod;
                            g_host_kbd_key = key;
                            g_host_kbd_trigger = 1;
                        }
                        last_kbd_key = key;
                    }
                }
            }
        }
        else if (hid_class->protocol == 2) // 2 是鼠标
        {
            if (nbytes >= 3) 
            {
                g_usb_mouse_btn = buf[0];
                int8_t dx = (int8_t)buf[1];
                int8_t dy = (int8_t)buf[2];
                
                g_usb_mouse_dx += dx;
                g_usb_mouse_dy += dy;
            }
        }
        else if (hid_class->protocol == 0) // 0 是手柄或自定义HID
        {
            int16_t lx = 0, ly = 0, rx = 0, ry = 0;

            // 1. Xbox One / Series 系列 (蓝牙模式或标准HID)
            // Report ID = 0x01，16位坐标
            if (buf[0] == 0x01 && nbytes >= 15) {
                uint16_t raw_lx = buf[1] | (buf[2] << 8);
                uint16_t raw_ly = buf[3] | (buf[4] << 8);
                uint16_t raw_rx = buf[5] | (buf[6] << 8);
                uint16_t raw_ry = buf[7] | (buf[8] << 8);

                lx = (int16_t)(raw_lx / 256) - 128; // 向右为正
                ly = 128 - (int16_t)(raw_ly / 256); // Xbox 的 Y 下是最大值，用 128 去减变成向上为正
                rx = (int16_t)(raw_rx / 256) - 128;
                ry = 128 - (int16_t)(raw_ry / 256);
            }
            // 2. Switch Pro Controller (原生高精度模式)
            // Report ID = 0x30，12位错位打包压缩格式！
            else if (buf[0] == 0x30 && nbytes >= 12) {
                uint16_t raw_lx = buf[6] | ((buf[7] & 0x0F) << 8);
                uint16_t raw_ly = ((buf[7] & 0xF0) >> 4) | (buf[8] << 4);
                uint16_t raw_rx = buf[9] | ((buf[10] & 0x0F) << 8);
                uint16_t raw_ry = ((buf[10] & 0xF0) >> 4) | (buf[11] << 4);
                
                lx = (int16_t)(raw_lx / 16) - 128; 
                ly = (int16_t)(raw_ly / 16) - 128; // Switch 的 Y 天然就是向上为最大值(4095)，所以直接减
                rx = (int16_t)(raw_rx / 16) - 128;
                ry = (int16_t)(raw_ry / 16) - 128;
            }
            // 3. Switch Pro (精简HID模式) 或通用手柄
            // Report ID = 0x3F，普通 8 位坐标
            else if (buf[0] == 0x3F && nbytes >= 8) {
                lx = buf[4] - 128;
                ly = 128 - buf[5];
                rx = buf[6] - 128;
                ry = 128 - buf[7];
            }
            // 4. 最基础的 DirectInput PC 杂牌手柄 (无 ID 或 ID=0x01,0x03)
            else if (nbytes >= 5) {
                int offset = (buf[0] == 0x01 || buf[0] == 0x03) ? 1 : 0;
                lx = buf[offset + 0] - 128;
                ly = 128 - buf[offset + 1];
                // 右摇杆通常紧接其后或者隔一个位置
                rx = buf[offset + 2] - 128;
                ry = 128 - buf[offset + 3];
            }

            g_usb_joy_L_X = DEADZONE(lx);
            g_usb_joy_L_Y = DEADZONE(ly);
            g_usb_joy_R_X = DEADZONE(rx);
            g_usb_joy_R_Y = DEADZONE(ry);
        }

        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, 
                          hid_buffer[id], hid_class->intin->wMaxPacketSize, 0, 
                          usbh_hid_callback, hid_class);
        usbh_submit_urb(&hid_class->intin_urb);
    } 
    else if (nbytes == -USB_ERR_NAK || nbytes < 0)
    {
        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
                          hid_buffer[id], hid_class->intin->wMaxPacketSize, 0,
                          usbh_hid_callback, hid_class);
        usbh_submit_urb(&hid_class->intin_urb);
    }
}

/* 当设备枚举成功后，底层会调用 run */
void usbh_hid_run(struct usbh_hid *hid_class)
{
    g_usbh_hid_connected = 1;
    uint8_t id = hid_class->minor;
    if (id >= MAX_HID_INTERFACES) return;

    if (hid_buffer[id] == NULL) {
        hid_buffer[id] = (uint8_t *)malloc_bsc(128);
        if (hid_buffer[id] == NULL) {
            return; 
        }
        memset(hid_buffer[id], 0, 128);
    }

    // 只有键盘和鼠标支持降级为 Boot 协议，游戏手柄(0)不支持也不能发此命令
    if (hid_class->protocol == 1 || hid_class->protocol == 2) {
        usbh_hid_set_protocol(hid_class, HID_PROTOCOL_BOOT);
        if (hid_class->protocol == 1) {
            usbh_hid_set_idle(hid_class, 0, 0); 
        }
    }

    usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, 
                      hid_buffer[id], hid_class->intin->wMaxPacketSize, 0, 
                      usbh_hid_callback, hid_class);
    usbh_submit_urb(&hid_class->intin_urb);
}

void usbh_hid_stop(struct usbh_hid *hid_class)
{
    uint8_t id = hid_class->minor;
    if (id < MAX_HID_INTERFACES && hid_buffer[id] != NULL) {
        free_bsc(hid_buffer[id]);
        hid_buffer[id] = NULL;
    }
}

/* ================= 提供给 usb_task 的统一接口 ================= */

void usbh_hid_init(uint8_t busid, uint32_t reg_base)
{
    g_usbh_hid_connected = 0;
    g_usb_mouse_dx = 0;
    g_usb_mouse_dy = 0;
    g_usb_mouse_btn = 0;
    
    g_usb_joy_L_X = 0; g_usb_joy_L_Y = 0;
    g_usb_joy_R_X = 0; g_usb_joy_R_Y = 0;

    usbh_initialize(busid, reg_base, NULL);
}

void usbh_hid_deinit(void)
{
    usbh_deinitialize(0);
    g_usbh_hid_connected = 0;
    
    for (int i = 0; i < MAX_HID_INTERFACES; i++) {
        if (hid_buffer[i] != NULL) {
            free_bsc(hid_buffer[i]);
            hid_buffer[i] = NULL;
        }
    }
}

void usbh_hid_task(void)
{
    Delay_ms(20);
}
