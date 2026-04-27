#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "settings_unit.h"

void Create_Settings_Page(void)
{
	Create_Settings_Unit();
	Create_Navigation_Bar("设置");
	Create_Status_Bar();
}

void Update_Settings_Page(void)
{
	Update_Settings_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Settings_Page(void)
{
	Remove_Settings_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_settings_interface = {
    .id = PAGE_SETTINGS,
    .init = Create_Settings_Page,
    .update = Update_Settings_Page,
    .exit = Remove_Settings_Page,
};
