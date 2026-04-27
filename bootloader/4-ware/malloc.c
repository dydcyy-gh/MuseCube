#include "stm32f4xx.h"
#include <stdlib.h>
#include <string.h>
#include "tlsf.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "systick_conf.h"

//warning:不能在中断中调用malloc/free函数

// ===== 初始化配置 ===== 
#define TLSF_CTRL_SIZE   (1024)   // 1KB for control block
#define TLSF_CCM_SIZE    (64 * 1024 - TLSF_CTRL_SIZE) // 63KB

#define CCM_CTRL_ADDR 0x10000000
#define CCM_POOL_ADDR 0x10000400

#define TLSF_S(x) __attribute__((at(x)))
#define TLSF_A(x) __align(x)

TLSF_A(32) TLSF_S(CCM_CTRL_ADDR) uint8_t tlsf_ctrl_ccm[TLSF_CTRL_SIZE];
TLSF_A(32) TLSF_S(CCM_POOL_ADDR) uint8_t tlsf_pool_ccm[TLSF_CCM_SIZE];

tlsf_t tlsf_ccm = NULL;
pool_t ccm_pool = NULL;

// ===== freertos 安全配置 =====
SemaphoreHandle_t xCCMMutex = NULL;

// ===== TLSF 初始化 ===== 
uint8_t tlsf_init(void)
{
    tlsf_ccm = tlsf_create(tlsf_ctrl_ccm);
    if (tlsf_ccm == NULL) return 1;

    ccm_pool = tlsf_add_pool(tlsf_ccm, tlsf_pool_ccm, TLSF_CCM_SIZE);
    if (ccm_pool == NULL) return 2;

    xCCMMutex = xSemaphoreCreateMutex();
    if (xCCMMutex == NULL) return 3;
	
    return 0;
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
