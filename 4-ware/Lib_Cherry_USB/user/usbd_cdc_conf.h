#include "stm32f4xx.h"

#ifndef __USBD_CDC_CONF_H__
#define __USBD_CDC_CONF_H__

void usbd_cdc_init(uint8_t busid, uintptr_t reg_base);
int usb_cdc_send_data(const uint8_t *data, uint32_t len);
void usb_printf(const char *format, ...);
void usbd_cdc_deinit(void);

#endif
