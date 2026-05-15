#ifndef __WORDLIST_H__
#define __WORDLIST_H__

#include "ff.h"
#include <stdint.h>
#include <stdbool.h>

// 单个单词的数据结构
typedef struct {
    char word[32];      // 单词本身 (如 abandon)
    char phonetic[32];  // 音标，不含[] (如 əˈbændən)，如果没有则为空字符串
    char meaning[128];  // 中文释义 (如 vt.丢弃；放弃，抛弃)
} word_item_t;

// 词库解析器句柄
typedef struct {
    FIL file;                 // FatFs 文件对象
    uint32_t total_words;     // 词库中包含的总单词数
    uint32_t *line_offsets;   // 动态数组：存放每个单词在文件中的偏移量 (Index)
    uint32_t current_index;   // 当前顺序抽取的索引游标
} wordlist_t;

/**
 * @brief 打开词库并建立索引
 * @param wl 词库句柄
 * @param filepath txt文件路径
 * @return true 成功, false 失败(文件不存在或内存不足)
 */
bool wordlist_open(wordlist_t *wl, const char *filepath);

/**
 * @brief 关闭词库，释放索引内存
 */
void wordlist_close(wordlist_t *wl);

/**
 * @brief 根据索引号获取指定单词
 * @param wl 词库句柄
 * @param index 单词的序号 (0 ~ total_words-1)
 * @param item 存放解析结果的结构体
 * @return true 获取成功, false 失败
 */
bool wordlist_get_by_index(wordlist_t *wl, uint32_t index, word_item_t *item);

/**
 * @brief 顺序获取下一个单词 (读完会自动回到开头)
 */
bool wordlist_get_sequential(wordlist_t *wl, word_item_t *item);

/**
 * @brief 随机获取一个单词
 */
bool wordlist_get_random(wordlist_t *wl, word_item_t *item);

/**
 * @brief 重置顺序游标到词库开头
 */
void wordlist_reset_sequential(wordlist_t *wl);

#endif
