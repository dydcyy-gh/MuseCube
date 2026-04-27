#ifndef __NES_MAIN_H
#define __NES_MAIN_H

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////////////	 
//本程序移植自网友ye781205的NES模拟器工程
//ALIENTEK STM32F407开发板
//NES主函数 代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/7/1
//版本：V1.0  			  
////////////////////////////////////////////////////////////////////////////////// 	 
 
#define NES_SKIP_FRAME 	2		//定义模拟器跳帧数,默认跳2帧
 
#define INLINE 	static inline
#define int8 	char
#define int16 	short
#define int32 	int
#define uint8 	unsigned char
#define uint16 	unsigned short
#define uint32 	unsigned int
#define boolean uint8 

//nes信息头结构体
typedef struct
{
	unsigned char id[3]; // 'NES'
	unsigned char ctrl_z; // control-z
	unsigned char num_16k_rom_banks;
	unsigned char num_8k_vrom_banks;
	unsigned char flags_1;
	unsigned char flags_2;
	unsigned char reserved[8];
}NES_header;   

extern uint8_t* PRG_ROM_Cache;
extern uint32_t prg_file_offset;
extern uint32_t chr_file_offset;
#include "ff.h" // 确保能认识 FIL
extern FIL *nes_file; 

extern uint16_t *nes_dma_buf;

extern uint8_t nes_frame_cnt;		//nes帧计数器
extern int MapperNo;				//map编号
extern int NES_scanline;			//扫描线
extern NES_header *RomHeader;		//rom文件头 
extern int VROM_1K_SIZE;
extern int VROM_8K_SIZE; 
extern uint8_t cpunmi;  			//cpu中断标志  在 6502.s里面
extern uint8_t cpuirq;			
extern uint8_t PADdata;   			//手柄1键值 
extern uint8_t PADdata1;   			//手柄1键值 
extern uint8_t lianan_biao;			//连按标志 
#define  CPU_NMI  cpunmi=1;
#define  CPU_IRQ  cpuirq=1;

void cpu6502_init(void);				//在 cart.s
void run6502(uint32_t); 		   		//在 6502.s 

uint8_t nes_init(const char *path);
void nes_task(void);
void nes_deinit(void);

#endif







