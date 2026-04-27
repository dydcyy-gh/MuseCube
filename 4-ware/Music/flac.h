#ifndef __FLAC_H__ 
#define __FLAC_H__

#include "stm32f4xx.h"
#include "foxen-flac.h"
#include <inttypes.h>
#include <string.h>
#include "ff.h" 

//flaC 标签 
typedef __packed struct 
{
    uint8_t id[3];		   	//ID,在文件起始位置,必须是flaC 4个字母 
}FLAC_Tag;

//metadata 数据块头信息结构体 
typedef __packed struct 
{
    uint8_t head;		   	//metadata block头
	uint8_t size[3];		//metadata block数据长度	
}MD_Block_Head;


//FLAC控制结构体
typedef struct {
    fx_flac_t *decoder;           // foxen-flac解码器实例
    uint32_t samplerate;          // 采样率
	uint16_t nchannels;			  //通道数量;1,表示单声道;2,表示双声道; 
    uint32_t totsec;              // 总时长(秒)
    uint32_t cursec;              // 当前播放时长(秒)
    uint32_t datastart;           // 数据开始位置
    uint32_t bitrate;             // 比特率
    uint8_t bps;                  // 位深度
} __flacctrl;

extern __flacctrl * flacctrl;

uint8_t flac_init(FIL* fx, __flacctrl* fctrl);
void flac_i2s_dma_tx_callback(void);
void flac_get_curtime(FIL*fx,__flacctrl *flacx);
void flac_play_song_task(uint8_t* fname);

#endif



























