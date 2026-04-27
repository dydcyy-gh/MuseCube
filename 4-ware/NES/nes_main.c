#include "nes_main.h" 
#include "nes_ppu.h"
#include "nes_mapper.h"
#include "nes_apu.h"

#include "malloc.h" 
#include <string.h>
#include "systick_conf.h"
#include "ff.h"
#include "variables.h"
#include "defines.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "i2s.h"

//////////////////////////////////////////////////////////////////////////////////	 
//本程序移植自网友ye781205的NES模拟器工程
//NES主函数 代码 (低内存防碎片+SD卡动态读取+DMA批处理+I2S音频 终极修复版)  	
//todo:lcd刷屏导致音频卡顿优化
////////////////////////////////////////////////////////////////////////////////// 	 
 
uint8_t nes_frame_cnt;		
int MapperNo;			    
int NES_scanline;		    
int VROM_1K_SIZE;
int VROM_8K_SIZE;

uint8_t PADdata;   			
uint8_t PADdata1;   		

void *raw_nes_ram_ptr = NULL; 
uint8_t *NES_RAM;			  
uint8_t *NES_SRAM;  
NES_header *RomHeader; 	      
MAPPER *NES_Mapper;		 
MapperCommRes *MAPx;  

uint8_t* spr_ram;			  
ppu_data* ppu;			      

uint8_t* PRG_ROM_Cache;       
uint8_t* VROM_banks;          
uint8_t* VROM_tiles;

apu_t *apu;
uint16_t *wave_buffers;       // 存放APU单声道数据
uint16_t *i2sbuf1;            // 存放I2S双声道数据缓冲1
uint16_t *i2sbuf2;            // 存放I2S双声道数据缓冲2

uint16_t *nes_dma_buf;        
uint32_t prg_file_offset = 0; 
uint32_t chr_file_offset = 0; 

FIL *nes_file; 

