#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "calculator_unit.h"

void Create_Calculator_Page(void)
{
	Create_Calculator_Unit();
	Create_Navigation_Bar("计算器");
	Create_Status_Bar();
}

void Update_Calculator_Page(void)
{
	Update_Calculator_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Calculator_Page(void)
{
	Remove_Calculator_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_calculator_interface = {
    .id = PAGE_CALCULATOR,
    .init = Create_Calculator_Page,
    .update = Update_Calculator_Page,
    .exit = Remove_Calculator_Page,
};
