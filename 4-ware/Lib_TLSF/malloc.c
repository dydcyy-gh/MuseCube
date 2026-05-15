#include "stm32f4xx.h"
#include <stdlib.h>
#include <string.h>
#include "tlsf.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "systick_conf.h"
#include "variables.h"

//warning:不能在中断中调用malloc/free函数

// ===== 初始化配置 ===== 
#define TLSF_CTRL_SIZE   (864)
#define TLSF_BSC_SIZE    (89 * 1024)
#define TLSF_CCM_SIZE    (64 * 1024 - TLSF_CTRL_SIZE * 2)

//0x10000000 864B (in ccm ram)
#define BSC_CTRL_ADDR 0x10000000
//0x2000E000 72kB (in basic ram)
#define BSC_POOL_ADDR 0x20007000
//0x10000360 864B (in ccm ram)
#define CCM_CTRL_ADDR 0x10000360
//0x100006C0 64KB - 864*2 byte (in ccm ram)
#define CCM_POOL_ADDR 0x100006C0

#define TLSF_S(x) __attribute__((at(x)))
#define TLSF_A(x) __align(x)

TLSF_A(32) TLSF_S(BSC_CTRL_ADDR) uint8_t tlsf_ctrl_bsc[TLSF_CTRL_SIZE];
TLSF_A(32) 						 uint8_t tlsf_pool_bsc[TLSF_BSC_SIZE];

TLSF_A(32) TLSF_S(CCM_CTRL_ADDR) uint8_t tlsf_ctrl_ccm[TLSF_CTRL_SIZE];
TLSF_A(32) TLSF_S(CCM_POOL_ADDR) uint8_t tlsf_pool_ccm[TLSF_CCM_SIZE];

tlsf_t tlsf_bsc = NULL;
tlsf_t tlsf_ccm = NULL;

pool_t bsc_pool = NULL;
pool_t ccm_pool = NULL;

// ===== TLSF 初始化 ===== 
uint8_t tlsf_init(void)
{
    tlsf_bsc = tlsf_create(tlsf_ctrl_bsc);
    if (tlsf_bsc == NULL) return 1;

    tlsf_ccm = tlsf_create(tlsf_ctrl_ccm);
    if (tlsf_ccm == NULL) return 2;

    bsc_pool = tlsf_add_pool(tlsf_bsc, tlsf_pool_bsc, TLSF_BSC_SIZE);
    if (bsc_pool == NULL) return 3;

    ccm_pool = tlsf_add_pool(tlsf_ccm, tlsf_pool_ccm, TLSF_CCM_SIZE);
    if (ccm_pool == NULL) return 4;

	xBSCMutex = xSemaphoreCreateMutex();
	xCCMMutex = xSemaphoreCreateMutex();
	
	if (xBSCMutex == NULL || xCCMMutex == NULL) return 5;
	
    return 0;
}

// ===== BSC (Basic SRAM) 池相关函数 ===== 
void *malloc_bsc(uint32_t bytes)
{
    void *ptr = NULL;
	
	if(RTOS_OK) xSemaphoreTake(xBSCMutex, portMAX_DELAY);
	ptr = tlsf_malloc(tlsf_bsc, bytes);
	if(RTOS_OK) xSemaphoreGive(xBSCMutex);
	
    return ptr;
}

void free_bsc(void* ptr)
{
    if(RTOS_OK) xSemaphoreTake(xBSCMutex, portMAX_DELAY);
    tlsf_free(tlsf_bsc, ptr);
    if(RTOS_OK) xSemaphoreGive(xBSCMutex);
}

void *realloc_bsc(void* ptr, uint32_t bytes)
{
    void *new_ptr = NULL;
    
    if(RTOS_OK) xSemaphoreTake(xBSCMutex, portMAX_DELAY);
	new_ptr = tlsf_realloc(tlsf_bsc, ptr, bytes);
	if(RTOS_OK) xSemaphoreGive(xBSCMutex);
	
    return new_ptr;
}

// ===== CCM (Core Coupled Memory) 池相关函数 ===== 
void *malloc_ccm(uint32_t bytes)
{
    void *ptr = NULL;
    
	if(RTOS_OK) xSemaphoreTake(xCCMMutex, portMAX_DELAY);
	ptr = tlsf_malloc(tlsf_ccm, bytes);
	if(RTOS_OK) xSemaphoreGive(xCCMMutex);
	
    return ptr;
}

void free_ccm(void* ptr)
{
	if(RTOS_OK) xSemaphoreTake(xCCMMutex, portMAX_DELAY);
	tlsf_free(tlsf_ccm, ptr);
	if(RTOS_OK) xSemaphoreGive(xCCMMutex);
}

