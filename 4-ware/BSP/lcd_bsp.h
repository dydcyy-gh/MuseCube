#include "stm32f4xx.h"                  // Device header

#ifndef __LCD_BSP_H__
#define __LCD_BSP_H__

void LCD_Init(void);//LCD初始化
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count);
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t *color);

/* 等待 LCD SPI-DMA 传输结束，带超时。返回 1=已完成 0=超时。
   判定依据是硬件状态（DMA stream 已关闭且 SPI 空闲），不依赖事件组置位，
   因此即使完成中断的置位丢失也不会永久阻塞。 */
uint8_t LCD_Wait_DMA_Done(uint32_t timeout_ms);

#endif
