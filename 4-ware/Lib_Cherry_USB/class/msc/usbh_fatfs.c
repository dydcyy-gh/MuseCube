/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include "diskio.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include <stdio.h>

// 变成数组，支持 2 块 U 盘
struct usbh_msc *active_msc_class[2] = {NULL, NULL};

uint8_t USB_disk_initialize(uint8_t usb_id)
{
    char dev_name[10];
    if(usb_id >= 2) return RES_PARERR;
    
    // U盘0 对应 /dev/sda, U盘1 对应 /dev/sdb
    snprintf(dev_name, sizeof(dev_name), "/dev/sd%c", 'a' + usb_id);
    
    active_msc_class[usb_id] = (struct usbh_msc *)usbh_find_class_instance(dev_name);
    if (active_msc_class[usb_id] == NULL) {
        return RES_NOTRDY;
    }
    if (usbh_msc_scsi_init(active_msc_class[usb_id]) < 0) {
        return RES_NOTRDY;
    }
    return RES_OK;
}

uint8_t USB_disk_read(uint8_t usb_id, uint8_t *buff, uint32_t sector, uint32_t count)
{
    if(usb_id >= 2 || !active_msc_class[usb_id]) return RES_NOTRDY;
    
    int ret = usbh_msc_scsi_read10(active_msc_class[usb_id], sector, (uint8_t *)buff, count);
    return (ret < 0) ? RES_ERROR : RES_OK;
}

uint8_t USB_disk_write(uint8_t usb_id, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    if(usb_id >= 2 || !active_msc_class[usb_id]) return RES_NOTRDY;
    
    int ret = usbh_msc_scsi_write10(active_msc_class[usb_id], sector, (uint8_t *)buff, count);
    return (ret < 0) ? RES_ERROR : RES_OK;
}

uint8_t USB_disk_ioctl(uint8_t usb_id, uint8_t cmd, void *buff)
{
    if(usb_id >= 2 || !active_msc_class[usb_id]) return RES_NOTRDY;
    
    uint8_t result = 0;
    switch (cmd) {
        case CTRL_SYNC:
            result = RES_OK;
            break;
        case GET_SECTOR_SIZE:
            *(uint16_t *)buff = active_msc_class[usb_id]->blocksize;
            result = RES_OK;
            break;
        case GET_BLOCK_SIZE:
            *(uint32_t *)buff = 1;
            result = RES_OK;
            break;
        case GET_SECTOR_COUNT:
            *(uint32_t *)buff = active_msc_class[usb_id]->blocknum;
            result = RES_OK;
            break;
        default:
            result = RES_PARERR;
            break;
    }
    return result;
}
