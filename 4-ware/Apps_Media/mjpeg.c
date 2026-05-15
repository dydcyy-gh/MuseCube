#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include "tjpgd.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include <string.h>
#include "lvgl.h"
#include "variables.h"
#include "defines.h"
#include "debug.h"

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define BUF_LINES  48
#define WORKBUF_SIZE 4096   // TJpgDec 工作缓冲区大小

static FIL fil;                     // 文件对象
static uint8_t *workbuf = NULL;     // TJpgDec 工作内存池

// --- 双缓冲相关变量 ---
static uint16_t *line_buf[2] = {NULL, NULL}; // 双缓冲区
static uint8_t write_idx = 0;                // 当前CPU正在写入的缓冲区索引 (0 或 1)
static uint8_t dma_busy = 0;                 // 标记DMA是否正在传输

static uint16_t buf_start_y = 0;     // 缓冲区当前起始行
static uint16_t buf_filled_lines = 0;// 缓冲区有效行数
static int file_opened = 0;          // 文件是否已打开
static int initialized = 0;

// TJpgDec 输入回调：从文件读取数据
static size_t in_func(JDEC *jd, uint8_t *buff, size_t nbyte) {
    FIL *fp = (FIL*)jd->device;
    UINT br;
    if (buff) {
        if (f_read(fp, buff, nbyte, &br) != FR_OK) return 0;
        return br;
    } else {
        if (f_lseek(fp, f_tell(fp) + nbyte) != FR_OK) return 0;
        return nbyte;
    }
}

// 刷新行缓冲区到 LCD (双缓冲核心逻辑)
static void flush_line_buffer(void) {
    if (buf_filled_lines == 0) return;

    // 1. 如果上次的 DMA 还在传输，必须等待它完成，防止覆盖数据
    if (dma_busy) {
        xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
        dma_busy = 0;
    }

    // 2. 启动新的 DMA 传输，发送当前已填满的缓冲区
    LCD_Color_Fill(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled_lines - 1, line_buf[write_idx]);
    dma_busy = 1; // 标记 DMA 正在忙

    // 3. 更新坐标并切换缓冲区，让 CPU 立即继续解码，不阻塞！
    buf_start_y += buf_filled_lines;
    buf_filled_lines = 0;
    write_idx ^= 1; // 切换索引 (0变1, 1变0)
}

// TJpgDec 输出回调：将矩形数据写入行缓冲区
static int out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    uint16_t *src = (uint16_t*)bitmap;   // RGB565 数据
    uint16_t left = rect->left;
    uint16_t right = rect->right;
    uint16_t top = rect->top;
    uint16_t bottom = rect->bottom;

    for (uint16_t y = top; y <= bottom; y++) {
        // 确保当前行在行缓冲区内，否则触发双缓冲刷新
        while (y >= buf_start_y + BUF_LINES) {
            flush_line_buffer();
        }
        int row_in_buf = y - buf_start_y;
        if (row_in_buf < 0) continue;    // 防御

        // 注意：这里写入的是当前活动的缓冲区 line_buf[write_idx]
        uint16_t *dst = &line_buf[write_idx][row_in_buf * LCD_WIDTH + left];
        uint16_t width = right - left + 1;
        memcpy(dst, src, width * sizeof(uint16_t));

        // 更新有效行数
        if (row_in_buf + 1 > buf_filled_lines) {
            buf_filled_lines = row_in_buf + 1;
        }

        src += width;   // 移动到下一行
    }
    return 1;   // 继续解码
}

// 辅助函数：在视频流中寻找下一帧的 JPEG 头 (0xFF 0xD8)
static uint8_t find_next_jpeg_frame(FIL *fp) {
    uint8_t buf[256];
    UINT br;
    uint8_t last_byte = 0;
    uint32_t current_pos = f_tell(fp);

    while (1) {
        if (f_read(fp, buf, sizeof(buf), &br) != FR_OK || br == 0) {
            return 0; // 文件结束或读取错误
        }
        for (UINT i = 0; i < br; i++) {
            if (last_byte == 0xFF && buf[i] == 0xD8) {
                f_lseek(fp, current_pos + i - 1);
                return 1;
            }
            last_byte = buf[i];
        }
        current_pos += br;
    }
}

// 初始化：打开文件，分配双重内存
uint8_t video_mjpeg_play_init(const char *file)
{
    if (initialized) return 0;
	
    // 3. 打开文件
    if (f_open(&fil, file, FA_READ) != FR_OK) return 1;
    file_opened = 1;

    // 4. 分配工作缓冲区
    workbuf = (uint8_t*)malloc_bsc(WORKBUF_SIZE);
    if (!workbuf) return 1;

    // 5. 分配双行缓冲区
    line_buf[0] = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
    line_buf[1] = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
    if (!line_buf[0] || !line_buf[1]) return 1;

    buf_start_y = 0;
    buf_filled_lines = 0;
    write_idx = 0;
    dma_busy = 0;
    initialized = 1;
    return 0;
}

// 解码并显示下一帧
uint8_t video_mjpeg_play_task(void) {
    if (!initialized) return 1;

    // 寻找下一个 JPEG 帧
    if (!find_next_jpeg_frame(&fil)) {
        f_lseek(&fil, 0);
        if (!find_next_jpeg_frame(&fil)) return 2;
    }

    JDEC jdec;
    jdec.pool = workbuf;
    jdec.sz_pool = WORKBUF_SIZE;

    JRESULT res = jd_prepare(&jdec, in_func, workbuf, WORKBUF_SIZE, &fil);
    if (res != JDR_OK) {
        f_lseek(&fil, f_tell(&fil) + 2);
        return 0; 
    }

    // 每帧开始前重置状态
    buf_start_y = 0;
    buf_filled_lines = 0;

    res = jd_decomp(&jdec, out_func, 0);
    if (res != JDR_OK) {
        return 0; 
    }

    // 刷新该帧剩余的最后几行
    if (buf_filled_lines > 0) {
        flush_line_buffer();
    }

    // --- 关键帧同步 ---
    // 确保当前帧的最后一个 DMA 传输完成，再退出任务
    // 防止下一帧立马开始解码覆盖了还在传输的缓冲区
    if (dma_busy) {
        xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
        dma_busy = 0;
    }

    return 0;
}

// 反初始化：释放资源
void video_mjpeg_play_deinit(void) 
{
    if (!initialized) return;

    // 退出前确保没有未完成的DMA传输
    if (dma_busy) {
        xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
        dma_busy = 0;
    }

    if (line_buf[0]) { free_bsc(line_buf[0]); line_buf[0] = NULL; }
    if (line_buf[1]) { free_bsc(line_buf[1]); line_buf[1] = NULL; }
    if (workbuf) { free_bsc(workbuf); workbuf = NULL; }
    if (file_opened) { f_close(&fil); file_opened = 0; }
    
    initialized = 0;
}
