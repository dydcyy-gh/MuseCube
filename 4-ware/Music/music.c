#include "stm32f4xx.h"
#include "ff.h"
#include "malloc.h"
#include "fatfs.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "wav.h"
#include "mp3.h"
#include "flac.h"
#include "ape.h"
#include "aac.h"
#include "music.h"
#include "rng.h"
#include "es9018k2m.h"
#include "FreeRTOS.h"
#include "task.h"
#include "variables.h"
#include "defines.h"

// 文件类型定义
#define FORMAT_WAV  1
#define FORMAT_MP3  2
#define FORMAT_FLAC 3
#define FORMAT_AAC  4
#define FORMAT_APE  5

#define MUSIC_PATH_PREFIX "0:/MUSIC"

#define NAME_LEN_MAX 64

// 控制参数
__musicctrl music_ctrl;

//MUSIC参数
__musicinfo music_info;

// 文件管理
uint8_t *pname = NULL;
uint16_t total_files = 0;
uint16_t current_index = 0;
uint8_t current_format = 0;

static char display_name_buf[NAME_LEN_MAX];

// [优化] 不区分大小写的后缀比较
uint8_t get_file_format(char* ext)
{
    char ext_lower[5];
    int i;
    for(i=0; i<4 && ext[i]; i++) {
        ext_lower[i] = tolower(ext[i]);
    }
    ext_lower[i] = 0;

    if(strcmp(ext_lower, "wav") == 0) return FORMAT_WAV;
    if(strcmp(ext_lower, "mp3") == 0) return FORMAT_MP3;
    if(strcmp(ext_lower, "flac")== 0) return FORMAT_FLAC;
    if(strcmp(ext_lower, "aac") == 0) return FORMAT_AAC;
    if(strcmp(ext_lower, "ape") == 0) return FORMAT_APE;
    return 0;
}

// 辅助函数：去掉文件名后缀并更新显示缓冲区
static void update_display_name(char* fname)
{
    strncpy(display_name_buf, fname, NAME_LEN_MAX - 1);
    display_name_buf[NAME_LEN_MAX - 1] = '\0';
    char *dot = strrchr(display_name_buf, '.');
    if (dot) *dot = '\0'; // 截断
}

// 核心辅助函数：根据索引获取文件详细信息
static uint8_t scan_music_dir(uint16_t index_to_find, FILINFO* info_out, uint8_t* format_out)
{
    DIR music_dir;
    FILINFO file_info;
    FRESULT res;
    uint16_t count = 0;
    uint8_t found = 0;

    res = f_opendir(&music_dir, MUSIC_PATH_PREFIX);
    if (res != FR_OK) return 0;

    while (1) 
    {
        res = f_readdir(&music_dir, &file_info);
        if (res != FR_OK || file_info.fname[0] == 0) break;
        
        if (file_info.fattrib & (AM_DIR | AM_HID)) continue;

        uint8_t* ext = fatfs_get_extension(file_info.fname);
        uint8_t fmt = get_file_format((char*)ext);
        
        if (fmt != 0) 
        {
            if (count == index_to_find) 
            {
                if (info_out) memcpy(info_out, &file_info, sizeof(FILINFO));
                if (format_out) *format_out = fmt;
                found = 1;
                break; 
            }
            count++;
        }
    }
    f_closedir(&music_dir);
    
    return found; 
}

// 辅助函数：更新当前播放路径及 info 结构体
static void update_current_song_info(void)
{
    FILINFO temp_info;
    
    // 查找当前索引的文件信息
    if (scan_music_dir(current_index, &temp_info, &current_format))
    {
        // 1. 更新播放路径 pname
        // 假设 pname 已分配足够空间
        sprintf((char*)pname, "%s/%s", MUSIC_PATH_PREFIX, temp_info.fname);
        
        // 2. 更新显示名称
        update_display_name(temp_info.fname);
        
        // 3. 填充 music_info 结构体
        music_info.file_format = current_format;
        music_info.music_name = display_name_buf;
        music_info.file_date = temp_info.fdate;
        music_info.file_time = temp_info.ftime;
        
        // 重置动态参数
        music_info.artist_name = NULL; 
        music_info.total_sec = 0;
        music_info.current_sec = 0;
        music_info.bitrate = 0;
        music_info.samplerate = 0;
        music_info.bit_depth = 0;
    }
    else
    {
        current_format = 0;
    }
}

void play_next_song(void)
{
    if (total_files == 0) return;
	current_index = (current_index == total_files-1) ? 0 : (current_index+1);
    update_current_song_info();
}

void play_previous_song(void)
{
    if (total_files == 0) return;
	current_index = (current_index == 0) ? (total_files-1) : (current_index-1);
    update_current_song_info();
}

void play_same_song(void)
{
    // 如果文件被删除了，f_open 会失败，那是播放任务处理的事。
    if (total_files == 0 || pname == NULL) return;
    music_info.current_sec = 0;
}

void play_random_song(void)
{
    uint16_t old_index = current_index;
    uint16_t new_index;
    
    if (total_files == 0) return;

    if (total_files == 1) new_index = 0;
	else 
	{
        do {new_index = RNG_GetRandomRange(0, total_files - 1);} 
		while (new_index == old_index && total_files > 1);
    }
    current_index = new_index;
    update_current_song_info();
}