void *realloc_ccm(void* ptr, uint32_t bytes)
{
    void *new_ptr = NULL;
    if(RTOS_OK) xSemaphoreTake(xCCMMutex, portMAX_DELAY);
	new_ptr = tlsf_realloc(tlsf_ccm, ptr, bytes);
	if(RTOS_OK) xSemaphoreGive(xCCMMutex);
    return new_ptr;
}

// ===== 内存遍历回调函数 ===== 
static void tlsf_mem_walker(void* ptr, size_t size, int used, void* user)
{
    (void)ptr;  // 未使用参数
    
    mem_monitor_t* mon = (mem_monitor_t*)user;
    
    mon->total_size += size;// 总大小累加
    
    if(used) {
        // 已使用块
        mon->used_cnt++;
        mon->used_size += size;
    } else {
        // 空闲块
        mon->free_cnt++;
        mon->free_size += size;
        // 记录最大的空闲块
        if(size > mon->free_biggest_size)
            mon->free_biggest_size = size;
    }
}

// ===== 获取 BSC 池的内存监控信息 ===== 
void tlsf_monitor_bsc(mem_monitor_t* mon)
{
	if(RTOS_OK) xSemaphoreTake(xBSCMutex, portMAX_DELAY);
	
    if (mon == NULL || bsc_pool == NULL) return;

    // 初始化监控数据
    memset(mon, 0, sizeof(mem_monitor_t));
    
    // 遍历 BSC 池中的所有内存块
    tlsf_walk_pool(bsc_pool, tlsf_mem_walker, mon);
    
    // 计算使用百分比
    if(mon->total_size > 0) 
		mon->used_pct = 100 - ((100U * mon->free_size) / mon->total_size);
    else  
		mon->used_pct = 0;
    
    // 计算碎片率：碎片率 = 1 - (最大空闲块 / 总空闲)
    if(mon->free_size > 0)
        mon->frag_pct = 100 - (mon->free_biggest_size * 100U / mon->free_size);
    else
        mon->frag_pct = 0;
	
	if(RTOS_OK) xSemaphoreGive(xBSCMutex);
}

// ===== 获取 CCM 池的内存监控信息 ===== 
void tlsf_monitor_ccm(mem_monitor_t* mon)
{
	if(RTOS_OK) xSemaphoreTake(xCCMMutex, portMAX_DELAY);
	
    if (mon == NULL || ccm_pool == NULL) return;

    // 初始化监控数据
    memset(mon, 0, sizeof(mem_monitor_t));
    
    // 遍历 CCM 池中的所有内存块
    tlsf_walk_pool(ccm_pool, tlsf_mem_walker, mon);

    // 计算使用百分比
    if(mon->total_size > 0)
		mon->used_pct = 100 - ((100U * mon->free_size) / mon->total_size);
    else
        mon->used_pct = 0;
    
    // 计算碎片率
    if(mon->free_size > 0)
        mon->frag_pct = 100 - (mon->free_biggest_size * 100U / mon->free_size);
    else
        mon->frag_pct = 0;
	
	if(RTOS_OK) xSemaphoreGive(xCCMMutex);
}

// ===== 获取合并的内存监控信息 ===== 
void tlsf_monitor_all(mem_monitor_t* mon)
{
    if (mon == NULL) return;
    
    mem_monitor_t bsc_mon, ccm_mon;
    
    // 获取 BSC 池的内存监控信息
    tlsf_monitor_bsc(&bsc_mon);
    
    // 获取 CCM 池的内存监控信息
    tlsf_monitor_ccm(&ccm_mon);
    
    // 合并两个池的信息到 mon_total
    memset(mon, 0, sizeof(mem_monitor_t));
    
    mon->total_size = bsc_mon.total_size + ccm_mon.total_size;
    mon->free_cnt = bsc_mon.free_cnt + ccm_mon.free_cnt;
    mon->free_size = bsc_mon.free_size + ccm_mon.free_size;
    mon->used_cnt = bsc_mon.used_cnt + ccm_mon.used_cnt;
    mon->used_size = bsc_mon.used_size + ccm_mon.used_size;
    
    // 记录最大的空闲块（比较两个池中最大的空闲块）
    mon->free_biggest_size = (bsc_mon.free_biggest_size > ccm_mon.free_biggest_size) 
                             ? bsc_mon.free_biggest_size : ccm_mon.free_biggest_size;
    
    // 计算总的使用百分比
    if(mon->total_size > 0)
        mon->used_pct = 100 - ((100U * mon->free_size) / mon->total_size);
    else
        mon->used_pct = 0;

    // 计算总的碎片率
    if(mon->free_size > 0)
        mon->frag_pct = 100 - (mon->free_biggest_size * 100U / mon->free_size);
    else
        mon->frag_pct = 0;
}
