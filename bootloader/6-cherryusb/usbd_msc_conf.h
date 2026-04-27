#include "stm32f4xx.h"

#ifndef __USBD_MSC_CONF_H__
#define __USBD_MSC_CONF_H__

void usb_msc_init(uint8_t busid, uintptr_t reg_base);
void msc_bootuf2_init(uint8_t busid, uintptr_t reg_base);
void usb_msc_deinit(void);

#endif
