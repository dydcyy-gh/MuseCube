#include "stm32f4xx.h"

#ifndef __USBH_MSC_CONF_H__
#define __USBH_MSC_CONF_H__

void usbh_msc_init(uint8_t busid, uintptr_t reg_base);
void usbh_msc_task(void);
void usbh_msc_deinit(void);

#endif
