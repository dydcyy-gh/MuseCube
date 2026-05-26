#include "stm32f4xx.h"

#ifndef __USBH_FATFS_H__
#define __USBH_FATFS_H__

// 增加 usb_id 参数
uint8_t USB_disk_initialize(uint8_t usb_id);
uint8_t USB_disk_read(uint8_t usb_id, uint8_t *buff, uint32_t sector, uint32_t count);
uint8_t USB_disk_write(uint8_t usb_id, const uint8_t *buff, uint32_t sector, uint32_t count);
uint8_t USB_disk_ioctl(uint8_t usb_id, uint8_t cmd, void *buff);

#endif
