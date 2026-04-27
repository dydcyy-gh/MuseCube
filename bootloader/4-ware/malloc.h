#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

// ===== TLSF 初始化 =====
uint8_t tlsf_init(void);

// ===== CCM (Core Coupled Memory) 池相关函数 =====
void *malloc_ccm(uint32_t bytes);
void free_ccm(void* ptr);
void *realloc_ccm(void* ptr, uint32_t bytes);

#endif
