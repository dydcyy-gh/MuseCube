#ifndef PINYIN_FIND_H
#define PINYIN_FIND_H

const char * pinyin_lookup(const char *py);
const char * pinyin_lookup_prefix(const char *py);
const char * pinyin_lookup_fuzzy(const char *py);

int  pinyin_buf_init(void);
void pinyin_buf_deinit(void);

#endif
