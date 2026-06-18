#ifndef __AIFF_H__
#define __AIFF_H__

#include "stm32f4xx.h"                  // Device header
#include "ff.h"   

#define AIFF_I2S_TX_DMA_BUFSIZE    8*1024		
//定义AIFF TX DMA 数组大小
 
// AIFF 的大端 Chunk ID
#define AIFF_ID_FORM 0x464F524D // 'F' 'O' 'R' 'M'
#define AIFF_ID_AIFF 0x41494646 // 'A' 'I' 'F' 'F'
#define AIFF_ID_AIFC 0x41494643 // 'A' 'I' 'F' 'C' (部分苹果设备导出时使用的压缩/扩展版标记)
#define AIFF_ID_COMM 0x434F4D4D // 'C' 'O' 'M' 'M'
#define AIFF_ID_SSND 0x53534E44 // 'S' 'S' 'N' 'D'

//AIFF 播放控制结构体
typedef __packed struct
{ 
    uint16_t nchannels;				//通道数量;1,表示单声道;2,表示双声道; 
    uint16_t blockalign;			//块对齐(字节);  
    uint32_t datasize;				//音频PCM数据大小 

    uint32_t totsec ;				//整首歌时长,单位:秒
    uint32_t cursec ;				//当前播放时长
	
    uint32_t bitrate;	   			//比特率(位速)
    uint32_t samplerate;			//采样率 
    uint16_t bps;					//位数,比如16bit,24bit,32bit
	
    uint32_t datastart;				//数据帧开始的位置(在文件里面的绝对偏移量)
} __aiffctrl; 

extern __aiffctrl aiffctrl;

uint32_t aiff_file_seek(uint32_t pos);
void aiff_get_curtime(FIL* fx, __aiffctrl* aiffx);
void aiff_play_song_task(uint8_t* fname);

#endif
