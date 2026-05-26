/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */
#include "sdio_sdcard.h"
#include "usbh_fatfs.h"
#include "malloc.h"       
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"

// 修改开头的宏定义
#define SD_CARD	 0  // SD卡, 卷标为 "0:"
#define USB_DISK1 1 // U盘1, 卷标为 "1:"
#define USB_DISK2 2 // U盘2, 卷标为 "2:"

/*-----------------------------------------------------------------------*/
DSTATUS disk_status (BYTE pdrv)
{
	return 0; // 都不作限制
}

/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (BYTE pdrv)
{
	u8 res=0;	    
	switch(pdrv)
	{
		case SD_CARD:   res = SD_Init(); break;
		case USB_DISK1: res = USB_disk_initialize(0); break; // 传入ID 0
		case USB_DISK2: res = USB_disk_initialize(1); break; // 传入ID 1
		default: res=1; 
	}		 
	return res ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
	u8 res=0; 
	if (!count) return RES_PARERR;		 	 
	switch(pdrv)
	{
		case SD_CARD:   res = SD_ReadDisk(buff, sector, count); break;
		case USB_DISK1: res = USB_disk_read(0, buff, sector, count); break;
		case USB_DISK2: res = USB_disk_read(1, buff, sector, count); break;
		default: res=1; 
	}
	return (res==0x00) ? RES_OK : RES_ERROR;	
}

/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
	u8 res=0;  
	if (!count) return RES_PARERR;		 	 
	switch(pdrv)
	{
		case SD_CARD:
			res = SD_WriteDisk((u8*)buff, sector, count);
			while(res) { SD_Init(); res = SD_WriteDisk((u8*)buff, sector, count); }
			break;
		case USB_DISK1: res = USB_disk_write(0, buff, sector, count); break;
		case USB_DISK2: res = USB_disk_write(1, buff, sector, count); break;
		default: res=1; 
	}
	return (res == 0x00) ? RES_OK : RES_ERROR;	
}
#endif

/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
	uint8_t res;						  			     
	if(pdrv == SD_CARD)
	{
        // 保持原样...
		switch(cmd)
		{
			case CTRL_SYNC: res = RES_OK; break;	 
			case GET_SECTOR_SIZE: *(uint32_t*)buff = 512; res = RES_OK; break;	 
			case GET_BLOCK_SIZE: *(uint16_t*)buff = SDCardInfo.CardBlockSize; res = RES_OK; break;	 
			case GET_SECTOR_COUNT: *(uint32_t*)buff = SDCardInfo.CardCapacity / 512; res = RES_OK; break;
			default: res = RES_PARERR; break;
		}
	}
	else if(pdrv == USB_DISK1) { res = USB_disk_ioctl(0, cmd, buff); }
	else if(pdrv == USB_DISK2) { res = USB_disk_ioctl(1, cmd, buff); }
	else res = RES_ERROR;
    
	return (DRESULT)res;
}

uint32_t get_fattime(void)
{
    // STM32 标准库的 Year 通常是 0-99 (代表 2000-2099)
    // FatFs 的年份起点是 1980
    // 例如：2026 年，now_date.RTC_Year = 26
    // FatFs Year = 2000 + 26 - 1980 = 46 (或者直接 26 + 20)
    
    return (DWORD)(
        ((now_date.RTC_Year + 20) << 25) |  // Year: 2000 + Year - 1980
        (now_date.RTC_Month << 21)       |  // Month: 1..12
        (now_date.RTC_Date << 16)        |  // Day: 1..31
        (now_time.RTC_Hours << 11)       |  // Hour: 0..23
        (now_time.RTC_Minutes << 5)      |  // Min: 0..59
        (now_time.RTC_Seconds >> 1)         // Sec: 0..29 (除以2)
    );
}
