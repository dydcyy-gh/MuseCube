#ifndef __OGG_H__
#define __OGG_H__

#include "stm32f4xx.h"
#include "ff.h"

// DMA 传输大小
#define OGG_I2S_TX_DMA_BUFSIZE    2*1024

// 提前声明 stb_vorbis 结构体（避开庞大的头文件引入）
typedef struct stb_vorbis stb_vorbis;

// OGG 播放控制结构体
typedef struct
{ 
	stb_vorbis *stb_vf;             // stb_vorbis 解码状态句柄
	
	uint16_t nchannels;             // 通道数量: 1 单声道, 2 双声道
	uint32_t samplerate;            // 采样率
	uint16_t bps;                   // OGG解码输出固定为 16bit
	
	uint32_t totsec;                // 整首歌时长 (秒)
	uint32_t cursec;                // 当前播放时长 (秒)
} __oggctrl; 

extern __oggctrl oggctrl;

void ogg_file_seek(uint32_t target_sec);
void ogg_play_song_task(uint8_t* fname);

#endif

