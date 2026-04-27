/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include <stdarg.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

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
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
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

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

#define USB_PRINTF_BUF_SIZE 128
USB_MEM_ALIGNX static uint8_t write_buffer[USB_PRINTF_BUF_SIZE];
USB_MEM_ALIGNX static uint8_t read_buffer[CDC_MAX_MPS];

volatile static bool is_configured = false;
volatile static uint8_t dtr_enable = 0;

/* 定义 FreeRTOS 信号量与互斥锁 */
static SemaphoreHandle_t usb_tx_cplt_sem = NULL; // 发送完成信号量
static SemaphoreHandle_t usb_tx_mutex = NULL;    // 线程安全互斥锁

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_CONFIGURED:
            is_configured = true;
            /* 每次枚举成功重新复位信号量，防止遗留状态 */
            xSemaphoreTake(usb_tx_cplt_sem, 0); 
            usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
            break;
        case USBD_EVENT_DISCONNECTED:
            is_configured = false;
            /* 如果设备断开，释放可能被阻塞的任务 */
            xSemaphoreGive(usb_tx_cplt_sem);
            break;
        /* ... 其他保持不变 ... */
        default:
            break;
    }
}

void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
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

/*!< endpoint call back */
struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

static struct usbd_interface intf0;
static struct usbd_interface intf1;

/* 初始化处创建信号量和互斥锁 */
void usbd_cdc_init(uint8_t busid, uintptr_t reg_base)
{
    /* 创建二值信号量，初始为空 */
    if (usb_tx_cplt_sem == NULL) {
        usb_tx_cplt_sem = xSemaphoreCreateBinary();
    }
    /* 创建互斥锁 */
    if (usb_tx_mutex == NULL) {
        usb_tx_mutex = xSemaphoreCreateMutex();
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
    // 释放可能卡在发送中的任务
    if (usb_tx_cplt_sem != NULL) {
        xSemaphoreGive(usb_tx_cplt_sem);
    }
	usbd_ep_close(0, CDC_IN_EP);
    usbd_ep_close(0, CDC_OUT_EP);
    usbd_ep_close(0, CDC_INT_EP);
	usbd_deinitialize(0);
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    dtr_enable = dtr ? 1 : 0;
}

int usb_cdc_send_data(const uint8_t *data, uint32_t len)
{
    if (!is_configured || !dtr_enable) return -1; 
    if (__get_IPSR() != 0) return -1;

    if (xSemaphoreTake(usb_tx_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -2;
    }
	
    xSemaphoreTake(usb_tx_cplt_sem, 0); 

    usbd_ep_start_write(0, CDC_IN_EP, data, len);

    if (xSemaphoreTake(usb_tx_cplt_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
        xSemaphoreGive(usb_tx_mutex);
        return -3; 
    }

    xSemaphoreGive(usb_tx_mutex);
    return len;
}

// 修改 usb_printf 函数
void usb_printf(const char *format, ...)
{
    if (!is_configured || !dtr_enable) return;
    if (__get_IPSR() != 0) return;

    if (xSemaphoreTake(usb_tx_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        va_list args;
        va_start(args, format);
        int length = vsnprintf((char *)write_buffer, USB_PRINTF_BUF_SIZE - 3, format, args);
        va_end(args);

        if (length > 0) 
        {
            if (length >= USB_PRINTF_BUF_SIZE - 3) length = USB_PRINTF_BUF_SIZE - 3;

            for (int i = 0; i < length; i++) {
                if (write_buffer[i] == '\r' || write_buffer[i] == '\n') {
                    write_buffer[i] = ' ';
                }
            }

            write_buffer[length++] = '\r';
            write_buffer[length++] = '\n';
            write_buffer[length] = '\0';

            xSemaphoreTake(usb_tx_cplt_sem, 0);
            usbd_ep_start_write(0, CDC_IN_EP, write_buffer, length);
            xSemaphoreTake(usb_tx_cplt_sem, pdMS_TO_TICKS(100));
        }
        xSemaphoreGive(usb_tx_mutex);

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
