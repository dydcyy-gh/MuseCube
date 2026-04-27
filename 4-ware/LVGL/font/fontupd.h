#ifndef __FONTUPD_H__
#define __FONTUPD_H__	 
#include <stm32f4xx.h>

//字库信息结构体定义
//用来保存字库基本信息，地址，大小等
__packed typedef struct 
{
	uint8_t  fontok;		
	uint32_t unigbkaddr; 	
	uint32_t unigbksize;	
	uint32_t font8addr;			
	uint32_t font8size;			
	uint32_t font12addr;		
	uint32_t font12size;			
	uint32_t font16addr;		
	uint32_t font16size;		
	uint32_t font24addr;		
	uint32_t font24size;					
}_font_info; 

extern _font_info ftinfo;	//字库信息结构体
extern uint8_t __g_font_buf[210];

uint8_t update_font(uint8_t* src);			        //更新全部字库
uint8_t font_init(void);					        //初始化字库

#endif





















