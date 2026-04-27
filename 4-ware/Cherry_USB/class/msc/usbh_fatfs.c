/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include "diskio.h"
#include "usbh_core.h"
#include "usbh_msc.h"

struct usbh_msc *active_msc_class;

uint8_t USB_disk_initialize(void)
{
    active_msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
    if (active_msc_class == NULL) {
        return RES_NOTRDY;
    }
    if (usbh_msc_scsi_init(active_msc_class) < 0) {
        return RES_NOTRDY;
    }
    return RES_OK;
}

uint8_t USB_disk_read(uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;

    ret = usbh_msc_scsi_read10(active_msc_class, sector, align_buf, count);
    if (ret < 0) {
        ret = RES_ERROR;
    } else {
        ret = RES_OK;
    }
    return (uint8_t)ret;
}

uint8_t USB_disk_write(const uint8_t *buff, uint32_t sector, uint32_t count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;

    ret = usbh_msc_scsi_write10(active_msc_class, sector, align_buf, count);
    if (ret < 0) {
        ret = RES_ERROR;
    } else {
        ret = RES_OK;
    }
    return (uint8_t)ret;
}

uint8_t USB_disk_ioctl(uint8_t cmd, void *buff)
{
    uint8_t result = 0;

    switch (cmd) {
        case CTRL_SYNC:
            result = RES_OK;
            break;

        case GET_SECTOR_SIZE:
            *(uint16_t *)buff = active_msc_class->blocksize;
            result = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(uint32_t *)buff = 1;
            result = RES_OK;
            break;

        case GET_SECTOR_COUNT:
            *(uint32_t *)buff = active_msc_class->blocknum;
            result = RES_OK;
            break;

        default:
            result = RES_PARERR;
            break;
    }

    return result;
}
