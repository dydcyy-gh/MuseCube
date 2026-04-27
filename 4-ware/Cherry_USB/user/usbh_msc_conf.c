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
static volatile uint8_t g_usbh_msc_tested = 0;

int usb_msc_fatfs_test()
{
    const char *test_data = "cherryusb fatfs demo...\r\n";
    uint32_t data_len = strlen(test_data);
    uint32_t fnum;
    FIL fnew;
    uint8_t res_sd;

    // 挂载U盘
    res_sd = fatfs_mount(DEV_USB);
    if (res_sd != FR_OK) {
        USB_LOG_RAW("mount fail,res:%d\r\n", res_sd);
        return -1;
    }

    // 写入测试：只写入一次字符串
    res_sd = f_open(&fnew, "1:test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res_sd == FR_OK) {
        res_sd = f_write(&fnew, test_data, data_len, &fnum);
        if (res_sd == FR_OK) {
            USB_LOG_RAW("write success, write len:%d\n", fnum);
        } else {
            USB_LOG_RAW("write fail\r\n");
            f_close(&fnew);
            fatfs_unmount(DEV_USB);
            return -1;
        }
        f_close(&fnew);
    } else {
        USB_LOG_RAW("open fail\r\n");
        fatfs_unmount(DEV_USB);
        return -1;
    }

    // 读取测试：用一个小缓冲区读出数据并打印
    char read_buf[64] = {0};   // 局部缓冲区，足够容纳测试字符串
    res_sd = f_open(&fnew, "1:test.txt", FA_OPEN_EXISTING | FA_READ);
    if (res_sd == FR_OK) {
        res_sd = f_read(&fnew, read_buf, sizeof(read_buf) - 1, &fnum);
        if (res_sd == FR_OK) {
            USB_LOG_RAW("read success, read len:%d, content: %s\n", fnum, read_buf);
        } else {
            USB_LOG_RAW("read fail\r\n");
        }
        f_close(&fnew);
    } else {
        USB_LOG_RAW("open fail\r\n");
    }

    // 卸载U盘
    fatfs_unmount(DEV_USB);
    return 0;
}

/* 当U盘枚举成功后，底层会调用 run */
void usbh_msc_run(struct usbh_msc *msc_class)
{
    // 不再创建任务，只置标志位
    g_usbh_msc_connected = 1;
    g_usbh_msc_tested = 0; // 每次新插入重置执行标志
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
    g_usbh_msc_tested = 0;
    usbh_initialize(busid, reg_base, NULL);
}

// 2. 反初始化
void usbh_msc_deinit(void)
{
	fatfs_unmount(DEV_USB);
    usbh_deinitialize(0);
    g_usbh_msc_connected = 0;
    g_usbh_msc_tested = 0;
}

// 3. 业务任务 (由 USB_Task 循环调用)
void usbh_msc_task(void)
{
    // 只有在U盘挂载成功，并且尚未执行过测试的情况下才去读写
    if (g_usbh_msc_connected && !g_usbh_msc_tested) 
	{
        usb_msc_fatfs_test();
        g_usbh_msc_tested = 1;
    }
    Delay_ms(20);
    // 如果后续你需要轮询读取U盘里的文件/播放音乐，可在此处加入你的状态机
}
