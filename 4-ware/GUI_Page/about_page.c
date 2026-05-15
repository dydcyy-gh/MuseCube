#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "about_unit.h"

void Create_About_Page(void)
{
	Create_About_Unit();
	Create_Navigation_Bar("关于本机信息");
	Create_Status_Bar();
}

void Update_About_Page(void)
{
	Update_About_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_About_Page(void)
{
	Remove_About_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_about_interface = {
    .id = PAGE_ABOUT,
    .init = Create_About_Page,
    .update = Update_About_Page,
    .exit = Remove_About_Page,
};
