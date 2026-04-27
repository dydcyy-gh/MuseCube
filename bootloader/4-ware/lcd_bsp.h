#include "stm32f4xx.h"                  // Device header

#ifndef __LCD_BSP_H__
#define __LCD_BSP_H__

void LCD_Init(void);//LCD初始化
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count);
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t *color);

void Display_Icon(uint8_t y);
void Display_None(uint8_t y);
void Progress_Bar(uint8_t y, uint8_t num);

#endif
