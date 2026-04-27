//user file
#include "stm32f4xx.h"
#include "sdio_sdcard.h"
#include "w25q128.h"
#include "malloc.h"		  
#include "ff.h"
#include "string.h"
#include <ctype.h>

// 全局指针数组，最好初始化为NULL以防野指针
FATFS *fs[FF_VOLUMES] = {NULL};
FIL *file;	  				

/**
 * @brief  统一的 FatFs 挂载函数 (包含自动内存分配)
 * @param  pdrv: 0 - SD卡, 1 - U盘 (原外部Flash已删除)
 * @retval 0:成功, 1:失败
 */
uint8_t fatfs_mount(uint8_t pdrv)
{
    uint8_t res;
    // 动态生成挂载路径字符串，例如 "0:", "1:"
    char path[3] = {(char)(pdrv + '0'), ':', '\0'};

    if(pdrv >= FF_VOLUMES) return 1; // 越界保护

    // 1. 自动分配内存 (如果还未分配)
    if(fs[pdrv] == NULL)
    {
        fs[pdrv] = (FATFS*)malloc_bsc(sizeof(FATFS));
        if(fs[pdrv] == NULL) return 1; // 内存不足
    }

    // 2. 尝试挂载
    res = f_mount(fs[pdrv], path, 1); 	

    return (res == FR_OK) ? 0 : 1;
}

/**
 * @brief  统一的 FatFs 卸载函数 (包含自动内存释放)
 * @param  pdrv: 0 - SD卡, 1 - U盘 (原外部Flash已删除)
 * @retval 0:成功, 其他:失败
 */
uint8_t fatfs_unmount(uint8_t pdrv)
{
    uint8_t res;
    char path[3] = {(char)(pdrv + '0'), ':', '\0'};

    if(pdrv >= FF_VOLUMES) return 1; // 越界保护

    // 1. 卸载文件系统
    res = f_mount(NULL, path, 0);

    // 2. 自动释放内存
    if(fs[pdrv] != NULL)
    {
        free_bsc(fs[pdrv]);
        fs[pdrv] = NULL; // 必须置空，防止下次 mount 时重复使用野指针
    }

    return res;
}

// ================= 保留原来的函数 =================

// 计算容量  "0:" "1:" 总容量 剩余容量
uint8_t fatfs_get_space(const char *drv, uint16_t *total, uint16_t *free)
{
	FATFS *fs1;
	uint8_t res;
    uint32_t fre_clust = 0, fre_sect = 0, tot_sect = 0;
    res = (uint32_t)f_getfree((const char*)drv, (uint32_t*)&fre_clust, &fs1);
    if(res == 0)
	{											   
	    tot_sect = (fs1->n_fatent - 2) * fs1->csize;
	    fre_sect = fre_clust * fs1->csize;		 
		*total = (uint16_t)(tot_sect >> 11); // >>11 相当于除以 2048 (针对512字节扇区转换为MB)
		*free  = (uint16_t)(fre_sect >> 11);
 	}
	return res;
}

/**
 * @brief 从路径或文件名中提取文件扩展名（后缀）
 * @param path 文件路径（可包含目录）或纯文件名
 * @return 扩展名字符串（小写），若没有扩展名则返回空字符串
 */
uint8_t* fatfs_get_extension(const char *path)
{
    static uint8_t ext[8];  // 最多7字符后缀 + '\0'
    memset(ext, 0, sizeof(ext));
    
    if (path == NULL) return ext;
    
    // 1. 提取文件名（最后一个 '/' 或 '\\' 之后的部分）
    const char *fname = path;
    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }
    if (last_sep != NULL) {
        fname = last_sep + 1;
    }
    
    // 2. 查找最后一个 '.'
    const char *dot = strrchr(fname, '.');
    if (dot && dot != fname && *(dot+1) != '\0') {
        const char *ext_start = dot + 1;
        size_t ext_len = strlen(ext_start);
        size_t copy_len = ext_len > 7 ? 7 : ext_len;  // 最多7字符
        
        for (size_t i = 0; i < copy_len; i++) {
            ext[i] = (uint8_t)tolower(ext_start[i]);
        }
        ext[copy_len] = '\0';
    }
    return ext;
}
