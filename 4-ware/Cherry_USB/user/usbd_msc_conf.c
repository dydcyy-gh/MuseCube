/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_msc.h"
#include "sdio_sdcard.h"
#include "variables.h"
#include "defines.h"
#include "fatfs.h"

/*!< endpoint address */
#define MSC_IN_EP  0x81
#define MSC_OUT_EP 0x02

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + MSC_DESCRIPTOR_LEN)

#ifdef CONFIG_USB_HS
#define MSC_MAX_MPS 512
#else
#define MSC_MAX_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    MSC_DESCRIPTOR_INIT(0x00, MSC_OUT_EP, MSC_IN_EP, MSC_MAX_MPS, 0x00)
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
    "CherryUSB MSC DEMO",         /* Product */
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

const struct usb_descriptor msc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;
    (void)event;
}

static struct usbd_interface intf0;

void usbd_msc_init(uint8_t busid, uintptr_t reg_base)
{
	fatfs_unmount(DEV_SD);
#ifdef CONFIG_USBDEV_ADVANCE_DESC
    usbd_desc_register(busid, &msc_descriptor);
#else
    usbd_desc_register(busid, msc_descriptor);
#endif

    usbd_add_interface(busid, usbd_msc_init_intf(busid, &intf0, MSC_OUT_EP, MSC_IN_EP));
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void usbd_msc_deinit(void)
{
	fatfs_mount(DEV_SD);
	usbd_ep_close(0, MSC_IN_EP);
    usbd_ep_close(0, MSC_OUT_EP);
	usbd_deinitialize(0);
}

extern void my_usbd_msc_thread(void); //from usbd_msc.c

void usbd_msc_task(void)
{
	my_usbd_msc_thread();
}

	
#define SD_SECTOR_SIZE 512

/* 查询容量：仅SD卡 */
void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    (void)busid;
    (void)lun;
    *block_size = SD_SECTOR_SIZE;
    *block_num  = SDCardInfo.CardCapacity / SD_SECTOR_SIZE;
}

/* 扇区读：仅SD卡 */
int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    (void)busid;
    (void)lun;
    return SD_ReadDisk(buffer, sector, length / SD_SECTOR_SIZE);
}

/* 扇区写：仅SD卡 */
int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    (void)busid;
    (void)lun;
    return SD_WriteDisk(buffer, sector, length / SD_SECTOR_SIZE);
}
