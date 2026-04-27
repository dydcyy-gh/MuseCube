#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "file_unit.h"

void Create_File_Page(void)
{
	Create_File_Unit();
	Create_Navigation_Bar("文件管理");
	Create_Status_Bar();
}

void Update_File_Page(void)
{
	Update_File_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_File_Page(void)
{
	Remove_File_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_file_interface = {
    .id = PAGE_FILE,
    .init = Create_File_Page,
    .update = Update_File_Page,
    .exit = Remove_File_Page,
};