// 播放指定路径的歌曲 (例如: "0:/MUSIC/AAA.mp3")
// 返回值: 0 成功找到并切换, 1 失败(未找到该文件)
uint8_t play_specific_song(const char* filepath)
{
    DIR music_dir;
    FILINFO file_info;
    FRESULT res;
    uint16_t count = 0;
    uint8_t found = 0;
    
    // 如果系统尚未统计过总数，可以先调用 prepare 预防出错
    if (total_files == 0) return 0;

    // 1. 从完整路径中提取文件名 (去掉 "0:/MUSIC/" 等前缀)
    const char *slash = strrchr(filepath, '/');
    const char *fname_to_find = slash ? (slash + 1) : filepath;

    // 2. 扫描目录，寻找匹配的文件名以获取它的 current_index
    res = f_opendir(&music_dir, MUSIC_PATH_PREFIX);
    if (res != FR_OK) return 0;

    while (1) 
    {
        res = f_readdir(&music_dir, &file_info);
        if (res != FR_OK || file_info.fname[0] == 0) break;
        
        if (file_info.fattrib & (AM_DIR | AM_HID)) continue;

        uint8_t* ext = fatfs_get_extension(file_info.fname);
        uint8_t fmt = get_file_format((char*)ext);
        
        if (fmt != 0) 
        {
            // 比较文件名是否与目标一致
            if (strcmp(file_info.fname, fname_to_find) == 0) 
            {
                // --------- 找到了目标文件，更新系统状态 ---------
                current_index = count;
                current_format = fmt;
                
                // 更新播放路径 pname
                if (pname != NULL) {
                    strcpy((char*)pname, filepath);
                }
                
                // 更新显示名称
                update_display_name(file_info.fname);
                
                // 填充 music_info 结构体
                music_info.file_format = current_format;
                music_info.music_name = display_name_buf;
                music_info.file_date = file_info.fdate;
                music_info.file_time = file_info.ftime;
                
                // 重置动态参数
                music_info.artist_name = NULL; 
                music_info.total_sec = 0;
                music_info.current_sec = 0;
                music_info.bitrate = 0;
                music_info.samplerate = 0;
                music_info.bit_depth = 0;

                found = 1;
                break; 
            }
            count++;
        }
    }
    f_closedir(&music_dir);
    
    return (!found);
}

// [核心优化] 整合了 统计文件 和 加载第一首歌 的逻辑
uint8_t audio_play_prepare(void) 
{
    DIR music_dir;
    FILINFO file_info;
    FRESULT res;
    uint32_t buf_size;
	
    total_files = 0; 
    current_format = 0;
    current_index = 0;

    if(pname == NULL)
    {
        buf_size = strlen(MUSIC_PATH_PREFIX) + 1 + FF_MAX_LFN + 1;
        pname = malloc_bsc(buf_size);
        if(pname == NULL) return 1;
    }

    res = f_opendir(&music_dir, MUSIC_PATH_PREFIX);
    if(res == FR_OK) 
	{
        while(1) 
		{
            res = f_readdir(&music_dir, &file_info);
            if(res != FR_OK || file_info.fname[0] == 0) break;
            
            if (file_info.fattrib & (AM_DIR | AM_HID)) continue; // 跳过目录

            uint8_t* ext = fatfs_get_extension(file_info.fname);
            uint8_t fmt = get_file_format((char*)ext);
            
            if(fmt != 0) 
            {
                if(total_files == 0)
                {
                    sprintf((char*)pname, "%s/%s", MUSIC_PATH_PREFIX, file_info.fname);
                    update_display_name(file_info.fname);
                    
                    current_format = fmt;
                    music_info.file_format = fmt;
                    music_info.music_name = display_name_buf;
                    music_info.file_date = file_info.fdate;
                    music_info.file_time = file_info.ftime;
                    
                    // 重置其他信息
                    music_info.artist_name = NULL; 
                    music_info.total_sec = 0;
                    music_info.current_sec = 0;
                    music_info.bitrate = 0;
                    music_info.samplerate = 0;
                    music_info.bit_depth = 0;
                }
                total_files++;
            }
        }
        f_closedir(&music_dir);
    }
	
    if(total_files > 0) return 0;
    
    audio_play_clear();
    return 2; // 无文件
}

void audio_play_task(void) 
{
    if (pname == NULL) return; 

    switch(current_format) 
	{
        case FORMAT_WAV:
            wav_play_song_task(pname);
            break;
        case FORMAT_MP3:
            mp3_play_song_task(pname);
            break;
        case FORMAT_FLAC:
			flac_play_song_task(pname);
            break;
        case FORMAT_AAC:
			aac_play_song_task(pname);
            break;
		case FORMAT_APE:
			ape_play_song_task(pname);
            break;
        default:
            break;
    }
}

void audio_play_clear(void) 
{
	if(pname) 
	{
        free_bsc(pname);
        pname = NULL;
    }
    music_info.music_name = NULL;
    total_files = 0;
    current_index = 0;
    current_format = 0;
}
