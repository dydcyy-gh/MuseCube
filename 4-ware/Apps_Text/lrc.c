#include "lrc.h"
#include "malloc.h"
#include <string.h>
#include <stdlib.h>

// 辅助函数：简单的字符转数字，避免引入过多的标准库开销
static int parse_number(const char **p, int len) {
    int val = 0;
    for (int i = 0; i < len; i++) {
        if (**p >= '0' && **p <= '9') {
            val = val * 10 + (**p - '0');
            (*p)++;
        } else {
            break;
        }
    }
    return val;
}

bool lrc_decoder_open(lrc_decoder_t *decoder, const char *filepath, uint32_t max_buf_size, uint16_t max_lines)
{
    if (decoder == NULL || filepath == NULL || max_buf_size == 0 || max_lines == 0) return false;

    if (f_open(&decoder->file, filepath, FA_READ) != FR_OK) {
        return false;
    }

    decoder->file_size = f_size(&decoder->file);
    decoder->buf_size = (decoder->file_size < max_buf_size) ? decoder->file_size : max_buf_size;
    decoder->line_count = 0;
    decoder->max_lines = max_lines;

    // 分配文本读取缓冲区 (+1 为了末尾放 \0)
    decoder->text_buf = (char *)malloc_bsc(decoder->buf_size + 1);
    if (decoder->text_buf == NULL) {
        f_close(&decoder->file);
        return false;
    }

    // 分配时间轴与行指针结构体数组
    decoder->lines = (lrc_line_t *)malloc_bsc(sizeof(lrc_line_t) * max_lines);
    if (decoder->lines == NULL) {
        free_bsc(decoder->text_buf);
        decoder->text_buf = NULL;
        f_close(&decoder->file);
        return false;
    }

    UINT br;
    if (f_read(&decoder->file, decoder->text_buf, decoder->buf_size, &br) != FR_OK || br == 0) {
        lrc_decoder_close(decoder);
        return false;
    }
    decoder->text_buf[br] = '\0'; // 确保结尾安全
    f_close(&decoder->file);      // 读取完毕，立刻关闭文件释放句柄

    // --- 开始原地解析 LRC ---
    // LRC标准格式举例: [01:23.45] 歌词内容 \r\n
    // 我们遍历缓冲区，将 \r 和 \n 替换为 \0，从而把整个 buf 切割成独立的短字符串
    char *p = decoder->text_buf;
    while (*p != '\0' && decoder->line_count < max_lines) {
        // 跳过空白字符（如果文件带有BOM，或者多余空格）
        while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
        if (*p == '\0') break;

        // 寻找时间标签起止符号 '[' 和 ']'
        if (*p == '[') {
            const char *time_start = p + 1;
            char *tag_end = strchr(p, ']');
            
            if (tag_end != NULL) {
                // 解析时间 [mm:ss.xx] 或 [mm:ss.xxx]
                const char *tp = time_start;
                int min = parse_number(&tp, 2);
                if (*tp == ':') tp++;
                int sec = parse_number(&tp, 2);
                if (*tp == '.' || *tp == ':') tp++;
                int ms = parse_number(&tp, 3);
                
                // 如果毫秒位只有两位（如 .45），则代表 450 毫秒
                if (tp - time_start <= 7) ms *= 10; 

                uint32_t total_ms = min * 60000 + sec * 1000 + ms;

                // 找这句歌词文本的起始位置
                char *text_start = tag_end + 1;
                // 有些 LRC 会有多个时间标签连着比如 [00:10.00][00:20.00]文本，此处作简单化处理，只认最后一个标签后的文本
                
                // 寻找行末，并将回车换行设为 \0，断断开字符串
                char *line_end = text_start;
                while (*line_end != '\r' && *line_end != '\n' && *line_end != '\0') {
                    line_end++;
                }

                // 保存这行记录
                decoder->lines[decoder->line_count].time_ms = total_ms;
                decoder->lines[decoder->line_count].text = text_start;
                decoder->line_count++;

                // 原地截断
                if (*line_end != '\0') {
                    *line_end = '\0';
                    p = line_end + 1; 
                } else {
                    p = line_end;
                }
                continue; // 下一行
            }
        }
        
        // 如果没有找到 '['，或者格式不对，跳过这一行直接找下一个换行
        while (*p != '\r' && *p != '\n' && *p != '\0') p++;
    }

    return true;
}

void lrc_decoder_close(lrc_decoder_t *decoder)
{
    if (decoder == NULL) return;

    if (decoder->lines != NULL) {
        free_bsc(decoder->lines);
        decoder->lines = NULL;
    }

    if (decoder->text_buf != NULL) {
        free_bsc(decoder->text_buf);
        decoder->text_buf = NULL;
    }
}

int16_t lrc_decoder_get_line_index(lrc_decoder_t *decoder, uint32_t current_time_ms)
{
    if (decoder == NULL || decoder->line_count == 0) return -1;

    // 尚未达到第一句的时间
    if (current_time_ms < decoder->lines[0].time_ms) {
        return -1;
    }

    // 从后往前找，找到第一个时间小于等于当前时间的歌词，就是正在播放的这一句
    for (int16_t i = decoder->line_count - 1; i >= 0; i--) {
        if (current_time_ms >= decoder->lines[i].time_ms) {
            return i;
        }
    }

    return 0; // 兜底返回第一句
}