uint8_t nes_init(const char *path)
{
	UINT br;
	uint8_t res = 0;  

	nes_file = malloc_bsc(sizeof(FIL));  
	if(nes_file == 0) return 1;						
	res = f_open(nes_file, path, FA_READ);
	if(res != FR_OK) {
		free_bsc(nes_file);
		return 2;
	}
	
	raw_nes_ram_ptr = malloc_bsc(0x800 + 1024);
	if (raw_nes_ram_ptr) {
		NES_RAM = (uint8_t *)(((uint32_t)raw_nes_ram_ptr + 1023) & ~1023);
	}

	PRG_ROM_Cache = malloc_bsc(32 * 1024);        
	VROM_banks = malloc_bsc(8 * 1024);            
	VROM_tiles = VROM_banks;                      
	
    nes_dma_buf = malloc_bsc(240 * 12 * sizeof(uint16_t)); 
	ppu = malloc_bsc(sizeof(ppu_data));           
	spr_ram = malloc_bsc(0X100);                  
	RomHeader = malloc_bsc(sizeof(NES_header));
	NES_Mapper = malloc_bsc(sizeof(MAPPER));
	NES_SRAM = malloc_bsc(0X2000);  
    
    // 分配音频模块的内存
    apu = malloc_bsc(sizeof(apu_t));
    wave_buffers = malloc_bsc(APU_PCMBUF_SIZE * sizeof(uint16_t));
    i2sbuf1 = malloc_bsc(APU_PCMBUF_SIZE * 2 * sizeof(uint16_t)); 
    i2sbuf2 = malloc_bsc(APU_PCMBUF_SIZE * 2 * sizeof(uint16_t)); 
	
	if(!raw_nes_ram_ptr || !PRG_ROM_Cache || !VROM_banks || !nes_dma_buf || !ppu || !spr_ram || !RomHeader || !NES_Mapper || !NES_SRAM || !apu || !wave_buffers || !i2sbuf1 || !i2sbuf2)
	{
		return 1; // 内存不足
	}
    
	memset(NES_RAM, 0, 0X800);
	memset(NES_SRAM, 0, 0X2000); 
	memset(RomHeader, 0, sizeof(NES_header));	
	memset(NES_Mapper, 0, sizeof(MAPPER));	
	memset(spr_ram, 0, 0X100);				
	memset(ppu, 0, sizeof(ppu_data));			
	memset(PRG_ROM_Cache, 0, 32 * 1024);
	memset(VROM_banks, 0, 8 * 1024);
    memset(nes_dma_buf, 0, 240 * 12 * sizeof(uint16_t));
    memset(apu, 0, sizeof(apu_t));
    memset(wave_buffers, 0, APU_PCMBUF_SIZE * sizeof(uint16_t));
    memset(i2sbuf1, 0, APU_PCMBUF_SIZE * 2 * sizeof(uint16_t));
    memset(i2sbuf2, 0, APU_PCMBUF_SIZE * 2 * sizeof(uint16_t));
	
    f_lseek(nes_file, 0);
	f_read(nes_file, RomHeader, sizeof(NES_header), &br);	

	if(strncmp((char*)RomHeader->id, "NES", 3) == 0)
	{  
        prg_file_offset = 16 + ((RomHeader->flags_1 & 0x04) ? 512 : 0); 
        chr_file_offset = prg_file_offset + (RomHeader->num_16k_rom_banks * 16384);
        
		VROM_1K_SIZE = RomHeader->num_8k_vrom_banks * 8;
		VROM_8K_SIZE = RomHeader->num_8k_vrom_banks;  
        
        uint32_t prg_size = RomHeader->num_16k_rom_banks * 16384;
        f_lseek(nes_file, prg_file_offset);
        f_read(nes_file, PRG_ROM_Cache, prg_size > (32 * 1024) ? (32 * 1024) : prg_size, &br);

        if(VROM_8K_SIZE > 0) {		
             f_lseek(nes_file, chr_file_offset);
             f_read(nes_file, VROM_banks, 8192, &br);
		} 

		MapperNo=(RomHeader->flags_1>>4)|(RomHeader->flags_2&0xf0);
		if(RomHeader->flags_2 & 0x0E) MapperNo=RomHeader->flags_1>>4; 
		
        uint8_t i;
		for(i=0;i<255;i++) {		
			if (MapTab[i]==MapperNo) break;		
			if (MapTab[i]==-1) res=3; 
		} 
		if(res==0)
		{
			switch(MapperNo)
			{
				case 1:  
					MAP1 = malloc_bsc(sizeof(Mapper1Res)); 
					if(!MAP1) res=1;
					break;
				case 4:  case 6:  case 16: case 17: case 18: case 19:
				case 21: case 23: case 24: case 25: case 64: case 65:
				case 67: case 69: case 85: case 189:
					MAPx = malloc_bsc(sizeof(MapperCommRes)); 
					if(!MAPx) res=1;
					break;  
				default:
					break;
			}
		}
	} 

	Mapper_Init();						 
	cpu6502_init();							  	 
	PPU_reset();						
	
    // 初始化音频硬件
    apu_init();
    I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
    I2S2_SampleRate_Set(APU_SAMPLE_RATE); // 22050Hz
    I2S2_TX_DMA_Init((uint8_t*)i2sbuf1, (uint8_t*)i2sbuf2, APU_PCMBUF_SIZE * 2); 
	music_bitdepth = 16;
    I2S_Play_Start();

	return res;
}

uint8_t nes_frame = 0;

void nes_get_gamepad(void)
{
    uint8_t key = 0;

    /* ========== 右侧摇杆控制方向 ========== */
    // 假设 Y轴 >100 为上，<-100 为下
    if (g_key_R_Y >  80) key |= (1 << 4); // Up     (Bit 4)
    if (g_key_R_Y < -80) key |= (1 << 5); // Down   (Bit 5)
    // 假设 X轴 <-100 为左，>100 为右
    if (g_key_R_X < -80) key |= (1 << 6); // Left   (Bit 6)
    if (g_key_R_X >  80) key |= (1 << 7); // Right  (Bit 7)

    /* ========== 左侧摇杆控制功能键 ========== */
    // 向右映射为 A，向左映射为 B
    if (g_key_L_X < -80) key |= (1 << 0); // A      (Bit 0)
    if (g_key_L_X >  80) key |= (1 << 1); // B      (Bit 1)
    // 向上映射为 Select，向下映射为 Start
    if (g_key_L_Y >  80) key |= (1 << 2); // Select (Bit 2)
    if (g_key_L_Y < -80) key |= (1 << 3); // Start  (Bit 3)

    // 将状态赋给全局手柄变量1 (PADdata1留作备用或是2P)
    PADdata = key;
}


