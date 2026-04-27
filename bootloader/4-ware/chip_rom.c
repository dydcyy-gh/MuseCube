#include "stm32f4xx.h"
#include <stddef.h>

//Bootloader 占 64KB 
#define APP_START_ADDRESS 0x08010000

static uint16_t erased_sectors_bitmap = 0;

// 根据物理地址计算出所属的 STM32F4 扇区 (1MB Flash)
static uint32_t GetSector(uint32_t Address)
{
    if((Address < 0x08004000)) return FLASH_Sector_0;   // 16 KB
    else if((Address < 0x08008000)) return FLASH_Sector_1;   // 16 KB
    else if((Address < 0x0800C000)) return FLASH_Sector_2;   // 16 KB
    else if((Address < 0x08010000)) return FLASH_Sector_3;   // 16 KB
    else if((Address < 0x08020000)) return FLASH_Sector_4;   // 64 KB
    else if((Address < 0x08040000)) return FLASH_Sector_5;   // 128 KB
    else if((Address < 0x08060000)) return FLASH_Sector_6;   // 128 KB
    else if((Address < 0x08080000)) return FLASH_Sector_7;   // 128 KB
    else if((Address < 0x080A0000)) return FLASH_Sector_8;   // 128 KB
    else if((Address < 0x080C0000)) return FLASH_Sector_9;   // 128 KB
    else if((Address < 0x080E0000)) return FLASH_Sector_10;  // 128 KB
    else return FLASH_Sector_11;                             // 128 KB
}

// 辅助函数：根据扇区宏定义获取 0~11 的索引，方便操作位图
static uint8_t GetSectorIndex(uint32_t sector) {
    if(sector == FLASH_Sector_0) return 0;
    if(sector == FLASH_Sector_1) return 1;
    if(sector == FLASH_Sector_2) return 2;
    if(sector == FLASH_Sector_3) return 3;
    if(sector == FLASH_Sector_4) return 4;
    if(sector == FLASH_Sector_5) return 5;
    if(sector == FLASH_Sector_6) return 6;
    if(sector == FLASH_Sector_7) return 7;
    if(sector == FLASH_Sector_8) return 8;
    if(sector == FLASH_Sector_9) return 9;
    if(sector == FLASH_Sector_10) return 10;
    return 11;
}

void boot2uf2_flash_init(void)
{
    erased_sectors_bitmap = 0; // 每次初始化清空位图
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    FLASH_Lock();
}

int bootuf2_flash_write(uint32_t address, const uint8_t *data, size_t size)
{
    if (address < APP_START_ADDRESS) {
        return -1; // 拒绝写入 Bootloader 区域
    }

    uint32_t current_sector = GetSector(address);
    uint8_t sector_index = GetSectorIndex(current_sector);

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 如果这个扇区在本次升级中还没有被擦除过，则擦除它
    if ((erased_sectors_bitmap & (1 << sector_index)) == 0) {
        FLASH_EraseSector(current_sector, VoltageRange_3);
        erased_sectors_bitmap |= (1 << sector_index); // 标记为已擦除
    }

    // 后续的 FLASH_ProgramWord 写入逻辑保持不变 ...
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word_data = (uint32_t)data[i] | ((uint32_t)data[i+1] << 8) | 
                             ((uint32_t)data[i+2] << 16) | ((uint32_t)data[i+3] << 24);
        if (FLASH_ProgramWord(address + i, word_data) != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1; 
        }
    }
    
    FLASH_Lock();
    return 0;
}
