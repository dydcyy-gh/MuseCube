#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "clock_unit.h"

void Create_Clock_Page(void)
{
	Create_Clock_Unit();
	Create_Navigation_Bar("闹钟时钟");
	Create_Status_Bar();
}

void Update_Clock_Page(void)
{
	Update_Clock_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Clock_Page(void)
{
	Remove_Clock_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_clock_interface = {
    .id = PAGE_CLOCK,
    .init = Create_Clock_Page,
    .update = Update_Clock_Page,
    .exit = Remove_Clock_Page,
};