void nes_task(void)
{
	nes_get_gamepad();
	
	PPU_start_frame();
	for(NES_scanline = 0; NES_scanline< 240; NES_scanline++)
	{
		run6502(113*256);
		NES_Mapper->HSync(NES_scanline);
		if(nes_frame==0) scanline_draw(NES_scanline);
		else do_scanline_and_dont_draw(NES_scanline); 
	}  
	NES_scanline=240;
	run6502(113*256);
	NES_Mapper->HSync(NES_scanline); 
	start_vblank(); 
	if(NMI_enabled()) 
	{
		cpunmi=1;
		run6502(7*256);
	}
	NES_Mapper->VSync();
	for(NES_scanline=241;NES_scanline<262;NES_scanline++)
	{
		run6502(113*256);	  
		NES_Mapper->HSync(NES_scanline);		  
	}	   
	end_vblank(); 
 
    apu_process(wave_buffers, APU_PCMBUF_SIZE); 

	if (xSemaphoreTake(xI2SSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) 
	{
		uint16_t *target_buf = I2SdmaBuff ? i2sbuf2 : i2sbuf1;
		
		for(uint16_t i = 0; i < APU_PCMBUF_SIZE; i++) 
		{
			if(g_hdp0_or_spk1)
			{
				int32_t val = ((int32_t)((int16_t)wave_buffers[i]) * g_spk_value) >> 8;
				target_buf[i * 2]     = (uint16_t)val; // 左声道
				target_buf[i * 2 + 1] = (uint16_t)val; // 右声道
			}
			else //不用缩放
			{
				target_buf[i * 2]     = wave_buffers[i]; // 左声道
				target_buf[i * 2 + 1] = wave_buffers[i]; // 右声道
			}
		}
	}
	
	nes_frame_cnt++; 	
	nes_frame++;
	if(nes_frame>NES_SKIP_FRAME)nes_frame=0; 
}

void nes_deinit(void)
{
    I2S_Play_Stop();

	if(nes_file) {
		f_close(nes_file);
		free_bsc(nes_file);     				
	}
	
	if(raw_nes_ram_ptr) free_bsc(raw_nes_ram_ptr);
	if(NES_SRAM) free_bsc(NES_SRAM);	
	if(RomHeader) free_bsc(RomHeader);	
	if(NES_Mapper) free_bsc(NES_Mapper);
	if(spr_ram) free_bsc(spr_ram);		
	if(ppu) free_bsc(ppu);	

    if(PRG_ROM_Cache) free_bsc(PRG_ROM_Cache);
    if(VROM_banks) free_bsc(VROM_banks);
    if(nes_dma_buf) free_bsc(nes_dma_buf);
    
    if(apu) free_bsc(apu);
    if(wave_buffers) free_bsc(wave_buffers);
    if(i2sbuf1) free_bsc(i2sbuf1);
    if(i2sbuf2) free_bsc(i2sbuf2);

	switch (MapperNo)
	{
		case 1: 			
			if(MAP1) free_bsc(MAP1);
			break;	 	
		case 4: case 6: case 16: case 17: case 18: case 19: case 21: case 23:
		case 24: case 25: case 64: case 65: case 67: case 69: case 85: case 189:
			if(MAPx) free_bsc(MAPx);
			break;	 		 
		default:break; 
	}
	
	raw_nes_ram_ptr=0; NES_RAM=0; NES_SRAM=0; RomHeader=0; NES_Mapper=0; 
	spr_ram=0; ppu=0; VROM_banks=0; VROM_tiles=0; PRG_ROM_Cache=0; 
	MAP1=0; MAPx=0; nes_file=0; nes_dma_buf=0; 
    apu=0; wave_buffers=0; i2sbuf1=0; i2sbuf2=0;
}
