#include "wordlist.h"
#include "malloc.h"
#include <string.h>
#include <stdlib.h>

// 缓冲区大小，用于加速统计行数
#define SCAN_BUF_SIZE 512

// ================== 辅助函数：判断是否为词性缩写 ==================
static bool is_pos_marker(const char *p, const char *line_start) {
    // 确保它不是某个英文单词的结尾字符，要求前一个字符非字母
    if (p > line_start) {
        char prev = *(p - 1);
        if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z')) {
            return false;
        }
    }
    // 加入了 "ad." 等更多常见标识符
    const char *markers[] = {
        "n.", "v.", "a.", "adj.", "adv.", "ad.", 
        "vi.", "vt.", "prep.", "pron.", "conj.", 
        "num.", "art.", "int.", "pl.", "abbr.", "aux."
    };
    for (int i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (strncmp(p, markers[i], strlen(markers[i])) == 0) {
            return true;
        }
    }
    return false;
}

// ================== 内部函数：解析单行文本 ==================
static void parse_word_line(const char *line, word_item_t *item)
{
    memset(item, 0, sizeof(word_item_t));
    const char *p = line;

    // 1. 跳过行首可能的空格或不可见字符
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    // 2. 提取【单词本身】
    char *w = item->word;
    while (*p != ' ' && *p != '\t' && *p != '[' && *p != '\r' && *p != '\n' && *p != '\0') {
        if (w - item->word < (int)sizeof(item->word) - 1) *w++ = *p;
        p++;
    }
    *w = '\0';

    // 3. 提取【音标】
    while (*p == ' ' || *p == '\t') p++; 
    
    char *ph = item->phonetic;
    if (*p == '[') {
        p++; 
        while (*p != ']' && *p != '\r' && *p != '\n' && *p != '\0') {
            if (ph - item->phonetic < (int)sizeof(item->phonetic) - 1) *ph++ = *p;
            p++;
        }
        if (*p == ']') p++; 
        while (*p == ' ' || *p == '\t') p++; 
    }
    *ph = '\0';

    // 4. 提取【中文释义】(并进行空格清洗和精准词性换行)
    char *m = item->meaning;
    bool last_was_space = true; 
    bool first_char = true;

    while (*p != '\r' && *p != '\n' && *p != '\0') {
        if (*p == '\t' || *p == ' ') {
            if (!last_was_space && !first_char) {
                if (m - item->meaning < (int)sizeof(item->meaning) - 1) *m++ = ' ';
                last_was_space = true;
            }
        } else {
            // 检查是否遇到了新的词性标识
            if (!first_char && is_pos_marker(p, line)) {
                // 如果前一个是空格，直接替换为换行；否则插入一个换行符
                if (m > item->meaning && *(m - 1) == ' ') {
                    *(m - 1) = '\n';
                } else if (m - item->meaning < (int)sizeof(item->meaning) - 1) {
                    *m++ = '\n';
                }
            }

            if (m - item->meaning < (int)sizeof(item->meaning) - 1) *m++ = *p;
            
            // 标点处理：保留原有标点后加空格的逻辑
            if (*p == ';' || *p == ',') {
                if (m - item->meaning < (int)sizeof(item->meaning) - 1) *m++ = ' ';
                last_was_space = true;
            } else {
                last_was_space = false;
            }
            first_char = false;
        }
        p++;
    }
    
    // 去除末尾多余的空格
    if (m > item->meaning && *(m - 1) == ' ') m--;
    *m = '\0';
}

// ================== 公开接口 ==================

bool wordlist_open(wordlist_t *wl, const char *filepath)
{
    if (wl == NULL || filepath == NULL) return false;

    memset(wl, 0, sizeof(wordlist_t));

    if (f_open(&wl->file, filepath, FA_READ) != FR_OK) {
        return false;
    }

    uint8_t *scan_buf = (uint8_t *)malloc_bsc(SCAN_BUF_SIZE);
    if (!scan_buf) {
        f_close(&wl->file);
        return false;
    }

    uint32_t line_count = 0;
    UINT br;
    bool is_newline = true; 

    f_read(&wl->file, scan_buf, 3, &br);
    if (br == 3 && scan_buf[0] == 0xEF && scan_buf[1] == 0xBB && scan_buf[2] == 0xBF) {
        // 是BOM头
    } else {
        f_lseek(&wl->file, 0); 
    }

    while (f_read(&wl->file, scan_buf, SCAN_BUF_SIZE, &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            if (scan_buf[i] == '\n') {
                is_newline = true;
            } else if (is_newline && scan_buf[i] != '\r') {
                line_count++;
                is_newline = false;
            }
        }
    }

    if (line_count == 0) {
        free_bsc(scan_buf);
        f_close(&wl->file);
        return false;
    }

    wl->total_words = line_count;

    wl->line_offsets = (uint32_t *)malloc_bsc(wl->total_words * sizeof(uint32_t));
    if (!wl->line_offsets) {
        free_bsc(scan_buf);
        f_close(&wl->file);
        return false;
    }
    
    f_lseek(&wl->file, 0);
    f_read(&wl->file, scan_buf, 3, &br);
    if (!(br == 3 && scan_buf[0] == 0xEF && scan_buf[1] == 0xBB && scan_buf[2] == 0xBF)) {
        f_lseek(&wl->file, 0);
    }

    uint32_t current_offset = f_tell(&wl->file);
    uint32_t current_line = 0;
    is_newline = true;

    while (f_read(&wl->file, scan_buf, SCAN_BUF_SIZE, &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            if (scan_buf[i] == '\n') {
                is_newline = true;
            } else if (is_newline && scan_buf[i] != '\r') {
                if (current_line < wl->total_words) {
                    wl->line_offsets[current_line++] = current_offset + i;
                }
                is_newline = false;
            }
        }
        current_offset += br;
    }

    free_bsc(scan_buf);
    wl->current_index = 0;
    return true;
}

void wordlist_close(wordlist_t *wl)
{
    if (wl == NULL) return;
    
    f_close(&wl->file);
    
    if (wl->line_offsets) {
        free_bsc(wl->line_offsets);
        wl->line_offsets = NULL;
    }
    wl->total_words = 0;
}

bool wordlist_get_by_index(wordlist_t *wl, uint32_t index, word_item_t *item)
{
    if (wl == NULL || wl->line_offsets == NULL || index >= wl->total_words) return false;

    f_lseek(&wl->file, wl->line_offsets[index]);

    char line_buf[256];
    if (f_gets(line_buf, sizeof(line_buf), &wl->file) == NULL) {
        return false;
    }

    parse_word_line(line_buf, item);
    return true;
}

bool wordlist_get_sequential(wordlist_t *wl, word_item_t *item)
{
    if (wl == NULL || wl->total_words == 0) return false;

    bool res = wordlist_get_by_index(wl, wl->current_index, item);
    
    if (res) {
        wl->current_index++;
        if (wl->current_index >= wl->total_words) {
            wl->current_index = 0; 
        }
    }
    return res;
}

bool wordlist_get_random(wordlist_t *wl, word_item_t *item)
{
    if (wl == NULL || wl->total_words == 0) return false;
    uint32_t rand_index = rand() % wl->total_words;
    return wordlist_get_by_index(wl, rand_index, item);
}

void wordlist_reset_sequential(wordlist_t *wl)
{
    if (wl) {
        wl->current_index = 0;
    }
}
