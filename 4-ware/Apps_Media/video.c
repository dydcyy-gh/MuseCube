#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include <string.h>
#include "malloc.h"
#include "lcd_bsp.h"
#include "variables.h"
#include "defines.h"
#include "lvgl.h"
#include "fatfs.h"

// 视频参数
#define LCD_WIDTH       240
#define LCD_HEIGHT      240
#define BYTES_PER_PIXEL 2
#define FRAME_SIZE      (LCD_WIDTH * LCD_HEIGHT * BYTES_PER_PIXEL)  // 115200字节
#define LINE_SIZE       (LCD_WIDTH * BYTES_PER_PIXEL)               // 480字节

// 分块参数：每块60行，共4块
#define BLOCK_ROWS        60
#define BLOCK_SIZE       (LINE_SIZE * BLOCK_ROWS)  // 19200字节
#define BLOCKS_PER_FRAME (LCD_HEIGHT / BLOCK_ROWS) // 6

// 模块内部状态
static FIL fil;
static uint8_t *buf[2] = {NULL, NULL};
static uint32_t file_size = 0;
static uint32_t current_frame_start = 0;
static int block_idx = 0;
static int send_buf = 0;
static int fill_buf = 1;
static int initialized = 0;

// 初始化视频播放模块（接收文件路径）
uint8_t video_play_init(const char *file)
{
    FRESULT fr;
    UINT br;

    if (initialized) return 0;

    // 3. 打开文件
	if (f_open(&fil, file, FA_READ) != FR_OK) return 1;

    file_size = f_size(&fil);

    // 4. 分配双缓冲
    buf[0] = (uint8_t*)malloc_bsc(BLOCK_SIZE);
    buf[1] = (uint8_t*)malloc_bsc(BLOCK_SIZE);
    if (buf[0] == NULL || buf[1] == NULL) return 1;

    // 5. 初始化状态
    current_frame_start = 0;
    block_idx = 0;
    send_buf = 0;
    fill_buf = 1;

    // 读取第一块到发送缓冲区，启动 DMA
    fr = f_lseek(&fil, current_frame_start);
    if (fr != FR_OK) return 1;
    fr = f_read(&fil, buf[send_buf], BLOCK_SIZE, &br);
    if (fr != FR_OK || br != BLOCK_SIZE) return 1;

    LCD_Address_Set(0, 0, LCD_WIDTH - 1, BLOCK_ROWS - 1);
    LCD_Write_DMA((uint16_t*)buf[send_buf], BLOCK_SIZE / BYTES_PER_PIXEL);

    // 预读第二块
    fr = f_read(&fil, buf[fill_buf], BLOCK_SIZE, &br);
    if (fr != FR_OK || br != BLOCK_SIZE) return 1;

    initialized = 1;
    return 0;
}

uint8_t video_play_task(void) 
{
    FRESULT fr;
    UINT br;
    int next_block_idx;

    if (!initialized) return 1;
    xEventGroupWaitBits(xLcdEventGroup,LCD_USER_VDEO,pdTRUE,pdFALSE,portMAX_DELAY);
	
    if (block_idx == BLOCKS_PER_FRAME - 1) 
    {
        // 当前帧的最后一块完成，准备下一帧
        current_frame_start += FRAME_SIZE;
        if (current_frame_start + FRAME_SIZE > file_size) {
            current_frame_start = 0; // 文件回绕
        }

        // 下一帧的第一块索引为0
        next_block_idx = 0;

        // 交换缓冲区：填充缓冲区中已预读下一帧的第一块，现在让它成为发送缓冲区
        int tmp = send_buf;
        send_buf = fill_buf;
        fill_buf = tmp;

        // 启动新帧第一块的DMA
        LCD_Address_Set(0, 0, LCD_WIDTH - 1, BLOCK_ROWS - 1);
        LCD_Write_DMA((uint16_t*)buf[send_buf], BLOCK_SIZE / BYTES_PER_PIXEL);

        // 预读新帧的第二块到新的填充缓冲区（块索引1）
        // 此时文件指针位于下一帧第一块之后（即第二块起始），可直接f_read
        fr = f_read(&fil, buf[fill_buf], BLOCK_SIZE, &br);
        if (fr != FR_OK || br != BLOCK_SIZE) return 1;

        // 更新当前发送块索引为新帧的第一块
        block_idx = 0;
    } 
    else 
    {
        // 不是最后一块，继续当前帧的下一块
        next_block_idx = block_idx + 1;

        // 交换缓冲区：填充缓冲区中已预读下一块，让它成为发送缓冲区
        int tmp = send_buf;
        send_buf = fill_buf;
        fill_buf = tmp;

        // 启动下一块的DMA
        uint16_t start_row = next_block_idx * BLOCK_ROWS;
        uint16_t end_row = start_row + BLOCK_ROWS - 1;
        LCD_Address_Set(0, start_row, LCD_WIDTH - 1, end_row);
        LCD_Write_DMA((uint16_t*)buf[send_buf], BLOCK_SIZE / BYTES_PER_PIXEL);

        // 根据即将发送的块索引预读再下一块
        if (next_block_idx < BLOCKS_PER_FRAME - 1) 
        {
            // 还在当前帧内：文件指针已自动指向下一个块，直接f_read
            fr = f_read(&fil, buf[fill_buf], BLOCK_SIZE, &br);
            if (fr != FR_OK || br != BLOCK_SIZE) return 1;
        } 
        else if (next_block_idx == BLOCKS_PER_FRAME - 1) 
        {
            // 即将发送的是当前帧的最后一块，预读下一帧的第一块
            uint32_t next_frame_start = current_frame_start + FRAME_SIZE;
            if (next_frame_start + FRAME_SIZE > file_size) {
                next_frame_start = 0;
            }
            // 需要跳转到下一帧起始
            fr = f_lseek(&fil, next_frame_start);
            if (fr != FR_OK) return 1;
            fr = f_read(&fil, buf[fill_buf], BLOCK_SIZE, &br);
            if (fr != FR_OK || br != BLOCK_SIZE) return 1;
        }

        // 更新当前发送块索引
        block_idx = next_block_idx;
    }
    return 0;
}

// 反初始化视频播放模块
void video_play_deinit(void) 
{
    if (!initialized) return;

    f_close(&fil);

    if (buf[0]) {
        free_bsc(buf[0]);
        buf[0] = NULL;
    }
    if (buf[1]) {
        free_bsc(buf[1]);
        buf[1] = NULL;
    }

    initialized = 0;
}
