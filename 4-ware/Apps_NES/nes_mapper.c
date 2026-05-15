#include "nes_mapper.h"
#include "ff.h" 

extern uint8_t* PRG_ROM_Cache;
extern uint32_t prg_file_offset;
extern uint32_t chr_file_offset;
extern FIL *nes_file;
extern uint32_t cpu_data[]; 

#define ROMMAP_8000 (cpu_data[21])
#define ROMMAP_A000 (cpu_data[22])
#define ROMMAP_C000 (cpu_data[23])
#define ROMMAP_E000 (cpu_data[24])

//////////////////////////////////////////////////////////////////////////////////	 
//本程序移植自网友ye781205的NES模拟器工程
//NES MAP 驱动代码 (低内存改造 + SD卡防抖优化版)	   
////////////////////////////////////////////////////////////////////////////////// 	 

#include "mapper/000.cpp"
#include "mapper/001.cpp"
#include "mapper/002.cpp"
#include "mapper/003.cpp"
#include "mapper/004.cpp"

const int MapTab[] =  
{
	0,1,2,3,4,
	-1,
};

#define MASK_BANK(bank,mask) (bank) = ((bank) & (mask))
#define VALIDATE_VROM_BANK(bank) \
		MASK_BANK(bank,VROM_mask); \
		if((bank) >= VROM_1K_SIZE) return;
		
uint32 VROM_mask;  
uint32_t PRG_ROM_mask;

// 【性能优化点】记录当前槽位里存在的 Bank 号，避免重复读 SD 卡
static uint32_t current_prg_banks[4];
static uint32_t current_chr_banks[8];

void Mapper_Reset(void){} 
uint8 Mapper_ReadLow(uint16 addr){	return 0;}  
void Mapper_WriteLow(uint16 addr,uint8 data){}
void Mapper_Write( uint16 addr , uint8 data){}
void Mapper_Read( uint8 data,uint16 addr ){}
void Mapper_HSync( int scanline ){}  
void Mapper_VSync(void)  {}	
	
void Mapper_Init(void)
{
	uint32 probe; 
	NES_Mapper->Reset         = Mapper_Reset;
	NES_Mapper->Write         = Mapper_Write;
	NES_Mapper->Read          = Mapper_Read;
	NES_Mapper->WriteLow      = Mapper_WriteLow;
	NES_Mapper->ReadLow       = Mapper_ReadLow;	
	NES_Mapper->HSync         = Mapper_HSync;
	NES_Mapper->VSync         = Mapper_VSync; 
	
    // 初始化时清除缓存记录，强制第一次必然加载
    for(int i=0; i<4; i++) current_prg_banks[i] = 0xFFFFFFFF;
    for(int i=0; i<8; i++) current_chr_banks[i] = 0xFFFFFFFF;

	VROM_mask = 0xFFFF; 
	for(probe = 0x8000; probe; probe >>= 1) {
		if((VROM_1K_SIZE-1) & probe) break;
		VROM_mask >>= 1;
	}

    uint32_t prg_8k_banks = RomHeader->num_16k_rom_banks * 2;
    PRG_ROM_mask = 0xFFFF;
    for(probe = 0x8000; probe; probe >>= 1) {
        if((prg_8k_banks - 1) & probe) break;
        PRG_ROM_mask >>= 1;
    }
	
	switch (MapperNo) {
		case 0 :MAP0_Init();break;  
		case 1 :MAP1_Init();break;   
		case 2 :MAP2_Init();break;
 		case 3 :MAP3_Init();break;  
		case 4 :MAP4_Init();break;   
		default:break;
	}
	NES_Mapper->Reset();
} 

void asm_Mapper_Write(uint8 byData,uint16 wAddr) { NES_Mapper->Write(wAddr,byData); }  
void asm_Mapper_ReadLow( uint16 wAddr) { NES_Mapper->ReadLow( wAddr); } 
void asm_Mapper_WriteLow( uint8 byData ,uint16 wAddr) { NES_Mapper->WriteLow(wAddr, byData ); }


