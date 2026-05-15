#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "calendar_unit.h"

void Create_Calendar_Page(void)
{
	Create_Calendar_Unit();
	Create_Navigation_Bar("日历");
	Create_Status_Bar();
}

void Update_Calendar_Page(void)
{
	Update_Calendar_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Calendar_Page(void)
{
	Remove_Calendar_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_calendar_interface = {
    .id = PAGE_CALENDAR,
    .init = Create_Calendar_Page,
    .update = Update_Calendar_Page,
    .exit = Remove_Calendar_Page,
};
