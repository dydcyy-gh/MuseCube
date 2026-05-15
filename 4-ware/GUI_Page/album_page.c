#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "album_unit.h"

void Create_Album_Page(void)
{
	Create_Album_Unit();
	Create_Navigation_Bar("相册");
	Create_Status_Bar();
}

void Update_Album_Page(void)
{
	Update_Album_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Album_Page(void)
{
	Remove_Album_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_album_interface = {
    .id = PAGE_ALBUM,
    .init = Create_Album_Page,
    .update = Update_Album_Page,
    .exit = Remove_Album_Page,
};