//========================================================================
// CPU PRG-ROM 动态加载部分 
//========================================================================
void load_prg_bank_c(uint32_t slot, uint32_t bank_num) 
{
    uint32_t masked_bank = bank_num & PRG_ROM_mask; 

    // 【优化】如果请求的 bank 已经在缓存里了，直接跳过 SD 卡读取！
    if (current_prg_banks[slot] == masked_bank) {
        return;
    }
    current_prg_banks[slot] = masked_bank;
    
    UINT br;
    uint32_t offset = prg_file_offset + (masked_bank * 8192);
    
    f_lseek(nes_file, offset);
    f_read(nes_file, PRG_ROM_Cache + (slot * 8192), 8192, &br);
}

extern void map67_(signed char page);
extern void map89_(signed char page);  
extern void mapAB_(signed char page);
extern void mapCD_(signed char page);
extern void mapEF_(signed char page);

void set_CPU_bank3(signed char page )  { map67_(page) ;} 
void set_CPU_bank4(signed char page )  { map89_(page) ;} 
void set_CPU_bank5(signed char page )  { mapAB_(page) ;}
void set_CPU_bank6(signed char page )  { mapCD_(page) ;} 
void set_CPU_bank7(signed char page )  { mapEF_(page) ;}

void set_CPU_banks(int bank0_num,int bank1_num,int bank2_num, int bank3_num) {
    map89_(bank0_num); mapAB_(bank1_num); mapCD_(bank2_num); mapEF_(bank3_num);
}

//========================================================================
// PPU VROM 动态加载部分 
//========================================================================

// 【优化】同样加入防重复读取判断
#define LOAD_VROM_BANK(slot, bank_num) \
    do { \
        uint32_t real_bank = (bank_num) & VROM_mask; \
        if (real_bank < VROM_1K_SIZE) { \
            if (current_chr_banks[slot] != real_bank) { \
                current_chr_banks[slot] = real_bank; \
                UINT br; \
                f_lseek(nes_file, chr_file_offset + (real_bank * 1024)); \
                f_read(nes_file, VROM_banks + ((slot) * 1024), 1024, &br); \
            } \
            ppu->PPU_VRAM_banks[slot] = VROM_banks + ((slot) * 1024); \
            set_tile_bank(slot, VROM_banks + ((slot) * 1024)); \
        } \
    } while(0)

void set_PPU_banks(uint32 bank0_num, uint32 bank1_num,
				   uint32 bank2_num, uint32 bank3_num,
				   uint32 bank4_num, uint32 bank5_num,
				   uint32 bank6_num, uint32 bank7_num)
{
    LOAD_VROM_BANK(0, bank0_num);
    LOAD_VROM_BANK(1, bank1_num);
    LOAD_VROM_BANK(2, bank2_num);
    LOAD_VROM_BANK(3, bank3_num);
    LOAD_VROM_BANK(4, bank4_num);
    LOAD_VROM_BANK(5, bank5_num);
    LOAD_VROM_BANK(6, bank6_num);
    LOAD_VROM_BANK(7, bank7_num);
}

void set_PPU_bank0(uint32 bank_num) { LOAD_VROM_BANK(0, bank_num); }
void set_PPU_bank1(uint32 bank_num) { LOAD_VROM_BANK(1, bank_num); }
void set_PPU_bank2(uint32 bank_num) { LOAD_VROM_BANK(2, bank_num); }
void set_PPU_bank3(uint32 bank_num) { LOAD_VROM_BANK(3, bank_num); }
void set_PPU_bank4(uint32 bank_num) { LOAD_VROM_BANK(4, bank_num); } 
void set_PPU_bank5(uint32 bank_num) { LOAD_VROM_BANK(5, bank_num); }
void set_PPU_bank6(uint32 bank_num) { LOAD_VROM_BANK(6, bank_num); }
void set_PPU_bank7(uint32 bank_num) { LOAD_VROM_BANK(7, bank_num); }

void set_PPU_bank8(uint32 bank_num) {}
void set_PPU_bank9(uint32 bank_num) {}
void set_PPU_bank10(uint32 bank_num){}
void set_PPU_bank11(uint32 bank_num){}

void set_VRAM_bank(uint8 bank, uint32 bank_num)
{
	if(bank < 8) {        
        LOAD_VROM_BANK(bank, bank_num);
	} else if(bank < 12) {
		set_name_table(bank, bank_num);
	}
}
