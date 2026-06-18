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
#include "avi.h"
#include "i2s.h"

// --- 视频配置 ---
#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define BUF_LINES  16        // 极限对齐 MCU 尺寸，双重缓冲仅需 7.5KB
#define WORKBUF_SIZE 10240   // TJpgDec 工作区大小：给足10KB，绝对防止解不开复杂帧头！

// --- 音频配置 ---
#define AVI_AUDIO_BUF_SIZE   8192   // I2S DMA 硬件双缓冲总大小 (Ping-Pong各4KB)
#define AVI_AUDIO_FIFO_SIZE  16384  // 软件环形缓冲区 (提供约 90ms 弹性抗抖动能力)

// 宏定义：将四个字符组合为 32-bit 的 chunk ID
#define FOURCC(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

static FIL fil;                     
static uint8_t *workbuf = NULL;     
static int file_opened = 0;         
static int initialized = 0;
static uint32_t movi_offset = 0;    

// --- 视频相关状态 ---
static uint16_t *line_buf[2] = {NULL, NULL}; 
static uint8_t write_idx = 0;                
static uint8_t dma_busy = 0;                 
static uint16_t buf_start_y = 0;     
static uint16_t buf_filled_lines = 0;

// --- 音频相关状态 ---
static uint8_t has_audio = 0;
static uint8_t dma_started = 0;
static uint16_t audio_channels = 2;
static uint32_t audio_samplerate = 44100;
static uint16_t audio_bps = 16;
static uint8_t *audio_buf[2] = {NULL, NULL};

// --- 高性能环形缓冲 ---
static uint8_t *audio_fifo = NULL;
static volatile uint32_t fifo_wr = 0;
static volatile uint32_t fifo_rd = 0;
static volatile uint32_t fifo_count = 0;

// --------------------------- 音频核心驱动区 ---------------------------

// 【零拷贝提取】从环形缓冲区提取数据喂给 DMA
static void pull_audio_fifo(uint8_t *out_buf, uint32_t len) {
    if (fifo_count < len) {
        memset(out_buf, 0, len); // 缓冲不足则输出静音防爆音
        return;
    }

    uint32_t right_part = AVI_AUDIO_FIFO_SIZE - fifo_rd;
    if (len <= right_part) {
        memcpy(out_buf, &audio_fifo[fifo_rd], len);
        fifo_rd += len;
        if (fifo_rd == AVI_AUDIO_FIFO_SIZE) fifo_rd = 0;
    } else {
        memcpy(out_buf, &audio_fifo[fifo_rd], right_part);
        memcpy(out_buf + right_part, audio_fifo, len - right_part);
        fifo_rd = len - right_part;
    }
    
    vPortEnterCritical();
    fifo_count -= len;
    vPortExitCritical();
}

// 供各种碎片时间高频轮询的无阻塞音频服务
static void audio_service(void) {
    if (!has_audio || !dma_started) return;
    if (xSemaphoreTake(xI2SSemaphore, 0) == pdTRUE) {
        pull_audio_fifo(audio_buf[I2SdmaBuff], AVI_AUDIO_BUF_SIZE / 2);
    }
}

// --------------------------- 视频双缓冲与解码回调 ---------------------------

static void wait_video_dma_done(void) {
    while (dma_busy) {
        // 使用短暂超时确保在等待屏幕刷新时绝不饿死音频 DMA
        if (xEventGroupWaitBits(xLcdEventGroup, LCD_USER_MDIA, pdTRUE, pdFALSE, pdMS_TO_TICKS(2)) != 0) {
            dma_busy = 0;
            break;
        }
        audio_service(); 
    }
}

