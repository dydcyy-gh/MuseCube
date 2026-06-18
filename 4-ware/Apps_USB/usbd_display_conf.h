/*
 * Copyright (c) 2026, sakumisu
 * Optimized for STM32F405 & LVGL
 */
#ifndef USBD_DISPLAY_CONF_H
#define USBD_DISPLAY_CONF_H

#include "stm32f4xx.h" 

void usb_display_init(uint8_t busid, uintptr_t reg_base);
void usb_display_task(void);
void usb_display_deinit(void);

#endif
