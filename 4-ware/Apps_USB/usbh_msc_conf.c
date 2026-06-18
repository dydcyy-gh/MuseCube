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
#include "debug.h"

// 从 usb_config.h 中获取最大支持的 MSC 设备数 (默认为 2)
#define MAX_MSC_DEVICES CONFIG_USBHOST_MAX_MSC_CLASS

// 将状态变量改为数组，支持独立管理多个 U 盘
static volatile uint8_t g_usbh_msc_connected[MAX_MSC_DEVICES] = {0};
static uint8_t g_usbh_msc_mounted[MAX_MSC_DEVICES] = {0}; 

/* 当U盘枚举成功后，底层会调用 run */
void usbh_msc_run(struct usbh_msc *msc_class)
{
    // 【修改这里】：MSC类的索引存储在 sdchar 中 ('a'->0, 'b'->1)
    uint8_t id = msc_class->sdchar - 'a';
    
    if (id < MAX_MSC_DEVICES) {
        g_usbh_msc_connected[id] = 1;
    }
}

/* 当U盘拔出时，底层会调用 stop */
void usbh_msc_stop(struct usbh_msc *msc_class)
{
    // 【修改这里】：同上
    uint8_t id = msc_class->sdchar - 'a';
    
    if (id < MAX_MSC_DEVICES) {
        g_usbh_msc_connected[id] = 0;
    }
}

/* ================= 提供给 usb_task 的统一接口 ================= */

// 1. 初始化
void usbh_msc_init(uint8_t busid, uint32_t reg_base)
{
    for(int i = 0; i < MAX_MSC_DEVICES; i++) {
        g_usbh_msc_connected[i] = 0;
        g_usbh_msc_mounted[i] = 0;
    }
    usbh_initialize(busid, reg_base, NULL);
}

// 2. 反初始化
void usbh_msc_deinit(void)
{
    // 如果已经挂载了U盘，退出前先卸载所有已挂载的 U 盘
    for(int i = 0; i < MAX_MSC_DEVICES; i++) {
        if (g_usbh_msc_mounted[i]) {
            // DEV_USB 宏如果定义为 1，那么 i=0 时对应 1:, i=1 时对应 2:
            fatfs_unmount(DEV_USB + i); 
            g_usbh_msc_mounted[i] = 0;
        }
    }
    usbh_deinitialize(0);
    
    for(int i = 0; i < MAX_MSC_DEVICES; i++) {
        g_usbh_msc_connected[i] = 0;
    }
}

// 3. 业务任务 (由 USB_Task 循环调用)
void usbh_msc_task(void)
{
    for(int i = 0; i < MAX_MSC_DEVICES; i++) 
    {
        // 如果U盘已物理连接，但FatFs还没挂载，则执行挂载
        if (g_usbh_msc_connected[i] && !g_usbh_msc_mounted[i]) 
        {
            if (fatfs_mount(DEV_USB + i) == 0) { // fatfs_mount 返回0代表成功
                g_usbh_msc_mounted[i] = 1; // 挂载成功，保持状态
            }
        }
        // 如果U盘物理拔出了，但FatFs还没卸载，则执行卸载
        else if (!g_usbh_msc_connected[i] && g_usbh_msc_mounted[i])
        {
            fatfs_unmount(DEV_USB + i);
            g_usbh_msc_mounted[i] = 0;
        }
    }
    
    Delay_ms(20);
}