static size_t in_func(JDEC *jd, uint8_t *buff, size_t nbyte) {
    audio_service(); 
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

static void flush_line_buffer(void) {
    if (buf_filled_lines == 0) return;
    wait_video_dma_done();
    LCD_Color_Fill(0, buf_start_y, LCD_WIDTH - 1, buf_start_y + buf_filled_lines - 1, line_buf[write_idx]);
    dma_busy = 1;
    buf_start_y += buf_filled_lines;
    buf_filled_lines = 0;
    write_idx ^= 1;
}

static int out_func(JDEC *jd, void *bitmap, JRECT *rect) {
    audio_service(); 
    uint16_t *src = (uint16_t*)bitmap;
    uint16_t left = rect->left, right = rect->right, top = rect->top, bottom = rect->bottom;

    for (uint16_t y = top; y <= bottom; y++) {
        while (y >= buf_start_y + BUF_LINES) flush_line_buffer();
        int row_in_buf = y - buf_start_y;
        
        if (row_in_buf >= 0 && left < LCD_WIDTH) {
            uint16_t copy_right = (right >= LCD_WIDTH) ? (LCD_WIDTH - 1) : right;
            uint16_t width = copy_right - left + 1;
            uint16_t *dst = &line_buf[write_idx][row_in_buf * LCD_WIDTH + left];
            memcpy(dst, src, width * sizeof(uint16_t));
            
            if (row_in_buf + 1 > buf_filled_lines) buf_filled_lines = row_in_buf + 1;
        }
        src += (right - left + 1); 
    }
    return 1;
}

// --------------------------- 核心控制流程 ---------------------------

uint8_t video_avi_play_init(const char *file)
{
    // 将局部变量声明统一提取到函数最开头，彻底消灭 goto 警告
    uint32_t header[3]; 
    UINT br;
    uint32_t current_stream = 0; 

    if (initialized) return 0;
    has_audio = 0; dma_started = 0;
    fifo_wr = 0; fifo_rd = 0; fifo_count = 0;
    movi_offset = 0;
    
    if (f_open(&fil, file, FA_READ) != FR_OK) return 1;
    file_opened = 1;

    if (f_read(&fil, header, 12, &br) != FR_OK || br != 12) goto err;
    if (header[0] != FOURCC('R','I','F','F') || header[2] != FOURCC('A','V','I',' ')) goto err;

    while (1) {
        uint32_t chunk[2];
        if (f_read(&fil, chunk, 8, &br) != FR_OK || br != 8) break;
        uint32_t cid = chunk[0], csize = chunk[1];
        uint32_t align_size = (csize + 1) & ~1;

        if (cid == FOURCC('L','I','S','T')) {
            uint32_t list_type;
            if (f_read(&fil, &list_type, 4, &br) != FR_OK) break;
            if (list_type == FOURCC('m','o','v','i')) {
                movi_offset = f_tell(&fil); 
                break; 
            }
            if (list_type == FOURCC('h','d','r','l') || list_type == FOURCC('s','t','r','l')) {
                continue; // 允许游标自然进入该 List 内部读取子组件
            }
            f_lseek(&fil, f_tell(&fil) + align_size - 4);
            continue; 
        }
        else if (cid == FOURCC('s','t','r','h')) {
            uint32_t fccType;
            if (f_read(&fil, &fccType, 4, &br) == FR_OK) {
                if (fccType == FOURCC('v','i','d','s')) current_stream = 1;
                else if (fccType == FOURCC('a','u','d','s')) current_stream = 2;
                f_lseek(&fil, f_tell(&fil) + align_size - 4);
            }
        }
        else if (cid == FOURCC('s','t','r','f')) {
            if (current_stream == 2) {
                uint16_t wav_fmt[8]; 
                if (f_read(&fil, wav_fmt, 16, &br) == FR_OK) {
                    audio_channels = wav_fmt[1];
                    audio_samplerate = wav_fmt[2] | ((uint32_t)wav_fmt[3] << 16);
                    audio_bps = wav_fmt[7];
                    has_audio = 1;
                    f_lseek(&fil, f_tell(&fil) + align_size - 16);
                }
            } else f_lseek(&fil, f_tell(&fil) + align_size);
        }
        else f_lseek(&fil, f_tell(&fil) + align_size);
    }
    
    if (movi_offset == 0) goto err;

    workbuf = (uint8_t*)malloc_bsc(WORKBUF_SIZE);
    line_buf[0] = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
    line_buf[1] = (uint16_t*)malloc_bsc(LCD_WIDTH * BUF_LINES * sizeof(uint16_t));
    if (!workbuf || !line_buf[0] || !line_buf[1]) goto err;

    if (has_audio) {
        audio_buf[0] = (uint8_t*)malloc_bsc(AVI_AUDIO_BUF_SIZE / 2);
        audio_buf[1] = (uint8_t*)malloc_bsc(AVI_AUDIO_BUF_SIZE / 2);
        audio_fifo   = (uint8_t*)malloc_bsc(AVI_AUDIO_FIFO_SIZE);
        if (!audio_buf[0] || !audio_buf[1] || !audio_fifo) goto err;

        memset(audio_buf[0], 0, AVI_AUDIO_BUF_SIZE / 2);
        memset(audio_buf[1], 0, AVI_AUDIO_BUF_SIZE / 2);

        I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
        I2S2_SampleRate_Set(audio_samplerate);
        
        I2S2_TX_DMA_Init(audio_buf[0], audio_buf[1], (AVI_AUDIO_BUF_SIZE / 2) / 2); 
        
        if (xI2SSemaphore != NULL) xSemaphoreTake(xI2SSemaphore, 0); 
    }

    buf_start_y = 0; buf_filled_lines = 0; write_idx = 0; dma_busy = 0;
    initialized = 1;
    return 0;

err:
    video_avi_play_deinit();
    return 1;
}

uint8_t video_avi_play_task(void) {
    if (!initialized) return 1;

    while (1) {
        audio_service(); 
        
        uint32_t chunk[2]; UINT br;
        if (f_read(&fil, chunk, 8, &br) != FR_OK || br != 8) {
            if (movi_offset != 0) {
                f_lseek(&fil, movi_offset);
                vPortEnterCritical();
                fifo_wr = 0; fifo_rd = 0; fifo_count = 0; // 清空历史残音
                vPortExitCritical();
                continue; 
            }
            return 2; 
        }
        
        uint32_t cid = chunk[0], csize = chunk[1];
        uint32_t align_size = (csize + 1) & ~1;
        uint8_t *id = (uint8_t*)&cid;
        uint32_t chunk_start_pos = f_tell(&fil);

        if (cid == FOURCC('L','I','S','T')) {
            uint32_t list_type;
            if (f_read(&fil, &list_type, 4, &br) == FR_OK) {
                if (list_type == FOURCC('m','o','v','i') || list_type == FOURCC('r','e','c',' ')) continue;
            }
            f_lseek(&fil, chunk_start_pos + align_size);
            continue;
        }

        if (id[2] == 'd' && id[3] == 'c') {
            JDEC jdec;
            jdec.pool = workbuf; jdec.sz_pool = WORKBUF_SIZE;
            
            if (jd_prepare(&jdec, in_func, workbuf, WORKBUF_SIZE, &fil) == JDR_OK) {
                buf_start_y = 0; buf_filled_lines = 0;
                jd_decomp(&jdec, out_func, 0);
                if (buf_filled_lines > 0) flush_line_buffer();
                wait_video_dma_done();
            }
            
            f_lseek(&fil, chunk_start_pos + align_size);
            return 0; // 成功解析一帧画面，让出控制权响应按键
        }
        else if (id[2] == 'w' && id[3] == 'b') {
            if (has_audio) {
                uint32_t left = csize;
                
                while (left > 0) {
                    uint32_t free_space = AVI_AUDIO_FIFO_SIZE - fifo_count;
                    
                    if (audio_channels == 1 && audio_bps == 16) {
                        uint8_t temp_buf[256];
                        uint32_t read_len = (left > sizeof(temp_buf)) ? sizeof(temp_buf) : left;
                        
                        if (free_space < read_len * 2) {
                            if (dma_started) {
                                if (xSemaphoreTake(xI2SSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
                                    pull_audio_fifo(audio_buf[I2SdmaBuff], AVI_AUDIO_BUF_SIZE / 2);
                                } else left = 0; 
                            } else break; 
                            continue;
                        }
                        
                        if (f_read(&fil, temp_buf, read_len, &br) == FR_OK && br > 0) {
                            int16_t *src = (int16_t*)temp_buf;
                            for (uint32_t i = 0; i < br / 2; i++) {
                                int16_t val = src[i];
                                audio_fifo[fifo_wr++] = val & 0xFF; if(fifo_wr == AVI_AUDIO_FIFO_SIZE) fifo_wr = 0;
                                audio_fifo[fifo_wr++] = (val >> 8) & 0xFF; if(fifo_wr == AVI_AUDIO_FIFO_SIZE) fifo_wr = 0;
                                audio_fifo[fifo_wr++] = val & 0xFF; if(fifo_wr == AVI_AUDIO_FIFO_SIZE) fifo_wr = 0;
                                audio_fifo[fifo_wr++] = (val >> 8) & 0xFF; if(fifo_wr == AVI_AUDIO_FIFO_SIZE) fifo_wr = 0;
                            }
                            vPortEnterCritical(); fifo_count += (br * 2); vPortExitCritical();
                            left -= br;
                        } else break;
                    } 
                    else {
                        uint32_t right_part = AVI_AUDIO_FIFO_SIZE - fifo_wr;
                        uint32_t read_len = (left > right_part) ? right_part : left;
                        if (read_len > free_space) read_len = free_space;
                        
                        if (read_len > 0) {
                            if (f_read(&fil, &audio_fifo[fifo_wr], read_len, &br) == FR_OK && br > 0) {
                                fifo_wr += br;
                                if (fifo_wr == AVI_AUDIO_FIFO_SIZE) fifo_wr = 0;
                                vPortEnterCritical(); fifo_count += br; vPortExitCritical();
                                left -= br;
                            } else break;
                        } else {
                            if (dma_started) {
                                if (xSemaphoreTake(xI2SSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
                                    pull_audio_fifo(audio_buf[I2SdmaBuff], AVI_AUDIO_BUF_SIZE / 2);
                                } else break; 
                            } else break;
                        }
                    }
                    
                    // 等到 FIFO 攒够一半水量（避免空响）再让 DMA 上班发声
                    if (!dma_started && fifo_count >= (AVI_AUDIO_FIFO_SIZE / 2)) {
                        if (xI2SSemaphore != NULL) xSemaphoreTake(xI2SSemaphore, 0); 
                        I2S_Play_Start();
                        dma_started = 1;
                    }
                }
            }
            f_lseek(&fil, chunk_start_pos + align_size);
        }
        else if (cid == FOURCC('i','d','x','1')) {
            f_lseek(&fil, movi_offset);
            vPortEnterCritical();
            fifo_wr = 0; fifo_rd = 0; fifo_count = 0;
            vPortExitCritical();
        }
        else {
            f_lseek(&fil, chunk_start_pos + align_size);
        }
    }
}

void video_avi_play_deinit(void) 
{
    if (!initialized && !file_opened) return;

    wait_video_dma_done();

    if (has_audio) {
        I2S_Play_Stop();
        if (audio_buf[0]) { free_bsc(audio_buf[0]); audio_buf[0] = NULL; }
        if (audio_buf[1]) { free_bsc(audio_buf[1]); audio_buf[1] = NULL; }
        if (audio_fifo)   { free_bsc(audio_fifo);   audio_fifo = NULL;   }
        has_audio = 0;
    }

    if (line_buf[0]) { free_bsc(line_buf[0]); line_buf[0] = NULL; }
    if (line_buf[1]) { free_bsc(line_buf[1]); line_buf[1] = NULL; }
    if (workbuf) { free_bsc(workbuf); workbuf = NULL; }
    if (file_opened) { f_close(&fil); file_opened = 0; }
    
    initialized = 0;
    dma_started = 0;
}
