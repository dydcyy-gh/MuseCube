#ifndef __LRC_H__
#define __LRC_H__

#include "ff.h"
#include <stdint.h>
#include <stdbool.h>

// 歌词行数据结构
typedef struct {
    uint32_t time_ms;  // 该句歌词的时间戳（毫秒）
    char *text;        // 指向歌词文本的指针（指向解码器的 text_buf 内部）
} lrc_line_t;

// LRC 文件解码器句柄
typedef struct {
    FIL file;                 // FatFs 文件对象
    uint32_t file_size;       // 文件总大小
    char *text_buf;           // 用于存放读取到的歌词原文
    uint32_t buf_size;        // 文本缓冲区大小
    
    lrc_line_t *lines;        // 存放解析后的每一行时间戳与文本指针
    uint16_t line_count;      // 实际解析出的有效歌词行数
    uint16_t max_lines;       // 最大支持的行数容量
} lrc_decoder_t;

/**
 * @brief 初始化 LRC 歌词解码器并解析内容
 * @param decoder 解码器句柄
 * @param filepath 歌词文件路径
 * @param max_buf_size 最大允许读取的字节数（如 8192 或更大）
 * @param max_lines 最大支持解析的行数（如 100 行为一首普通歌，长歌可设 200）
 * @return true 成功, false 失败(文件不存在、内存不足或格式错误)
 */
bool lrc_decoder_open(lrc_decoder_t *decoder, const char *filepath, uint32_t max_buf_size, uint16_t max_lines);

/**
 * @brief 关闭 LRC 解码器，释放内存
 * @param decoder 解码器句柄
 */
void lrc_decoder_close(lrc_decoder_t *decoder);

/**
 * @brief 根据当前播放时间获取对应的歌词索引
 * @param decoder 解码器句柄
 * @param current_time_ms 当前播放时间（毫秒）
 * @return >=0 当前应显示的歌词行索引; -1 表示没有匹配的歌词（如还没开始）
 */
int16_t lrc_decoder_get_line_index(lrc_decoder_t *decoder, uint32_t current_time_ms);

#endif // __LRC_H__
