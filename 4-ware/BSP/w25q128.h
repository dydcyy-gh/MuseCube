#include "stm32f4xx.h"

#ifndef __W25Q128_H__
#define __W25Q128_H__		    

uint8_t W25QXX_Init(void);
void W25QXX_Read(uint8_t* pBuffer,uint32_t ReadAddr,uint32_t NumByteToRead);   //读取flash
void W25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint32_t NumByteToWrite);//写入flash
void W25QXX_Erase_Sector(uint32_t Dst_Addr);   
void W25QXX_Erase_Chip(void);    	  			//整片擦除
void W25QXX_Write_Raw(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite);

#endif
















