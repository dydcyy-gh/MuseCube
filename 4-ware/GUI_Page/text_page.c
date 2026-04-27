#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "text_unit.h"

void Create_Text_Page(void)
{
	Create_Text_Unit();
	Create_Navigation_Bar("文件管理");
	Create_Status_Bar();
}

void Update_Text_Page(void)
{
	Update_Text_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Text_Page(void)
{
	Remove_Text_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_text_interface = {
    .id = PAGE_TEXT,
    .init = Create_Text_Page,
    .update = Update_Text_Page,
    .exit = Remove_Text_Page,
};
