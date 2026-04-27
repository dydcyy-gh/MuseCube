#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "defines.h"
#include "variables.h"

/* 已有任务句柄 (初始化为NULL以保证安全) */
TaskHandle_t    Basic_Task_handler = NULL;
TaskHandle_t    Lvgl_Task_handler  = NULL;
TaskHandle_t    USB_Task_handler   = NULL;
TaskHandle_t    Music_Task_handler = NULL;
TaskHandle_t    Video_Task_handler = NULL;
TaskHandle_t    Game_Task_handler  = NULL;

/* 任务函数声明 */
extern void Basic_Task( void * pvParameters );
extern void Lvgl_Task( void * pvParameters );
extern void USB_Task( void * pvParameters );
extern void Music_Task( void * pvParameters );
extern void Video_Task( void * pvParameters );
extern void Game_Task( void * pvParameters );

/* 任务控制块结构体 */
typedef struct {
    TaskFunction_t      task_func;      // 任务主函数
    const char *        task_name;      // 任务名称
    uint16_t            stack_size;     // 堆栈大小
    UBaseType_t         priority;       // 任务优先级
    TaskHandle_t *      handle_ptr;     // 任务句柄指针
    volatile uint8_t *  status_ptr;     // 任务状态变量指针 (连接 variables.c)
} Task_Registry_t;

/* 任务注册表 (数组索引必须与 Task_N_xxx 宏严格对应) */
static const Task_Registry_t Task_List[] = {
    [Task_N_Basic] = { Basic_Task, "Basic_Task", BASIC_STACK_SIZE, BASIC_PRIO, &Basic_Task_handler, &Basic_Task_Status },
    [Task_N_LVGL]  = { Lvgl_Task,  "LVGL_Task",  LVGL_STACK_SIZE,  LVGL_PRIO,  &Lvgl_Task_handler,  &LVGL_Task_Status  },
    [Task_N_USB]   = { USB_Task,   "USB_Task",   USB_STACK_SIZE,   USB_PRIO,   &USB_Task_handler,   &USB_Task_Status   },
    [Task_N_Music] = { Music_Task, "Music_Task", MUSIC_STACK_SIZE, MUSIC_PRIO, &Music_Task_handler, &Music_Task_Status },
    [Task_N_Video] = { Video_Task, "Video_Task", VIDEO_STACK_SIZE, VIDEO_PRIO, &Video_Task_handler, &Video_Task_Status },
    [Task_N_Game]  = { Game_Task,  "Game_Task",  GAME_STACK_SIZE,  GAME_PRIO,  &Game_Task_handler,  &Game_Task_Status  },
};

#define TASK_NUM_MAX (sizeof(Task_List) / sizeof(Task_List[0]))

/**
 * @brief 触发任务管理器，更改任务状态
 */
void Taskmanager_Ctrl(uint8_t task_num, uint8_t task_action, uint8_t IsFromISR)
{
    if (task_num >= TASK_NUM_MAX) return;

    *(Task_List[task_num].status_ptr) = task_action;

    if (IsFromISR) 
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xTaskManagerSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xSemaphoreGive(xTaskManagerSemaphore);
    }
}

/**
 * @brief 任务管理主线程
 */
void Task_Manager( void * pvParameters )
{
    while (1)
    {
        // 阻塞等待外部使能指令
        xSemaphoreTake(xTaskManagerSemaphore, portMAX_DELAY);

        // 遍历所有注册的任务，处理其状态机
        for (uint8_t i = 0; i < TASK_NUM_MAX; i++)
        {
            uint8_t current_action = *(Task_List[i].status_ptr);
            TaskHandle_t *pHandle = Task_List[i].handle_ptr;

            switch (current_action)
            {
                case Task_T_Creat:
                    if (*pHandle == NULL) {
                        xTaskCreate(Task_List[i].task_func,
                                    Task_List[i].task_name,
                                    Task_List[i].stack_size,
                                    NULL,
                                    Task_List[i].priority,
                                    pHandle);
                    }
                    *(Task_List[i].status_ptr) = Task_P_Running;
                    break;

                case Task_T_Suspend:
                    if (*pHandle != NULL) {
                        vTaskSuspend(*pHandle);
                    }
                    *(Task_List[i].status_ptr) = Task_P_Stop;
                    break;

                case Task_T_Resume:
                    if (*pHandle != NULL) {
                        vTaskResume(*pHandle);
                    }
                    *(Task_List[i].status_ptr) = Task_P_Running;
                    break;

                case Task_T_Delete:
                    if (*pHandle != NULL) {
                        vTaskDelete(*pHandle);
                        *pHandle = NULL; 
                    }
                    *(Task_List[i].status_ptr) = Task_P_Null;
                    break;

                default:
                    break;
            }
        }
    }
}
