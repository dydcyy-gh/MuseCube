#ifndef __CHIP_ROM_H__
#define __CHIP_ROM_H__		    

#include "stm32f4xx.h"
#include <stddef.h> // 解决 size_t 未定义的报错

// 对外提供的接口声明
void boot2uf2_flash_init(void);
int bootuf2_flash_write(uint32_t address, const uint8_t *data, size_t size);

#endif
