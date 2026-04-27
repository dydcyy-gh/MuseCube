#include "stm32f4xx.h"

#ifndef __USBH_HID_CONF_H__
#define __USBH_HID_CONF_H__

void usbh_hid_init(uint8_t busid, uintptr_t reg_base);
void usbh_hid_task(void);
void usbh_hid_deinit(void);

#endif
