#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "time_set_unit.h"

void Create_Time_Set_Page(void)
{
	Create_Time_Set_Unit();
	Create_Navigation_Bar("日期与时间");
	Create_Status_Bar();
}

void Update_Time_Set_Page(void)
{
	Update_Time_Set_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Time_Set_Page(void)
{
	Remove_Time_Set_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_time_set_interface = {
    .id = PAGE_TIME_SET,
    .init = Create_Time_Set_Page,
    .update = Update_Time_Set_Page,
    .exit = Remove_Time_Set_Page,
};
