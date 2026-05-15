#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "es9018_unit.h"

void Create_ES9018_Page(void)
{
	Create_ES9018_Unit();
	Create_Navigation_Bar("DAC设置");
	Create_Status_Bar();
}

void Update_ES9018_Page(void)
{
	Update_ES9018_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_ES9018_Page(void)
{
	Remove_ES9018_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_es9018_interface = {
    .id = PAGE_ES9018,
    .init = Create_ES9018_Page,
    .update = Update_ES9018_Page,
    .exit = Remove_ES9018_Page,
};
