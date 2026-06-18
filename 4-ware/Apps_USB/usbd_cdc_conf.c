/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>     /* 引入 memcpy */
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "debug.h"
#include "malloc.h"
#include "shell.h"      /* 引入 Letter Shell 头文件 */

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x01  // 从 0x02 改为 0x01
#define CDC_INT_EP 0x82  // 从 0x83 改为 0x82

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

/* 自动适配 FS 的 64 字节包长 */
#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02)
};

static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB CDC DEMO",         /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed) { return device_descriptor; }
static const uint8_t *config_descriptor_callback(uint8_t speed) { return config_descriptor; }
static const uint8_t *device_quality_descriptor_callback(uint8_t speed) { return device_quality_descriptor; }
static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3) return NULL;
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

#define USB_PRINTF_BUF_SIZE 256
static uint8_t *write_buffer = NULL;
static uint8_t *read_buffer = NULL;

/* ========================================================================= */
/* Letter Shell 相关变量 */
/* ========================================================================= */
#define USB_SHELL_BUF_SIZE 256
static Shell usb_shell;
static char *usb_shell_buf = NULL;      // 动态分配的 Shell 对象工作缓存

/* 使用 malloc_bsc 分配的简易环形缓冲区，隔离中断与任务 */
#define USB_RX_RING_SIZE 256
static uint8_t *usb_rx_ring_buf = NULL; // 动态分配的接收缓冲
volatile static uint16_t rx_ring_head = 0; // 写指针 (中断用)
volatile static uint16_t rx_ring_tail = 0; // 读指针 (任务用)

volatile static bool is_configured = false;
volatile static uint8_t dtr_enable = 0;
static bool shell_started = false;

/* ========================================================================= */
/* USB Shell 专用写接口 (包含 DMA 内存复制处理)                              */
/* ========================================================================= */
static signed short usb_shell_write(char *data, unsigned short len)
{
    if (!is_configured || !dtr_enable || write_buffer == NULL || len == 0) return 0;
    if (len > USB_PRINTF_BUF_SIZE) len = USB_PRINTF_BUF_SIZE;

    if (xSemaphoreTake(usb_tx_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;

    xSemaphoreTake(usb_tx_cplt_sem, 0);
    memcpy(write_buffer, data, len);
    usbd_ep_start_write(0, CDC_IN_EP, write_buffer, len);

    if (xSemaphoreTake(usb_tx_cplt_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        xSemaphoreGive(usb_tx_mutex);
        return 0;
    }

    xSemaphoreGive(usb_tx_mutex);
    return len;
}

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_CONFIGURED:
            is_configured = true;
            xSemaphoreTake(usb_tx_cplt_sem, 0);
            if (read_buffer != NULL) {
                usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
            }
            break;
        case USBD_EVENT_DISCONNECTED:
            is_configured = false;
            xSemaphoreGive(usb_tx_cplt_sem);
            break;
        default:
            break;
    }
}

/* ========================================================================= */
/* 接收完成回调 (运行在中断上下文中)                                           */
/* ========================================================================= */
void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if (read_buffer != NULL) {
        /* 如果是 CMD 模式，将收到的数据塞入我们自己用 bsc 分配的环形缓冲区 */
        if (g_usb_function == USBD_CMD && usb_rx_ring_buf != NULL) {
            for (uint32_t i = 0; i < nbytes; i++) {
                uint16_t next_head = (rx_ring_head + 1) % USB_RX_RING_SIZE;
                if (next_head != rx_ring_tail) { // 如果环形缓冲区未满
                    usb_rx_ring_buf[rx_ring_head] = read_buffer[i];
                    rx_ring_head = next_head;
                }
            }
        }
        usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
    }
}

