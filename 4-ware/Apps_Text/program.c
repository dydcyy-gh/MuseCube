#include "program.h"
#include "malloc.h"
#include <string.h>

bool prog_decoder_open(prog_decoder_t *decoder, const char *filepath, uint32_t max_buf_size)
{
    if (decoder == NULL || filepath == NULL || max_buf_size == 0) return false;

    if (f_open(&decoder->file, filepath, FA_READ) != FR_OK) {
        return false;
    }

    decoder->file_size = f_size(&decoder->file);
    decoder->buf_size = max_buf_size;
    
    // 分配输出缓冲区
    decoder->text_buf = (char *)malloc_bsc(max_buf_size);
    if (decoder->text_buf == NULL) {
        f_close(&decoder->file);
        return false;
    }

    uint32_t out_len = 0;
    char temp_buf[128];
    UINT br;
    
    // 分块读取并实时解析 (解决 \t 缩进不显示和 \r 乱码问题)
    while (f_read(&decoder->file, temp_buf, sizeof(temp_buf), &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            if (out_len >= max_buf_size - 1) break; // 留 1 个字节给 '\0'
            
            char c = temp_buf[i];
            
            if (c == '\r') {
                continue; // Windows 回车符，直接丢弃
            } 
            else if (c == '\0') {
                decoder->text_buf[out_len++] = ' '; // 防止内部含 0x00 导致字符串提早截断
            } 
            else if (c == '\t') {
                // 将 1 个 Tab 展开为 4 个空格
                for (int s = 0; s < 4; s++) {
                    if (out_len >= max_buf_size - 1) break;
                    decoder->text_buf[out_len++] = ' ';
                }
            } 
            else {
                // 正常字符录入
                decoder->text_buf[out_len++] = c;
            }
        }
        if (out_len >= max_buf_size - 1) break; // 缓冲区已满，结束读取
    }
    
    f_close(&decoder->file);
    decoder->text_buf[out_len] = '\0';

    // 如果文件很大，超过了我们读取的范围，在末尾追加截断提示
    if (out_len >= max_buf_size - 1 && decoder->file_size > out_len) {
        const char * warning = "\n...[File too large, Truncated]...";
        if (out_len > strlen(warning)) {
            strcpy(&decoder->text_buf[out_len - strlen(warning)], warning);
        }
    }

    return true;
}

void prog_decoder_close(prog_decoder_t *decoder)
{
    if (decoder == NULL) return;

    if (decoder->text_buf != NULL) {
        free_bsc(decoder->text_buf);
        decoder->text_buf = NULL;
    }
}
