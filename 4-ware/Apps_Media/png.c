#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include <string.h>
#include "task_manager.h"
#include "lvgl.h"
#include "variables.h"
#include "defines.h"
#include "debug.h"
#include "PNGdec.h"

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define BUF_LINES  16

static FIL fil;                     // FatFs 文件对象
static PNGIMAGE *png = NULL;        // PNGdec 工作结构体 (约 45KB)
static uint16_t *line_buf = NULL;   // DMA 行缓冲区 (RGB565, 16行)
static uint16_t *row_rgb565 = NULL; // 存放 PNGRGB565 转换后的一整行数据
static uint16_t buf_start_y = 0;    // 缓冲区当前起始行
static uint16_t buf_filled_lines = 0; // 缓冲区有效行数
static int16_t g_offset_x = 0;      // 居中X偏移
static int16_t g_offset_y = 0;      // 居中Y偏移

// 声明 PNGdec.c 中实际使用但未暴露在头的函数
extern int PNGInit(PNGIMAGE *pPNG);
extern int DecodePNG(PNGIMAGE *pPage, void *pUser, int iOptions);
extern void PNGRGB565(PNGDRAW *pDraw, uint16_t *pPixels, int iEndiannes, uint32_t u32Bkgd, int iHasAlpha);

// -------------------------------------------------------------------------
// FatFs 文件操作回调
// -------------------------------------------------------------------------
static int32_t png_read_cb(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    FIL *f = (FIL*)pFile->fHandle;
    UINT br;
    if (f_read(f, pBuf, iLen, &br) != FR_OK) return 0;
    pFile->iPos = f_tell(f);
    return br;
}

static int32_t png_seek_cb(PNGFILE *pFile, int32_t iPosition) {
    FIL *f = (FIL*)pFile->fHandle;
    if (f_lseek(f, iPosition) != FR_OK) return pFile->iPos;
    pFile->iPos = f_tell(f);
    return pFile->iPos;
}

// -------------------------------------------------------------------------
// 显示与缓冲逻辑
// -------------------------------------------------------------------------

// 刷新行缓冲区到 LCD
static void flush_line_buffer(void) {
    if (buf_filled_lines == 0) return;
    LCD_Address_Set(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled_lines - 1);
    LCD_Write_DMA(line_buf, LCD_WIDTH * buf_filled_lines);
    xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
    
    buf_filled_lines = 0;
    memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
}

// PNGdec 输出回调：逐行接收解码数据
static int PNGDraw(PNGDRAW *pDraw) {
    int lcd_y = pDraw->y + g_offset_y;

    // 丢弃超出屏幕垂直范围的行
    if (lcd_y < 0 || lcd_y >= LCD_HEIGHT) return 0;

    // 如果缓冲区已满或者不连续，先刷新缓冲区
    if (buf_filled_lines > 0 && (lcd_y != buf_start_y + buf_filled_lines || buf_filled_lines >= BUF_LINES)) {
        flush_line_buffer();
    }

    // 记录新的起始行
    if (buf_filled_lines == 0) {
        buf_start_y = lcd_y;
    }

    int width = pDraw->iWidth;

    // 使用库自带的转换函数，将一行 PNG 像素格式化为 RGB565 (支持透明通道混合)
    // u32Bkgd = 0x000000 意味着带有 Alpha 的像素会跟黑色背景进行混合
    PNGRGB565(pDraw, row_rgb565, PNG_RGB565_LITTLE_ENDIAN, 0x00000000, pDraw->iHasAlpha);

    int lcd_x = g_offset_x;
    int src_x = 0;
    int copy_w = width;

    // 水平裁剪逻辑
    if (lcd_x < 0) {
        src_x = -lcd_x;
        copy_w += lcd_x;
        lcd_x = 0;
    }
    if (lcd_x + copy_w > LCD_WIDTH) {
        copy_w = LCD_WIDTH - lcd_x;
    }
    
    if (copy_w > 0) {
        // 搬运此行有效数据到 DMA 行缓冲区中
        uint16_t *dst = &line_buf[buf_filled_lines * LCD_WIDTH];
        memcpy(dst + lcd_x, row_rgb565 + src_x, copy_w * sizeof(uint16_t));
    }

    buf_filled_lines++;
    return 0; // 0 表示继续解码
}

// -------------------------------------------------------------------------
// 主解码函数
// -------------------------------------------------------------------------

/**
 * @brief 解码并显示一张 PNG 图片（支持居中显示与局部裁剪）
 * @param path 图片文件路径（如 "0:/PICTURE/image.png"）
 * @return 0 成功，1 失败
 */
uint8_t Decode_PNG_Picture(const char *path)
{
    FRESULT fr;
    uint8_t ret = 0;
    uint8_t file_opened = 0;

    png = NULL;
    line_buf = NULL;
    row_rgb565 = NULL;

    do {
        fr = f_open(&fil, path, FA_READ);
        if (fr != FR_OK) {
            ret = 1;
            break;
        }
        file_opened = 1;

        png = (PNGIMAGE*)malloc_bsc(sizeof(PNGIMAGE));
        line_buf = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));

        if (!png || !line_buf) {
            ret = 1;
            break;
        }

        memset(png, 0, sizeof(PNGIMAGE));
        png->iError = PNG_SUCCESS;
        png->pfnRead = png_read_cb;
        png->pfnSeek = png_seek_cb;
        png->pfnDraw = PNGDraw;
        png->pfnOpen = NULL;
        png->pfnClose = NULL;
        png->PNGFile.fHandle = &fil;
        png->PNGFile.iSize = f_size(&fil);

        if (PNGInit(png) != PNG_SUCCESS) {
            ret = 1;
            break;
        }

        row_rgb565 = (uint16_t*)malloc_bsc(png->iWidth * sizeof(uint16_t));
        if (!row_rgb565) {
            ret = 1;
            break;
        }

        g_offset_x = (LCD_WIDTH - png->iWidth) / 2;
        g_offset_y = (LCD_HEIGHT - png->iHeight) / 2;

        buf_start_y = 0;
        buf_filled_lines = 0;
        memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));

        int dec_res = DecodePNG(png, NULL, PNG_FAST_PALETTE);
        
        if (buf_filled_lines > 0) {
            flush_line_buffer();
        }

        if (dec_res != PNG_SUCCESS) {
            ret = 1;
            break;
        }

    } while(0);

    // 统一清理
    if (row_rgb565) { free_bsc(row_rgb565); row_rgb565 = NULL; }
    if (line_buf) { free_bsc(line_buf); line_buf = NULL; }
    if (png) { free_bsc(png); png = NULL; }
    if (file_opened) { f_close(&fil); }

    return ret;
}
