#ifndef __AAC_H__
#define __AAC_H__

#include "stm32f4xx.h"
#include "aacdec.h"
#include "ff.h"

#define AAC_TITSIZE_MAX		40		//歌曲名字最大长度
#define AAC_ARTSIZE_MAX		40		//歌曲名字最大长度
#define AAC_FILE_BUF_SZ     10*1024	//AAC 解码时,文件buf大小

//MP3控制结构体
typedef __packed struct 
{
    uint32_t totsec ;		//整首歌时长,单位:秒
    uint32_t cursec ;		//当前播放时长
	
	uint8_t  nChans;
	uint8_t  bit_depth;
    uint32_t bitrate;	   		//比特率
	uint32_t Samplerate_Core;	//采样率
	uint32_t Samplerate_Out;	//采样率
	uint16_t outputSamps;		//PCM输出数据量大小
	
	uint32_t datastart;		//数据帧开始的位置(在文件里面的偏移)
}__aacctrl;

extern __aacctrl * aacctrl;

void aac_i2s_dma_tx_callback(void);
uint32_t aac_file_seek(uint32_t pos);
void aac_get_curtime(FIL*fx,__aacctrl *aacx);
uint8_t aac_play_song_prepare(uint8_t* fname);
void aac_play_song_task(uint8_t* fname);

#endif
