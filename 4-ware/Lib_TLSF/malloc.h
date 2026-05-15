#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

// ===== TLSF 内存监控信息结构体 ===== 
typedef struct {
    uint32_t total_size;        // 内存池总大小
    uint32_t free_cnt;          // 空闲块数量
    uint32_t free_size;         // 总空闲内存
    uint32_t free_biggest_size; // 最大空闲块
    uint32_t used_cnt;          // 已用块数量
    uint32_t used_size;         // 已用内存
    uint8_t  used_pct;          // 已用百分比
    uint8_t  frag_pct;          // 碎片率 (%)
} mem_monitor_t;

// ===== TLSF 初始化 =====
uint8_t tlsf_init(void);

// ===== BSC (Basic SRAM) 池相关函数 ===== 
void *malloc_bsc(uint32_t bytes);
void free_bsc(void* ptr);
void *realloc_bsc(void* ptr, uint32_t bytes);

// ===== CCM (Core Coupled Memory) 池相关函数 =====
void *malloc_ccm(uint32_t bytes);
void free_ccm(void* ptr);
void *realloc_ccm(void* ptr, uint32_t bytes);

// ===== 获取 BSC 池的内存监控信息 =====
void tlsf_monitor_bsc(mem_monitor_t* mon);
// ===== 获取 CCM 池的内存监控信息 =====
void tlsf_monitor_ccm(mem_monitor_t* mon);
// ===== 获取 合并的  内存监控信息 =====
void tlsf_monitor_all(mem_monitor_t* mon);

#endif
