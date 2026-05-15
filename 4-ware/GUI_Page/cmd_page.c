#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "cmd_unit.h"

void Create_Cmd_Page(void)
{
	Create_Cmd_Unit();
	Create_Status_Bar();
}

void Update_Cmd_Page(void)
{
	Update_Cmd_Unit();
	Update_Status_Bar();
}

void Remove_Cmd_Page(void)
{
	Remove_Cmd_Unit();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_cmd_interface = {
    .id = PAGE_CMD,
    .init = Create_Cmd_Page,
    .update = Update_Cmd_Page,
    .exit = Remove_Cmd_Page,
};
