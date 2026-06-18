#include "stm32f4xx.h"

#ifndef __FILE_UNIT_H__
#define __FILE_UNIT_H__

void Create_File_Unit(void);

void Update_File_Unit(void);

void Remove_File_Unit(void);

void chosen_file_path_free(void);

// 底层文件操作引擎 (供后台任务调用)
int do_copy(const char* src, const char* dst);
int do_delete(const char* path);

#endif
