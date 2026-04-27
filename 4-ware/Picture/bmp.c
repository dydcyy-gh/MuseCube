#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include <string.h>
#include <stdlib.h>
#include "task_manager.h"
#include "lvgl.h"
#include "variables.h"
#include "defines.h"

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define BUF_LINES  16

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      
    uint32_t bfSize;      
    uint16_t bfReserved1; 
    uint16_t bfReserved2; 
    uint32_t bfOffBits;   
} BMP_FILE_HEADER;

typedef struct {
    uint32_t biSize;          
    int32_t  biWidth;         
    int32_t  biHeight;        
    uint16_t biPlanes;        
    uint16_t biBitCount;      
    uint32_t biCompression;   
    uint32_t biSizeImage;     
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMP_INFO_HEADER;
#pragma pack(pop)

uint8_t Decode_BMP_Picture(const char *path)
{
    FIL fil;
    FRESULT fr;
    UINT br;
    uint8_t ret = 0;
    uint8_t file_opened = 0;
    
    uint16_t *line_buf = NULL;
    uint8_t *src_buf = NULL;

    BMP_FILE_HEADER file_h;
    BMP_INFO_HEADER info_h;

    // 使用 do-while(0) 替代 goto
    do {
        fr = f_open(&fil, path, FA_READ);
        if (fr != FR_OK) {
            ret = 1;
            break;
        }
        file_opened = 1;

        f_read(&fil, &file_h, sizeof(BMP_FILE_HEADER), &br);
        f_read(&fil, &info_h, sizeof(BMP_INFO_HEADER), &br);

        if (file_h.bfType != 0x4D42 || info_h.biBitCount < 16) { 
            ret = 1;
            break;
        }

        uint32_t w = info_h.biWidth;
        uint32_t h = abs(info_h.biHeight);
        uint8_t bottom_up = (info_h.biHeight > 0); 

        uint32_t short_side = (w < h) ? w : h;
        uint8_t scale = 1;
        while (scale < 8 && (short_side / (scale * 2)) >= LCD_WIDTH) {
            scale *= 2;
        }

        int32_t scaled_w = w / scale;
        int32_t scaled_h = h / scale;

        int32_t offset_x = (LCD_WIDTH - scaled_w) / 2;
        int32_t offset_y = (LCD_HEIGHT - scaled_h) / 2;

        int32_t crop_left   = (offset_x < 0) ? 0 : offset_x;
        int32_t crop_right  = (offset_x + scaled_w > LCD_WIDTH) ? LCD_WIDTH - 1 : offset_x + scaled_w - 1;
        int32_t crop_top    = (offset_y < 0) ? 0 : offset_y;
        int32_t crop_bottom = (offset_y + scaled_h > LCD_HEIGHT) ? LCD_HEIGHT - 1 : offset_y + scaled_h - 1;

        line_buf = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
        
        uint8_t bytes_per_pixel = info_h.biBitCount / 8; 
        uint32_t row_bytes = ((w * info_h.biBitCount + 31) / 32) * 4; 
        uint32_t pixels_to_read = (crop_right - crop_left + 1) * scale;
        uint32_t bytes_to_read = pixels_to_read * bytes_per_pixel;
        src_buf = (uint8_t*)malloc_bsc(bytes_to_read);

        if (!line_buf || !src_buf) {
            ret = 1;
            break;
        }

        uint16_t buf_filled = 0;
        uint16_t buf_start_y = crop_top;
        memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t)); 

        for (int32_t y = crop_top; y <= crop_bottom; y++) {
            int32_t img_y = y - offset_y; 
            int32_t src_y = img_y * scale;
            if (bottom_up) {
                src_y = h - 1 - src_y; 
            }

            int32_t img_x_start = crop_left - offset_x;
            int32_t src_x_start = img_x_start * scale;

            uint32_t file_offset = file_h.bfOffBits + src_y * row_bytes + src_x_start * bytes_per_pixel;
            if (f_lseek(&fil, file_offset) == FR_OK) {
                f_read(&fil, src_buf, bytes_to_read, &br);
                
                uint16_t *dst = &line_buf[buf_filled * LCD_WIDTH + crop_left];
                uint8_t *src_ptr = src_buf;

                for (int32_t x = crop_left; x <= crop_right; x++) {
                    uint16_t color = 0;
                    if (bytes_per_pixel == 3 || bytes_per_pixel == 4) {
                        uint8_t b = src_ptr[0];
                        uint8_t g = src_ptr[1];
                        uint8_t r = src_ptr[2];
                        color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    } 
                    else if (bytes_per_pixel == 2) {
                        color = src_ptr[0] | (src_ptr[1] << 8); 
                    }
                    *dst = color;
                    dst++;
                    src_ptr += scale * bytes_per_pixel; 
                }
            }
            
            buf_filled++;

            if (buf_filled >= BUF_LINES || y == crop_bottom) {
                LCD_Address_Set(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled - 1);
                LCD_Write_DMA(line_buf, LCD_WIDTH * buf_filled);
                xEventGroupWaitBits(xLcdEventGroup, LCD_USER_VDEO, pdTRUE, pdFALSE, portMAX_DELAY);
                
                buf_filled = 0;
                buf_start_y = y + 1;
                memset(line_buf, 0, LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
            }
        }
    } while (0);

    // 统一资源清理区
    if (line_buf) free_bsc(line_buf);
    if (src_buf) free_bsc(src_buf);
    if (file_opened) f_close(&fil);

    return ret;
}
