#include "stm32f4xx.h"

#ifndef __USBD_MSC_CONF_H__
#define __USBD_MSC_CONF_H__

void usbd_msc_init(uint8_t busid, uintptr_t reg_base);
void usbd_msc_deinit(void);
void usbd_msc_task(void);

#endif
