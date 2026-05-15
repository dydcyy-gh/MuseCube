#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include "AnimatedGIF.h"
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
#define BUF_LINES  32

static FIL fil;                     
static GIFIMAGE *gif = NULL;        
static uint16_t *line_buf = NULL;   
static uint16_t buf_start_y = 0;    
static uint16_t buf_filled_lines = 0; 
static int16_t g_offset_x = 0;      
static int16_t g_offset_y = 0;      

static uint8_t file_opened = 0;

extern void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
extern void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count);
extern int GIFInit(GIFIMAGE *pGIF);

static int32_t gif_read_cb(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    FIL *f = (FIL*)pFile->fHandle;
    UINT br;
    if (f_read(f, pBuf, iLen, &br) != FR_OK) return 0;
    pFile->iPos = f_tell(f);
    return br;
}

static int32_t gif_seek_cb(GIFFILE *pFile, int32_t iPosition) {
    FIL *f = (FIL*)pFile->fHandle;
    if (f_lseek(f, iPosition) != FR_OK) return pFile->iPos;
    pFile->iPos = f_tell(f);
    return pFile->iPos;
}

static void flush_line_buffer(void) {
    if (buf_filled_lines == 0) return;
    LCD_Address_Set(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled_lines - 1);
    LCD_Write_DMA(line_buf, LCD_WIDTH * buf_filled_lines);
    xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
    
    buf_filled_lines = 0;
    memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
}

static void GIFDraw(GIFDRAW *pDraw) {
    int lcd_y = pDraw->iY + pDraw->y + g_offset_y;

    if (lcd_y < 0 || lcd_y >= LCD_HEIGHT) return;

    if (buf_filled_lines > 0 && (lcd_y != buf_start_y + buf_filled_lines || buf_filled_lines >= BUF_LINES)) {
        flush_line_buffer();
    }

    if (buf_filled_lines == 0) {
        buf_start_y = lcd_y;
    }

    int lcd_x = pDraw->iX + g_offset_x;
    int width = pDraw->iWidth;
    int src_x = 0;

    if (lcd_x < 0) {
        src_x = -lcd_x;
        width += lcd_x;
        lcd_x = 0;
    }
    if (lcd_x + width > LCD_WIDTH) {
        width = LCD_WIDTH - lcd_x;
    }
    if (width <= 0) return;

    uint8_t *src_pixels = pDraw->pPixels + src_x;
    uint16_t *pal = pDraw->pPalette;
    uint16_t *dst = &line_buf[buf_filled_lines * LCD_WIDTH];

    for (int i = 0; i < width; i++) {
        uint8_t c = src_pixels[i];
        if (pDraw->ucHasTransparency && c == pDraw->ucTransparent) {
            dst[lcd_x + i] = pal[pDraw->ucBackground];
        } else {
            dst[lcd_x + i] = pal[c];
        }
    }

    buf_filled_lines++;
}

// -------------------------------------------------------------------------
// 主解码接口 API (Init / Task / Deinit)
// -------------------------------------------------------------------------

void Decode_GIF_Deinit(void)
{
    if (line_buf) {
        free_bsc(line_buf);
        line_buf = NULL;
    }
    if (gif) {
        free_bsc(gif);
        gif = NULL;
    }
    if (file_opened) {
        f_close(&fil);
        file_opened = 0;
    }
}

uint8_t Decode_GIF_Init(const char *path)
{
    FRESULT fr;
    uint8_t ret = 0;

    Decode_GIF_Deinit();

    do {
        fr = f_open(&fil, path, FA_READ);
        if (fr != FR_OK) {
            ret = 1;
            break;
        }
        file_opened = 1;

        gif = (GIFIMAGE*)malloc_bsc(sizeof(GIFIMAGE));
        line_buf = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));

        if (!gif || !line_buf) {
            ret = 1;
            break;
        }

        GIF_begin(gif, GIF_PALETTE_RGB565_LE);
        
        gif->iError = GIF_SUCCESS;
        gif->pfnRead = gif_read_cb;
        gif->pfnSeek = gif_seek_cb;
        gif->pfnDraw = GIFDraw;
        gif->pfnOpen = NULL;
        gif->pfnClose = NULL;
        gif->GIFFile.fHandle = &fil;
        gif->GIFFile.iSize = f_size(&fil);

        if (!GIFInit(gif)) {
            ret = 1;
            break;
        }

        g_offset_x = (LCD_WIDTH - gif->iCanvasWidth) / 2;
        g_offset_y = (LCD_HEIGHT - gif->iCanvasHeight) / 2;

    } while(0);

    if (ret != 0) {
        Decode_GIF_Deinit();
    }

    return ret;
}

uint8_t Decode_GIF_Task(void)
{
    if (!gif || !line_buf || !file_opened) {
        return 1; // 未初始化或出错
    }

    int delayMilliseconds = 0;
    TickType_t t0 = xTaskGetTickCount();

    buf_start_y = 0;
    buf_filled_lines = 0;
    memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));

    // 解码一帧
    int play_result = GIF_playFrame(gif, &delayMilliseconds, NULL);

    if (buf_filled_lines > 0) {
        flush_line_buffer();
    }

    if (play_result < 0) {
        return 1; // 真正的解码错误，返回 1 通知外层退出
    }

    // 控制帧率延迟
    TickType_t t1 = xTaskGetTickCount();
    uint32_t decode_time = (t1 - t0) * portTICK_PERIOD_MS;
    if (delayMilliseconds > decode_time) {
        vTaskDelay(pdMS_TO_TICKS(delayMilliseconds - decode_time));
    } else {
        vTaskDelay(1); // 让出 CPU 控制权
    }

    // 若当前序列播放到最后一帧，无缝重头开始
        if (play_result == 0) {
        // 1. 文件指针归零
        f_lseek(&fil, 0);
        
        // 2. 重新初始化 GIFIMAGE 结构体状态
        GIF_begin(gif, GIF_PALETTE_RGB565_LE);
        
        // 3. 重新绑定必要的回调与文件参数
        gif->iError = GIF_SUCCESS;
        gif->pfnRead = gif_read_cb;
        gif->pfnSeek = gif_seek_cb;
        gif->pfnDraw = GIFDraw;
        gif->pfnOpen = NULL;
        gif->pfnClose = NULL;
        gif->GIFFile.fHandle = &fil;
        gif->GIFFile.iSize = f_size(&fil);
        gif->GIFFile.iPos = 0;
        
        // 4. 再次调用初始化解析 Header
        GIFInit(gif);
    }

    return 0; 
}
