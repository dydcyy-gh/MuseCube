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

#define SD_CARD	 0  // SD卡, 卷标为0
#define USB_DISK 1  // U盘,  卷标为1

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	switch (pdrv) 
	{
		case SD_CARD :
			return 0;
	
		case USB_DISK :
			return 0;
	}
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	u8 res=0;	    
	switch(pdrv)
	{
		case SD_CARD:		// SD卡
			res = SD_Init();
			break;
		case USB_DISK:		// U盘
			res = USB_disk_initialize();
			break;
		default:
			res=1; 
	}		 
	if(res) return 1;
	else return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	u8 res=0; 
	if (!count) return RES_PARERR;		 	 
	switch(pdrv)
	{
		case SD_CARD:
			res = SD_ReadDisk(buff, sector, count);	 
			break;
		case USB_DISK:
			res = USB_disk_read(buff, sector, count);
			break;
		default:
			res=1; 
	}

	if(res==0x00) return RES_OK;	 
	else return RES_ERROR;	
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	u8 res=0;  
	if (!count) return RES_PARERR;		 	 
	switch(pdrv)
	{
		case SD_CARD:
			res = SD_WriteDisk((u8*)buff, sector, count);
			while(res)	// 写出错时重试
			{
				SD_Init();
				res = SD_WriteDisk((u8*)buff, sector, count);	
			}
			break;
		case USB_DISK:
			res = USB_disk_write(buff, sector, count);
			break;
		default:
			res=1; 
	}
	if(res == 0x00) return RES_OK;	 
	else return RES_ERROR;	
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	uint8_t res;						  			     
	if(pdrv == SD_CARD)
	{
		switch(cmd)
		{
			case CTRL_SYNC:
				res = RES_OK; 
				break;	 
			case GET_SECTOR_SIZE:
				*(uint32_t*)buff = 512; 
				res = RES_OK;
				break;	 
			case GET_BLOCK_SIZE:
				*(uint16_t*)buff = SDCardInfo.CardBlockSize;
				res = RES_OK;
				break;	 
			case GET_SECTOR_COUNT:
				*(uint32_t*)buff = SDCardInfo.CardCapacity / 512;
				res = RES_OK;
				break;
			default:
				res = RES_PARERR;
				break;
		}
	}
	else if(pdrv == USB_DISK)
	{
		res = USB_disk_ioctl(cmd, buff);
	}
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
