#include "stm32f4xx.h"
#include <stdbool.h>

#ifndef __USBD_CDC_CONF_H__
#define __USBD_CDC_CONF_H__

void usbd_cdc_init(uint8_t busid, uintptr_t reg_base);
int usb_cdc_send_data(const uint8_t *data, uint32_t len);
void usb_printf(const char *format, ...);
void usbd_cdc_deinit(void);
void usbd_cdc_cmd_task(void);
bool usbd_cdc_is_ready(void);

#endif
