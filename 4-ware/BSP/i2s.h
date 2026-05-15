#include "stm32f4xx.h"                  // Device header

#ifndef __I2S_H__
#define __I2S_H__

extern volatile uint8_t I2SdmaBuff;

void I2S2_Init(uint16_t I2S_Standard, uint16_t I2S_Mode,uint16_t Clock_Polarity,uint16_t DataFormat);
uint8_t I2S2_SampleRate_Set(uint32_t samplerate);
void I2S2_TX_DMA_Init(uint8_t* buf0, uint8_t* buf1, uint16_t num);
void I2S_Play_Start(void);
void I2S_Play_Stop(void);

#endif
