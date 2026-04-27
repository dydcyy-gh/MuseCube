#include "stm32f4xx.h"                  // Device header
#include "ff.h"   

#ifndef __WAV_H__
#define __WAV_H__

#define WAV_I2S_TX_DMA_BUFSIZE    8*1024		
//定义WAV TX DMA 数组大小(播放192Kbps@24bit的时候,需要设置8192大才不会卡)
 
//RIFF块
typedef __packed struct
{
    uint32_t ChunkID;		   	//chunk id;这里固定为"RIFF",即0X46464952
    uint32_t ChunkSize;		   	//集合大小;文件总大小-8
    uint32_t Format;	   	    //格式;WAVE,即0X45564157
} ChunkRIFF;

//fmt块
typedef __packed struct
{
    uint32_t ChunkID;		   	//chunk id;这里固定为"fmt ",即0X20746D66
    uint32_t ChunkSize;		   	//子集合大小(不包括ID和Size);这里为:20
    uint16_t AudioFormat;	  	//音频格式;0X01,表示线性PCM;0X11表示IMA ADPCM
    uint16_t NumOfChannels;		//通道数量;1,表示单声道;2,表示双声道;
    uint32_t SampleRate;	    //采样率;0X1F40,表示8Khz
    uint32_t ByteRate;			//字节速率; 
    uint16_t BlockAlign;		//块对齐(字节); 
    uint16_t BitsPerSample;		//单个采样数据大小;4位ADPCM,设置为4
} ChunkFMT;

//fact块 
typedef __packed struct 
{
    uint32_t ChunkID;		   	//chunk id;这里固定为"fact",即0X74636166;
    uint32_t ChunkSize;		   	//子集合大小(不包括ID和Size);这里为:4
    uint32_t NumOfSamples;	  	//采样的数量; 
} ChunkFACT;

//LIST块
typedef __packed struct 
{
    uint32_t ChunkID;		   	//chunk id;这里固定为"list",即0x5453494C;
    uint32_t ChunkSize ;		//子集合大小(不包括ID和Size);这里为:4   
} ChunkLIST;

//data块 
typedef __packed struct 
{
    uint32_t ChunkID;		   	//chunk id;这里固定为"data",即0X61746164;
    uint32_t ChunkSize;		   	//子集合大小(不包括ID和Size) 
} ChunkDATA;

////wav头
//typedef __packed struct
//{ 
//    ChunkRIFF riff;	//riff块
//    ChunkFMT fmt;  	//fmt块
//    ChunkDATA data;	//data块		 
//} __WaveHeader; 

//wav 播放控制结构体
typedef __packed struct
{ 
    uint16_t audioformat;			//音频格式;0X01,表示线性PCM;0X11表示IMA ADPCM
    uint16_t nchannels;				//通道数量;1,表示单声道;2,表示双声道; 
    uint16_t blockalign;			//块对齐(字节);  
    uint32_t datasize;				//WAV数据大小 

    uint32_t totsec ;				//整首歌时长,单位:秒
    uint32_t cursec ;				//当前播放时长
	
    uint32_t bitrate;	   			//比特率(位速)
    uint32_t samplerate;			//采样率 
    uint16_t bps;					//位数,比如16bit,24bit,32bit
	
    uint32_t datastart;				//数据帧开始的位置(在文件里面的偏移)
} __wavctrl; 

extern __wavctrl wavctrl;
extern uint8_t wavtransferend;	// I2S 传输完成标志

uint32_t wav_file_seek(uint32_t pos);
void wav_i2s_dma_tx_callback(void);
void wav_get_curtime(FIL* fx, __wavctrl* wavx);
void wav_play_song_task(uint8_t* fname);

#endif
















