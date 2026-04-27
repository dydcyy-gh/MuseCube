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
#include "debug.h"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_buffer[128];

static volatile uint8_t g_usbh_hid_connected = 0;

// 供 LVGL 调用的鼠标全局状态
volatile int16_t g_usb_mouse_dx = 0;
volatile int16_t g_usb_mouse_dy = 0;
volatile uint8_t g_usb_mouse_btn = 0;

/* USB 接收中断回调函数 */
void usbh_hid_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (!g_usbh_hid_connected) return;

    if (nbytes > 0) 
    {    
        if (nbytes == 7) 
        {
            g_usb_mouse_btn = hid_buffer[1];
            // 12位拆包
            int16_t dx = hid_buffer[2] | ((hid_buffer[3] & 0x0F) << 8);
            if (dx & 0x0800) dx |= 0xF000; 
            int16_t dy = ((hid_buffer[3] & 0xF0) >> 4) | (hid_buffer[4] << 4);
            if (dy & 0x0800) dy |= 0xF000; 
            
            // 如果方向反了加负号
            g_usb_mouse_dx += dx / 2;
            g_usb_mouse_dy += dy / 2;
        } 
        else if (nbytes == 5) 
        {
            g_usb_mouse_btn = hid_buffer[1];
            // 纯 8 位解析 (不需要除以 5)
            int8_t dx = (int8_t)hid_buffer[2];
            int8_t dy = (int8_t)hid_buffer[3];
            
            // 如果方向反了加负号
            g_usb_mouse_dx += dx;
            g_usb_mouse_dy += dy;
        } 
        else if (nbytes == 3) 
        {
            g_usb_mouse_btn = hid_buffer[0];
            int8_t dx = (int8_t)hid_buffer[1];
            int8_t dy = (int8_t)hid_buffer[2];
            
            g_usb_mouse_dx += dx;
            g_usb_mouse_dy += dy;
        }

        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, 
                          hid_buffer, hid_class->intin->wMaxPacketSize, 0, 
                          usbh_hid_callback, hid_class);
        usbh_submit_urb(&hid_class->intin_urb);
    } 
    else if (nbytes == -USB_ERR_NAK) 
    {
        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, 
                          hid_buffer, hid_class->intin->wMaxPacketSize, 0, 
                          usbh_hid_callback, hid_class);
        usbh_submit_urb(&hid_class->intin_urb);
    }
}

/* 当鼠标枚举成功后，底层会调用 run */
void usbh_hid_run(struct usbh_hid *hid_class)
{
    g_usbh_hid_connected = 1;
    // 不再创建线程，而是直接提交第一次中断传输 URB，后续在 callback 中循环提交
    usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, hid_buffer, hid_class->intin->wMaxPacketSize, 0, usbh_hid_callback, hid_class);
    usbh_submit_urb(&hid_class->intin_urb);
}

/* 当鼠标拔出时，底层会调用 stop */
void usbh_hid_stop(struct usbh_hid *hid_class)
{
    g_usbh_hid_connected = 0;
}

/* ================= 提供给 usb_task 的统一接口 ================= */

// 1. 初始化
void usbh_hid_init(uint8_t busid, uint32_t reg_base)
{
    g_usbh_hid_connected = 0;
    g_usb_mouse_dx = 0;
    g_usb_mouse_dy = 0;
    g_usb_mouse_btn = 0;
    usbh_initialize(busid, reg_base, NULL);
}

// 2. 反初始化
void usbh_hid_deinit(void)
{
    usbh_deinitialize(0);
    g_usbh_hid_connected = 0;
}

// 3. 业务任务
void usbh_hid_task(void)
{
    // 所以这里的 Task 只需要空跑即可，数据会被 LVGL 的 timer 自动取走
	Delay_ms(20);
}
