#ifndef __FATFS_H__
#define __FATFS_H__

#include "stm32f4xx.h"                  
#include "ff.h"      

uint8_t fatfs_mount(uint8_t pdrv);
uint8_t fatfs_unmount(uint8_t pdrv);

uint8_t fatfs_get_space(const char *drv, uint16_t *total, uint16_t *free);
uint8_t* fatfs_get_extension(const char *path);
	
#endif
