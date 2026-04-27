#ifndef PINYIN_DICT_H
#define PINYIN_DICT_H

typedef struct
{
	const char * const py;
	const char * const py_mb;
} pinyin_dict_t;

extern pinyin_dict_t pinyin_dict[];

#endif

