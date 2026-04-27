#ifndef __APE_H__ 
#define __APE_H__

#include "apedecoder.h"
#include "parser.h"
 
#define APE_FILE_BUF_SZ    			24*1024		//APE解码时,文件buf大小
#define APE_BLOCKS_PER_LOOP			2*1024		//APE解码时,每个循环解码block的个数

//APE控制结构体
typedef __packed struct 
{ 
    uint32_t totsec ;				//整首歌时长,单位:秒
    uint32_t cursec ;				//当前播放时长
	
    uint32_t bitrate;	   			//比特率
	uint32_t samplerate;			//采样率
	uint16_t outsamples;			//PCM输出数据量大小
	uint16_t bps;					//位数,比如16bit,24bit,32bit
	
	uint32_t datastart;				//数据帧开始的位置(在文件里面的偏移)
}__apectrl;

extern __apectrl * apectrl; 

void ape_fill_buffer(uint16_t* buf,uint16_t size);
void ape_i2s_dma_tx_callback(void);
void ape_get_curtime(FIL*fx,__apectrl *apectrl);
uint8_t ape_play_song_prepare(uint8_t* fname);
void ape_play_song_task(uint8_t* fname);

#endif
