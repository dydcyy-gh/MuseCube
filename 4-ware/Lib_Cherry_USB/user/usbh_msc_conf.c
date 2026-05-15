/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FreeRTOS.h"
#include "task.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include "fatfs.h"
#include "ff.h"
#include "systick_conf.h"

static volatile uint8_t g_usbh_msc_connected = 0;
static uint8_t g_usbh_msc_mounted = 0; // 记录FatFs是否已经挂载

/* 当U盘枚举成功后，底层会调用 run */
void usbh_msc_run(struct usbh_msc *msc_class)
{
    g_usbh_msc_connected = 1;
}

/* 当U盘拔出时，底层会调用 stop */
void usbh_msc_stop(struct usbh_msc *msc_class)
{
    g_usbh_msc_connected = 0;
}

/* ================= 提供给 usb_task 的统一接口 ================= */

// 1. 初始化
void usbh_msc_init(uint8_t busid, uint32_t reg_base)
{
    g_usbh_msc_connected = 0;
    g_usbh_msc_mounted = 0;
    usbh_initialize(busid, reg_base, NULL);
}

// 2. 反初始化
void usbh_msc_deinit(void)
{
    // 如果已经挂载了U盘，退出前先卸载
    if (g_usbh_msc_mounted) {
        fatfs_unmount(DEV_USB);
        g_usbh_msc_mounted = 0;
    }
    usbh_deinitialize(0);
    g_usbh_msc_connected = 0;
}

// 3. 业务任务 (由 USB_Task 循环调用)
void usbh_msc_task(void)
{
    // 如果U盘已物理连接，但FatFs还没挂载，则执行挂载
    if (g_usbh_msc_connected && !g_usbh_msc_mounted) 
    {
        if (fatfs_mount(DEV_USB) == FR_OK) {
            g_usbh_msc_mounted = 1; // 挂载成功，保持状态
            USB_LOG_RAW("USB MSC Mounted to FatFs.\r\n");
        }
    }
    // 如果U盘物理拔出了，但FatFs还没卸载，则执行卸载
    else if (!g_usbh_msc_connected && g_usbh_msc_mounted)
    {
        fatfs_unmount(DEV_USB);
        g_usbh_msc_mounted = 0;
        USB_LOG_RAW("USB MSC Unmounted.\r\n");
    }
    
    Delay_ms(20);
}
