#include "stm32f4xx.h" 
#include "mem_monitor_page.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "malloc.h"
#include "debug_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "status_bar.h"

void Create_Debug_Page(void)
{
	Create_Debug_Unit();
	Create_Navigation_Bar("系统日志");
	Create_Status_Bar();
}

void Update_Debug_Page(void)
{
	Update_Debug_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Debug_Page(void)
{
	Remove_Debug_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_debug_interface = {
    .id = PAGE_DEBUG,
    .init = Create_Debug_Page,
    .update = Update_Debug_Page,
    .exit = Remove_Debug_Page,
};
