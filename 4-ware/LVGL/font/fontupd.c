#include "fontupd.h"
#include "ff.h"	  
#include "w25q128.h"   
#include "lcd_bsp.h"  
#include "string.h"
#include "malloc.h"
#include "systick_conf.h"

//lvgl字库读取缓冲区
uint8_t __g_font_buf[210]; //162

// 字库配置结构
typedef struct {
    char* path;        			
    uint8_t type;      			
    __packed uint32_t *addr;    
    __packed uint32_t *size;    
} FontConfig;

#define FONTSECSIZE     (1024 * 1024 * 7)   // 字库占用空间大小 7MB
#define FONTINFOADDR    (1024 * 1024 * 8)   // 字库存储起始地址 8MB
#define Checksum        0xAA

_font_info ftinfo;

// 全局变量区
static const FontConfig fontConfigs[] = {
    {"/FONT/UNIGBK.bin",  0, &ftinfo.unigbkaddr,   &ftinfo.unigbksize},
    {"/FONT/Font8.bin",   1, &ftinfo.font8addr,    &ftinfo.font8size},
    {"/FONT/Font12.bin",  2, &ftinfo.font12addr,   &ftinfo.font12size},
	{"/FONT/Font16.bin",  3, &ftinfo.font16addr,   &ftinfo.font16size},
	{"/FONT/Font24.bin",  4, &ftinfo.font24addr,   &ftinfo.font24size},
};

#define FONT_CONFIG_COUNT (sizeof(fontConfigs) / sizeof(fontConfigs[0]))

// 内部使用的单文件更新函数
// 增加 start_addr 参数，显式传入写入地址，解耦对前一个文件的依赖
static uint8_t updata_fontx(uint8_t *fxpath, uint8_t fx, uint32_t start_addr)
{
    uint32_t flashaddr = start_addr;
    FIL *fftemp = NULL;
    uint8_t *tempbuf = NULL;
    uint8_t res;
    uint8_t ret = 0;
    uint16_t bread;
    uint32_t offx = 0;
    FILINFO fileInfo;

    fftemp = (FIL *)malloc_bsc(sizeof(FIL));
    tempbuf = malloc_bsc(4096);
    
    if (!fftemp || !tempbuf) {
        ret = 1; 
        goto __exit; // 统一出口，防止内存泄漏
    }

    res = f_open(fftemp, (const char *)fxpath, FA_READ);
    if (res != FR_OK) {
        ret = 2;
        goto __exit;
    }

    res = f_stat((const char *)fxpath, &fileInfo);
    if (res != FR_OK) {
        ret = 3;
        goto __exit;
    }
	
    // 更新结构体信息
    if (fx < FONT_CONFIG_COUNT) {
        *(fontConfigs[fx].addr) = flashaddr;
        *(fontConfigs[fx].size) = fileInfo.fsize;
    } else {
        ret = 4;
        goto __exit;
    }

    // 循环写入
    while (1) {
        res = f_read(fftemp, tempbuf, 4096, (uint32_t *)&bread);
        if (res != FR_OK) {
            ret = 5;
            break;
        }
        // 写入 Flash
        W25QXX_Write(tempbuf, offx + flashaddr, bread);
        offx += bread;
        if (bread != 4096) break;
    }
    
    f_close(fftemp);

__exit:
    if (fftemp) free_bsc(fftemp);
    if (tempbuf) free_bsc(tempbuf);
    return ret;
}

// 更新所有字体
uint8_t update_font(uint8_t* src)
{
    uint8_t ret = 0;
    uint8_t *pname = NULL;
    FIL *fftemp = NULL;
    uint8_t res;
    uint32_t current_flash_addr = FONTINFOADDR + sizeof(ftinfo); // 起始地址

    pname = malloc_bsc(100);
    fftemp = (FIL*)malloc_bsc(sizeof(FIL));
    
    if (!pname || !fftemp) {
        ret = 1;
        goto __error;
    }

    // 1. 预检查：所有文件必须都存在，否则不开始擦除
    for (uint8_t idx = 0; idx < FONT_CONFIG_COUNT; idx++) {
        strcpy((char*)pname, (char*)src);
        strcat((char*)pname, fontConfigs[idx].path);
        res = f_open(fftemp, (const char*)pname, FA_READ);
        if (res != FR_OK) {
            ret = 10 + idx;
            goto __error;
        }
        f_close(fftemp);
    }

	// 2. 擦除区域：从 FONTINFOADDR 开始的连续 FONTSECSIZE 字节（7MB）
	for (uint32_t addr = FONTINFOADDR; addr < FONTINFOADDR + FONTSECSIZE; addr += 4096) 
	{
		W25QXX_Erase_Sector(addr);
	}

    // 3. 写入文件
    for (uint8_t idx = 0; idx < FONT_CONFIG_COUNT; idx++) {
        strcpy((char*)pname, (char*)src);
        strcat((char*)pname, fontConfigs[idx].path);
        
        // 传入当前计算好的地址
        res = updata_fontx(pname, idx, current_flash_addr);
        if (res) {
            ret = 20 + idx; // 20+: 写入失败错误码
            goto __error;
        }
        
        // 计算下一个文件的起始地址 = 当前地址 + 当前文件大小
        // 必须确保 current_flash_addr 按 4字节或更优的方式对齐，虽然 W25Q 不强制，但便于管理
        current_flash_addr += *(fontConfigs[idx].size);
    }

    // 4. 写入头部信息
    ftinfo.fontok = Checksum;
    W25QXX_Write((uint8_t*)&ftinfo, FONTINFOADDR, sizeof(ftinfo));

__error:
    if (pname) free_bsc(pname);
    if (fftemp) free_bsc(fftemp);

    return ret;
}

//初始化字体
//返回值:0,字库完好.其他,字库丢失
uint8_t font_init(void)
{	
    uint8_t t = 0;
    while (t < 3) 
	{
        t++;
        W25QXX_Read((uint8_t*)&ftinfo, FONTINFOADDR, sizeof(ftinfo));
        if (ftinfo.fontok == Checksum) break;
        Delay_ms(20);
    }
    return (ftinfo.fontok != Checksum) ? 1 : 0;
}
