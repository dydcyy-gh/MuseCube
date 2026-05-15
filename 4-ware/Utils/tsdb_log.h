#ifndef __TSDB_LOG_H__
#define __TSDB_LOG_H__

#include "stm32f4xx.h"

void tsdb_printf(const char *format, ...);
void tsdb_show_recent_on_lvgl(int num);
void tsdb_show_recent_on_usb(int num);

#endif