void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) 
    {
        usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0);
    } 
    if (usb_tx_cplt_sem != NULL) 
    {
        xSemaphoreGiveFromISR(usb_tx_cplt_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

struct usbd_endpoint cdc_out_ep = { .ep_addr = CDC_OUT_EP, .ep_cb = usbd_cdc_acm_bulk_out };
struct usbd_endpoint cdc_in_ep = { .ep_addr = CDC_IN_EP, .ep_cb = usbd_cdc_acm_bulk_in };

static struct usbd_interface intf0;
static struct usbd_interface intf1;

void usbd_cdc_init(uint8_t busid, uintptr_t reg_base)
{
    if (write_buffer == NULL) write_buffer = malloc_bsc(USB_PRINTF_BUF_SIZE);
    if (read_buffer == NULL) read_buffer = malloc_bsc(CDC_MAX_MPS);
    if (usb_shell_buf == NULL) usb_shell_buf = malloc_bsc(USB_SHELL_BUF_SIZE);
    if (usb_rx_ring_buf == NULL) usb_rx_ring_buf = malloc_bsc(USB_RX_RING_SIZE);
    
    /* 每次初始化时复位环形缓冲读写指针 */
    rx_ring_head = 0;
    rx_ring_tail = 0;

    if (write_buffer == NULL || read_buffer == NULL || usb_shell_buf == NULL || usb_rx_ring_buf == NULL) {
        usbd_cdc_deinit();
        return;
    }

    if (usb_tx_cplt_sem == NULL) usb_tx_cplt_sem = xSemaphoreCreateBinary();
    if (usb_tx_mutex == NULL) usb_tx_mutex = xSemaphoreCreateMutex();

    /* 预配置 Shell 回调 (延迟到主机打开串口后才 shellInit) */
    if (g_usb_function == USBD_CMD) {
        usb_shell.write = usb_shell_write;
        usb_shell.read = NULL;
        shell_started = false;
    }

#ifdef CONFIG_USBDEV_ADVANCE_DESC
    usbd_desc_register(busid, &cdc_descriptor);
#else
    usbd_desc_register(busid, cdc_descriptor);
#endif
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf1));
    usbd_add_endpoint(busid, &cdc_out_ep);
    usbd_add_endpoint(busid, &cdc_in_ep);
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void usbd_cdc_deinit(void)
{
    is_configured = false;
    shell_started = false;
    if (usb_tx_cplt_sem != NULL) xSemaphoreGive(usb_tx_cplt_sem);
    
    usbd_ep_close(0, CDC_IN_EP);
    usbd_ep_close(0, CDC_OUT_EP);
    usbd_ep_close(0, CDC_INT_EP);
    usbd_deinitialize(0);

    /* 释放 Shell 实例 */
    shellRemove(&usb_shell);

    /* 释放动态分配的所有内存 */
    if (write_buffer != NULL) { free_bsc(write_buffer); write_buffer = NULL; }
    if (read_buffer != NULL) { free_bsc(read_buffer); read_buffer = NULL; }
    if (usb_shell_buf != NULL) { free_bsc(usb_shell_buf); usb_shell_buf = NULL; }
    if (usb_rx_ring_buf != NULL) { free_bsc(usb_rx_ring_buf); usb_rx_ring_buf = NULL; }
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    dtr_enable = dtr ? 1 : 0;
}

bool usbd_cdc_is_ready(void)
{
    return is_configured && (dtr_enable != 0);
}

int usb_cdc_send_data(const uint8_t *data, uint32_t len)
{
    if (!is_configured || !dtr_enable) return -1;
    if (__get_IPSR() != 0) return -1;

    if (xSemaphoreTake(usb_tx_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return -2;
    xSemaphoreTake(usb_tx_cplt_sem, 0); 
    
    usbd_ep_start_write(0, CDC_IN_EP, data, len);
    
    if (xSemaphoreTake(usb_tx_cplt_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        xSemaphoreGive(usb_tx_mutex);
        return -3; 
    }
    xSemaphoreGive(usb_tx_mutex);
    return len;
}

void usb_printf(const char *format, ...)
{
    if (g_usb_function != USBD_LOG) return;
    if (!is_configured || !dtr_enable) return;
    if (__get_IPSR() != 0) return;
    if (write_buffer == NULL) return;

    if (xSemaphoreTake(usb_tx_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        va_list args;
        va_start(args, format);
        int length = vsnprintf((char *)write_buffer, USB_PRINTF_BUF_SIZE - 1, format, args);
        va_end(args);

        if (length > 0)
        {
            if (length >= USB_PRINTF_BUF_SIZE - 1) length = USB_PRINTF_BUF_SIZE - 1;

            xSemaphoreTake(usb_tx_cplt_sem, 0);
            usbd_ep_start_write(0, CDC_IN_EP, write_buffer, length);
            xSemaphoreTake(usb_tx_cplt_sem, pdMS_TO_TICKS(100));
        }
        xSemaphoreGive(usb_tx_mutex);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/* ========================================================================= */
/* 任务调用：解析处理环形缓冲中的指令 (由外部 USB_Task 循环调用)               */
/* ========================================================================= */
void usbd_cdc_cmd_task(void)
{
    /* 延迟启动：等到主机打开串口 (DTR) 后才初始化 Letter Shell */
    if (!shell_started && dtr_enable && usb_shell_buf != NULL) {
        shellInit(&usb_shell, usb_shell_buf, USB_SHELL_BUF_SIZE);
        usb_shell.status.isChecked = 1;
        usb_shell.status.isActive = 1;
        shell_started = true;
    }

    unsigned short processed = 0;

    if (usb_rx_ring_buf != NULL) {
        while (rx_ring_tail != rx_ring_head) {
            uint8_t ch = usb_rx_ring_buf[rx_ring_tail];
            rx_ring_tail = (rx_ring_tail + 1) % USB_RX_RING_SIZE;
            shellHandler(&usb_shell, ch);
            processed++;
        }
    }

    if (processed > 0) return;
    vTaskDelay(pdMS_TO_TICKS(20));
}
