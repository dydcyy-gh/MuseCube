#include "stm32f4xx.h"

#ifndef __DEBUG_UNIT_H__
#define __DEBUG_UNIT_H__

void lvgl_printf(const char *format, ...);

void Create_Debug_Unit(void);
void Update_Debug_Unit(void);
void Remove_Debug_Unit(void);

#endif
