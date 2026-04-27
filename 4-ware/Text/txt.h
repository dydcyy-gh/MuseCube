#ifndef __TXT_H__
#define __TXT_H__

#include "ff.h"
#include <stdint.h>
#include <stdbool.h>

// TXT 文件解码器句柄
typedef struct {
    FIL file;                 // FatFs 文件对象
    uint32_t file_size;       // 文件总大小
    uint32_t current_offset;  // 当前所在的读取偏移量
    char *text_buf;           // 用于存放解码/读取到的文本数据
    uint32_t buf_size;        // 缓冲区最大容量
} txt_decoder_t;

/**
 * @brief 初始化 TXT 解码器
 * @param decoder 解码器句柄
 * @param filepath 文件路径 (FatFs 格式，如 "0:/test.txt")
 * @param buf_size 读取缓冲区大小 (建议 512 或 1024)
 * @return true 成功, false 失败(文件不存在或内存不足)
 */
bool txt_decoder_open(txt_decoder_t *decoder, const char *filepath, uint32_t buf_size);

/**
 * @brief 关闭 TXT 解码器，释放内存
 * @param decoder 解码器句柄
 */
void txt_decoder_close(txt_decoder_t *decoder);

/**
 * @brief 从指定偏移量读取一段文本块 (自动处理 UTF-8 边界防截断)
 * @param decoder 解码器句柄
 * @param offset 在文件中的偏移量 (第一页传 0)
 * @return 实际读取的有效字节数。你可以用 offset + valid_len 作为下一页的起始偏移量
 */
uint32_t txt_decoder_read_chunk(txt_decoder_t *decoder, uint32_t offset);

#endif
