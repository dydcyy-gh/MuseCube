#include "stm32f4xx.h"

#ifndef __USBD_HID_CONF_H__
#define __USBD_HID_CONF_H__

void usbd_hid_init(uint8_t busid, uintptr_t reg_base);
void usbd_hid_task(void);
void usbd_hid_deinit(void);

#endif
