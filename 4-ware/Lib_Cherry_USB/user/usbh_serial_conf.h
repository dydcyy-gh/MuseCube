#include "stm32f4xx.h"

#ifndef __USBH_SERIAL_CONF_H__
#define __USBH_SERIAL_CONF_H__

void usbh_serial_init(uint8_t busid, uintptr_t reg_base);
void usbh_serial_task(void);
void usbh_serial_deinit(void);

#endif
