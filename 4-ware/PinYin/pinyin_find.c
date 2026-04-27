#include <string.h>
#include "pinyin_find.h"
#include "pinyin_dict.h"

const char * pinyin_lookup(const char *py)
{
    static int dict_size = 0;
    if (dict_size == 0) {
        while (pinyin_dict[dict_size].py != NULL) {
            dict_size++;
        }
    }

    int low = 0;
    int high = dict_size - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = strcmp(py, pinyin_dict[mid].py);
        if (cmp == 0) {
            return pinyin_dict[mid].py_mb;   // 找到对应拼音，返回汉字串
        } else if (cmp < 0) {
            high = mid - 1;                  // 目标在左半区
        } else {
            low = mid + 1;                   // 目标在右半区
        }
    }
    return NULL;                             // 未找到
}
