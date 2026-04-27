#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include "tjpgd.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include <string.h>
#include "task_manager.h"
#include "lvgl.h"
#include "variables.h"
#include "defines.h"
#include "debug.h"

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define BUF_LINES  16
#define WORKBUF_SIZE 4096

static FIL fil;                     // 文件对象
static uint8_t *workbuf = NULL;     // TJpgDec 工作内存池
static uint16_t *line_buf = NULL;   // 行缓冲区 (RGB565)
static uint16_t buf_start_y = 0;    // 缓冲区当前起始行
static uint16_t buf_filled_lines = 0; // 缓冲区有效行数
static int file_opened = 0;          // 文件是否已打开
static int16_t g_offset_x = 0;       // 居中X偏移
static int16_t g_offset_y = 0;       // 居中Y偏移

extern void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
extern void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count);

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

// 刷新行缓冲区到 LCD
static void flush_line_buffer(void) {
    if (buf_filled_lines == 0) return;
    LCD_Address_Set(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled_lines - 1);
    LCD_Write_DMA(line_buf, LCD_WIDTH * buf_filled_lines);
    xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
    buf_filled_lines = 0;
    memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
}

// TJpgDec 输出回调：将矩形数据写入行缓冲区（支持平移与裁剪）
static int out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    uint16_t *src = (uint16_t*)bitmap;
    int left = rect->left + g_offset_x;
    int right = rect->right + g_offset_x;
    int top = rect->top + g_offset_y;
    int bottom = rect->bottom + g_offset_y;

    if (right < 0 || left >= LCD_WIDTH || bottom < 0 || top >= LCD_HEIGHT)
        return 1;

    int crop_left   = (left < 0) ? 0 : left;
    int crop_right  = (right >= LCD_WIDTH) ? LCD_WIDTH - 1 : right;
    int crop_top    = (top < 0) ? 0 : top;
    int crop_bottom = (bottom >= LCD_HEIGHT) ? LCD_HEIGHT - 1 : bottom;

    if (buf_filled_lines > 0 && crop_bottom >= buf_start_y + BUF_LINES) {
        flush_line_buffer();
    }

    if (buf_filled_lines == 0) {
        buf_start_y = crop_top;
    }

    for (int y = crop_top; y <= crop_bottom; y++) {
        int row_in_buf = y - buf_start_y;
        if (row_in_buf < 0 || row_in_buf >= BUF_LINES) continue;

        uint16_t src_y = rect->top + (y - top);
        uint16_t src_width = rect->right - rect->left + 1;
        uint16_t *src_row = src + (src_y - rect->top) * src_width + (crop_left - left);
        uint16_t *dst = &line_buf[row_in_buf * LCD_WIDTH + crop_left];
        uint16_t width = crop_right - crop_left + 1;

        memcpy(dst, src_row, width * sizeof(uint16_t));

        if (row_in_buf + 1 > buf_filled_lines) {
            buf_filled_lines = row_in_buf + 1;
        }
    }
    return 1;
}

/**
 * @brief 解码并显示一张 JPEG 图片（支持自动缩放与居中）
 * @param path 图片文件路径（如 "0:/VIDEO/image.jpg"）
 * @return 0 成功，1 失败
 */
uint8_t Decode_JPG_Picture(const char *path)
{
    FRESULT fr;
    JRESULT res;
    JDEC jdec;
    uint8_t ret = 0;

    file_opened = 0;
    workbuf = NULL;
    line_buf = NULL;

    do {
        fr = f_open(&fil, path, FA_READ);
        if (fr != FR_OK) {
            ret = 1;
            break;
        }
        file_opened = 1;

        workbuf = (uint8_t*)malloc_bsc(WORKBUF_SIZE);
        if (!workbuf) {
            ret = 1;
            break;
        }

        line_buf = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
        if (!line_buf) {
            ret = 1;
            break;
        }

        buf_start_y = 0;
        buf_filled_lines = 0;

        jdec.pool = workbuf;
        jdec.sz_pool = WORKBUF_SIZE;
        res = jd_prepare(&jdec, in_func, workbuf, WORKBUF_SIZE, &fil);
        if (res != JDR_OK) {
            ret = 1;
            break;
        }

        uint16_t w = jdec.width;
        uint16_t h = jdec.height;
        uint16_t short_side = (w < h) ? w : h;
        uint8_t scale = 0;
        if (short_side >= 1920)      scale = 3;   // 1/8
        else if (short_side >= 960)  scale = 2;   // 1/4
        else if (short_side >= 480)  scale = 1;   // 1/2

        uint16_t scaled_w = w >> scale;
        uint16_t scaled_h = h >> scale;
        g_offset_x = (LCD_WIDTH  - scaled_w) / 2;
        g_offset_y = (LCD_HEIGHT - scaled_h) / 2;

        buf_start_y = 0;
        buf_filled_lines = 0;
        memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));

        res = jd_decomp(&jdec, out_func, scale);
        if (res != JDR_OK) {
            ret = 1;
            break;
        }

        if (buf_filled_lines > 0) {
            flush_line_buffer();
        }
    } while(0);

    // 统一清理
    if (line_buf) {
        free_bsc(line_buf);
        line_buf = NULL;
    }
    if (workbuf) {
        free_bsc(workbuf);
        workbuf = NULL;
    }
    if (file_opened) {
        f_close(&fil);
        file_opened = 0;
    }
    return ret;
}
