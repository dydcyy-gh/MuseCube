#include "stm32f4xx.h" 
#include "mem_monitor_page.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "malloc.h"
#include "log_ctrl_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "status_bar.h"

void Create_Log_Ctrl_Page(void)
{
	Create_Log_Control();
	Create_Navigation_Bar("LOG设置");
	Create_Status_Bar();
}

void Update_Log_Ctrl_Page(void)
{
	Update_Log_Control();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Log_Ctrl_Page(void)
{
	Remove_Log_Control();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_log_ctrl_interface = {
    .id = PAGE_LOG_CTRL,
    .init = Create_Log_Ctrl_Page,
    .update = Update_Log_Ctrl_Page,
    .exit = Remove_Log_Ctrl_Page,
};
