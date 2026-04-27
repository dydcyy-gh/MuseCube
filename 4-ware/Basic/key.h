#include "stm32f4xx.h"                  // Device header

#ifndef __KEY_H__
#define __KEY_H__

void Wakeup_Key_Init(void);
void Enter_Standby_Mode(void);
void Read_Wakeup_Key(void);

void Stick_Middle_Init(void);
void Get_Stick_Middle(void);

#endif
