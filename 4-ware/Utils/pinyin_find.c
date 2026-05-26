#include <string.h>
#include "pinyin_find.h"
#include "pinyin_dict.h"
#include "malloc.h"

#define PINYIN_BUF_SIZE 256

static int   g_dict_size = 0;
static char *g_result    = NULL;

const char * pinyin_lookup(const char *py)
{
    if (g_dict_size == 0) {
        while (pinyin_dict[g_dict_size].py != NULL) {
            g_dict_size++;
        }
    }

    int low = 0;
    int high = g_dict_size - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = strcmp(py, pinyin_dict[mid].py);
        if (cmp == 0) {
            return pinyin_dict[mid].py_mb;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return NULL;
}

const char * pinyin_lookup_prefix(const char *py)
{
    if (py == NULL || py[0] == '\0') return NULL;
    if (g_result == NULL) return NULL;

    if (g_dict_size == 0) {
        while (pinyin_dict[g_dict_size].py != NULL) {
            g_dict_size++;
        }
    }

    int result_len = 0;
    int prefix_len = strlen(py);

    for (int i = 0; i < g_dict_size; i++) {
        if (strncmp(py, pinyin_dict[i].py, prefix_len) != 0) continue;

        const char *mb = pinyin_dict[i].py_mb;
        if (mb && mb[0]) {
            if (result_len + 3 >= PINYIN_BUF_SIZE - 1) break;
            memcpy(g_result + result_len, mb, 3);
            result_len += 3;
        }
    }

    if (result_len == 0) return NULL;
    g_result[result_len] = '\0';
    return g_result;
}

// 模糊音前缀匹配：input 是否模糊匹配 target 的前缀
// 覆盖 zh/z ch/c sh/s（翘舌/平舌），n/l（声母），an/ang en/eng in/ing（前后鼻音）
static int fuzzy_prefix_match(const char *input, const char *target)
{
    int i = 0, j = 0;
    int past_vowel = 0;

    while (input[i] && target[j]) {
        if (input[i] == 'a' || input[i] == 'e' || input[i] == 'i' ||
            input[i] == 'o' || input[i] == 'u' || input[i] == 'v') {
            past_vowel = 1;
        }

        // 翘舌/平舌声母对：zh↔z, ch↔c, sh↔s
        if ((input[i] == 'z' || input[i] == 'c' || input[i] == 's') &&
            (target[j] == 'z' || target[j] == 'c' || target[j] == 's')) {
            if (input[i] != target[j]) return 0;
            int ih = (input[i + 1] == 'h');
            int jh = (target[j + 1] == 'h');
            i += ih ? 2 : 1;
            j += jh ? 2 : 1;
            continue;
        }

        // 声母 n↔l 模糊（仅在遇到元音之前生效）
        if (!past_vowel && (input[i] == 'n' || input[i] == 'l') &&
            (target[j] == 'n' || target[j] == 'l')) {
            i++; j++;
            continue;
        }

        // 韵尾鼻音 n↔ng 模糊（仅在遇到过元音之后生效）
        if (past_vowel && input[i] == 'n' && target[j] == 'n') {
            int ig = (input[i + 1] == 'g');
            int jg = (target[j + 1] == 'g');
            i += ig ? 2 : 1;
            j += jg ? 2 : 1;
            continue;
        }

        if (input[i] == target[j]) {
            i++; j++;
        } else {
            return 0;
        }
    }

    return input[i] == '\0';
}

const char * pinyin_lookup_fuzzy(const char *py)
{
    if (py == NULL || py[0] == '\0') return NULL;
    if (g_result == NULL) return NULL;

    if (g_dict_size == 0) {
        while (pinyin_dict[g_dict_size].py != NULL) {
            g_dict_size++;
        }
    }

    int result_len = 0;

    for (int i = 0; i < g_dict_size; i++) {
        if (!fuzzy_prefix_match(py, pinyin_dict[i].py)) continue;

        const char *mb = pinyin_dict[i].py_mb;
        if (mb && mb[0]) {
            if (result_len + 3 >= PINYIN_BUF_SIZE - 1) break;
            memcpy(g_result + result_len, mb, 3);
            result_len += 3;
        }
    }

    if (result_len == 0) return NULL;
    g_result[result_len] = '\0';
    return g_result;
}

int pinyin_buf_init(void)
{
    if (g_result != NULL) return 1;  // 已分配
    g_result = (char *)malloc_bsc(PINYIN_BUF_SIZE);
    return (g_result != NULL) ? 1 : 0;
}

void pinyin_buf_deinit(void)
{
    if (g_result != NULL) {
        free_bsc(g_result);
        g_result = NULL;
    }
}
