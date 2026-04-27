#include "stm32f4xx.h" 
#include "mem_monitor_page.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "malloc.h"
#include "mem_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "status_bar.h"

void Create_Mem_Monitor_Page(void)
{
	Create_Mem_Chart();
	Create_Navigation_Bar("任务管理器");
	Create_Status_Bar();
}

void Update_Mem_Monitor_Page(void)
{
	Update_Mem_Chart();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Mem_Monitor_Page(void)
{
	Remove_Mem_Chart();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_mem_interface = {
    .id = PAGE_MEM,
    .init = Create_Mem_Monitor_Page,
    .update = Update_Mem_Monitor_Page,
    .exit = Remove_Mem_Monitor_Page,
};
