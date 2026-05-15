#include "stm32f4xx.h"
#include "ff.h" 

#ifndef __MUSIC_H__
#define __MUSIC_H__

typedef __packed struct
{
	uint8_t *i2sbuf1;
	uint8_t *i2sbuf2; 
	uint8_t *tbuf;
	FIL *file;
}__musicctrl;

//MUSIC信息结构体
typedef __packed struct
{
	char *music_name;            	//音乐名
	char *artist_name;       		//歌手名
	uint8_t file_format;            //音乐文件格式

	uint16_t file_date;             
    uint16_t file_time;             
	
    uint16_t total_sec;
    uint16_t current_sec;
	
    uint32_t bitrate;
    uint32_t samplerate;
    uint8_t bit_depth;
} __musicinfo; 

extern __musicctrl music_ctrl;
extern __musicinfo music_info;

void play_next_song(void);
void play_previous_song(void);
void play_same_song(void);
void play_random_song(void);
uint8_t play_specific_song(const char* filepath);

uint8_t audio_play_prepare(void);
void audio_play_task(void);
void audio_play_clear(void);

#endif
