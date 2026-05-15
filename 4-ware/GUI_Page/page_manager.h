#ifndef __PAGE_MANAGER_H
#define __PAGE_MANAGER_H

#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h> // 需要用到 NULL

// 页面状态枚举
typedef enum {
    PAGE_STATE_UNINIT = 0,  // 未初始化
    PAGE_STATE_INIT,        // 已初始化
    PAGE_STATE_ACTIVE,      // 活跃中
} Page_State_t;

// 页面接口结构体
typedef struct {
    uint32_t id;            // 页面ID
    void (*init)(void);     // 创建页面UI
    void (*update)(void);   // 刷新页面数据
    void (*exit)(void);     // 销毁页面对象
} Page_Interface_t;

// 页面ID定义 (保持连续，作为数组下标)
#define PAGE_NONE   	 0
#define PAGE_START   	 1
#define PAGE_DESKTOP     2
#define PAGE_MEM     	 3
#define PAGE_KEY_TEST 	 4
#define PAGE_MUSIC    	 5
#define PAGE_DEBUG    	 6
#define PAGE_LOG_CTRL    7
#define PAGE_USB_CTRL    8
#define PAGE_NOTE        9
#define PAGE_FILE        10
#define PAGE_GAME        11
#define PAGE_VIDEO       12
#define PAGE_DISPLAY     13
#define PAGE_SETTINGS    14
#define PAGE_TEXT        15
#define PAGE_CANVAS      16
#define PAGE_ABOUT       17
#define PAGE_ES9018      18
#define PAGE_TIME_SET    19
#define PAGE_CLOCK       20
#define PAGE_CALENDAR    21
#define PAGE_WORDS       22
#define PAGE_CALCULATOR  23
#define PAGE_CMD         24
#define PAGE_LOTS        25
#define PAGE_ALBUM       26
#define PAGE_MAX_ID      27

// 历史记录栈深度
#define PAGE_HISTORY_MAX_DEPTH 6

// 页面管理控制函数
void Page_Manager_Init(void);
void Page_Manager_Loop(void);
uint32_t Page_Get_Current(void);
void Page_Manager_Deinit(void);

// 内部实现函数（不要直接调用，使用可变参数吸收多余的NULL）
void _Page_Request_Switch_Impl(uint32_t new_page_id, const char *path, ...);

// 核心宏：通过在参数末尾追加 NULL，巧妙解决 1个 或 2个 参数的重载问题
#define Page_Request_Switch(...) _Page_Request_Switch_Impl(__VA_ARGS__, NULL)

// 后退与历史管理函数
void Page_Back(void);
void Page_Clear_History(void);

#endif
