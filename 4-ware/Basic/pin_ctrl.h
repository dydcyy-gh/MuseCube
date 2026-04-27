#include "stm32f4xx.h"                  // Device header

#ifndef __PIN_CTRL_H__
#define __PIN_CTRL_H__

void Is_Battery_Charging(void);//充电检测
void Is_Headphone_Connecting(void);//耳机插入检测
void Is_TFcard_Connecting(void);//TF卡插入检测

void Power_Maintain_Ctrl(uint8_t status);//不断电控制
void I2S_Exchange_Ctrl(uint8_t status);//i2s切换控制

void Speaker_Power_Ctrl(uint8_t status);//音响供电
void Headphone_Power_Ctrl(uint8_t status);//耳放供电

//USB向外供电
void USB_Power_Out_Ctrl(uint8_t status); //0-关  1-开
//USB主从切换
void USB_Slave_Host_Ctrl(uint8_t status); //0-从  1-主

void Pin_Ctrl_Init(void);

#endif
