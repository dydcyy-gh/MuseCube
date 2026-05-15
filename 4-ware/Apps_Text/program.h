#ifndef __PROGRAM_H__
#define __PROGRAM_H__

#include "ff.h"
#include <stdint.h>
#include <stdbool.h>

// 代码文件解码器句柄
typedef struct {
    FIL file;                 // FatFs 文件对象
    uint32_t file_size;       // 文件总大小
    char *text_buf;           // 用于存放解码/读取到的代码数据
    uint32_t buf_size;        // 缓冲区最大容量
} prog_decoder_t;

/**
 * @brief 初始化 代码文件 解码器并一次性加载内容 (受限于单片机内存，读取指定上限)
 * @param decoder 解码器句柄
 * @param filepath 文件路径
 * @param max_buf_size 最大允许读取的字节数 (如 8192)
 * @return true 成功, false 失败(文件不存在或内存不足)
 */
bool prog_decoder_open(prog_decoder_t *decoder, const char *filepath, uint32_t max_buf_size);

/**
 * @brief 关闭 代码文件 解码器，释放内存
 * @param decoder 解码器句柄
 */
void prog_decoder_close(prog_decoder_t *decoder);

#endif // __PROGRAM_H__
