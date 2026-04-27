#include "stm32f4xx.h"           
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define MAX_TASK_COUNT 10

/* 获取 CPU 使用率 (0~100.0) */
/* 依赖 FreeRTOSConfig.h 中 configGENERATE_RUN_TIME_STATS = 1 */
uint8_t Get_CPU_Usage(void)
{
    static uint32_t prev_total_runtime = 0;
    static uint32_t prev_idle_runtime = 0;

    TaskStatus_t task_status_array[MAX_TASK_COUNT];
    uint32_t total_runtime;
    volatile UBaseType_t task_count;
    
    /* 获取系统状态 (不需要 vTaskList 的文本格式化功能) */
    task_count = uxTaskGetSystemState(task_status_array, MAX_TASK_COUNT, &total_runtime);
    
    if (task_count == 0) return 0.0f;

    uint32_t idle_runtime = 0;

    /* 查找 IDLE 任务 */
    for(int i = 0; i < task_count; i++)
    {
        if(strcmp(task_status_array[i].pcTaskName, "IDLE") == 0)
        {
            idle_runtime = task_status_array[i].ulRunTimeCounter;
            break;
        }
    }

    /* 计算差值 */
    uint32_t delta_total = total_runtime - prev_total_runtime;
    uint32_t delta_idle = idle_runtime - prev_idle_runtime;

    /* 更新历史记录 */
    prev_total_runtime = total_runtime;
    prev_idle_runtime = idle_runtime;

    if(delta_total == 0) return 0.0f;

    /* CPU 使用率 = 1 - (空闲时间占比) */
    float usage = 100.0f * (1.0f - ((float)delta_idle / (float)delta_total));
    
    if(usage < 0.0f) usage = 0.0f;
    if(usage > 100.0f) usage = 100.0f;

    return (uint8_t)usage;
}
