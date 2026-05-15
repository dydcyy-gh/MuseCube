#include "txt.h"
#include "malloc.h"
#include <string.h>
#include "debug.h"

bool txt_decoder_open(txt_decoder_t *decoder, const char *filepath, uint32_t buf_size)
{
    if (decoder == NULL || filepath == NULL || buf_size <= 1) return false;

    if (f_open(&decoder->file, filepath, FA_READ) != FR_OK) {
        return false;
    }

    decoder->file_size = f_size(&decoder->file);
    decoder->current_offset = 0;
    decoder->buf_size = buf_size;

    decoder->text_buf = (char *)malloc_bsc(buf_size);
    if (decoder->text_buf == NULL) {
        f_close(&decoder->file);
        return false;
    }

    memset(decoder->text_buf, 0, buf_size);
    return true;
}

void txt_decoder_close(txt_decoder_t *decoder)
{
    if (decoder == NULL) return;

    // 关闭文件
    f_close(&decoder->file);

    // 释放 CCM 内存
    if (decoder->text_buf != NULL) {
        free_bsc(decoder->text_buf);
        decoder->text_buf = NULL;
    }
}

uint32_t txt_decoder_read_chunk(txt_decoder_t *decoder, uint32_t offset)
{
    if (decoder == NULL || decoder->text_buf == NULL) return 0;
    
    // 如果偏移量超出文件大小，直接返回 0 (文件尾)
    if (offset >= decoder->file_size) {
        decoder->text_buf[0] = '\0';
        return 0;
    }

    // 移动文件指针到指定偏移量
    f_lseek(&decoder->file, offset);

    UINT br = 0;
    // 读取数据，留 1 个字节给字符串结束符 '\0'
    FRESULT res = f_read(&decoder->file, decoder->text_buf, decoder->buf_size - 1, &br);

    if (res != FR_OK || br == 0) {
        debug_printf("FatFs Read Error! res: %d, br: %d\n", res, br);
        decoder->text_buf[0] = '\0';
        return 0;
    }

    // --- 【新增 1】检测 UTF-16 编码防卡死 ---
    // 如果大文件实际是 UTF-16，会包含大量 0x00 导致 LVGL 识别为空字符串
    if (offset == 0 && br >= 2) {
        if (((uint8_t)decoder->text_buf[0] == 0xFF && (uint8_t)decoder->text_buf[1] == 0xFE) || 
            ((uint8_t)decoder->text_buf[0] == 0xFE && (uint8_t)decoder->text_buf[1] == 0xFF)) {
            const char *err = "Encoding Error: This is UTF-16. Please save file as UTF-8.";
            strncpy(decoder->text_buf, err, decoder->buf_size - 1);
            decoder->text_buf[strlen(err)] = '\0';
            return br; // 强制返回，在界面显示错误提示
        }
    }

    // --- 【新增 2】处理 UTF-8 BOM 头 ---
    // Windows 记事本保存的 UTF-8 通常带 BOM (EF BB BF)，LVGL 无法渲染会报错
    if (offset == 0 && br >= 3) {
        if ((uint8_t)decoder->text_buf[0] == 0xEF && 
            (uint8_t)decoder->text_buf[1] == 0xBB && 
            (uint8_t)decoder->text_buf[2] == 0xBF) {
            // 将 BOM 替换为空格，不改变字节长度，保证偏移量不出错
            decoder->text_buf[0] = ' ';
            decoder->text_buf[1] = ' ';
            decoder->text_buf[2] = ' ';
        }
    }

    uint32_t valid_len = br;

    // --- 【优化】UTF-8 断字防截断处理 ---
    // 只有在未读到文件末尾时，才需要防截断
    if (offset + br < decoder->file_size) {
        int tail_bytes = 0; // 记录尾部属于多字节字符的连续字节数

        while (valid_len > 0) {
            uint8_t last_byte = (uint8_t)decoder->text_buf[valid_len - 1];

            if ((last_byte & 0x80) == 0x00) {
                break; // 0xxxxxxx: ASCII 字符，安全截断
            } 
            else if ((last_byte & 0xC0) == 0x80) {
                // 10xxxxxx: 多字节字符的后续字节
                tail_bytes++;
                valid_len--;
            } 
            else {
                // 11xxxxxx: 找到了多字节字符的首字节
                valid_len--; 
                
                // 判断这个 UTF-8 字符理应有多少个后续字节
                int expected_tail = 0;
                if ((last_byte & 0xE0) == 0xC0) expected_tail = 1;      
                else if ((last_byte & 0xF0) == 0xE0) expected_tail = 2; 
                else if ((last_byte & 0xF8) == 0xF0) expected_tail = 3; 

                // 如果实际读到的尾部字节 == 期望字节数，说明字符是完整的，恢复长度
                if (tail_bytes == expected_tail) {
                    valid_len += (1 + tail_bytes); 
                }
                break;
            }
        }
    }

    // 兜底保护
    if (valid_len == 0) valid_len = br; 

    // --- 【新增 3】深度文本清洗 ---
    for(uint32_t i = 0; i < valid_len; i++) {
        if (decoder->text_buf[i] == '\r') {
            decoder->text_buf[i] = ' '; // 替换回车，防止乱码方块
        }
        else if (decoder->text_buf[i] == '\0') {
            decoder->text_buf[i] = ' '; // 防止文本内部夹杂 0x00 导致字符串提早结束 (常见于乱码文件)
        }
    }

    // 添加字符串结束符
    decoder->text_buf[valid_len] = '\0';
    decoder->current_offset = offset;

    return valid_len;
}
