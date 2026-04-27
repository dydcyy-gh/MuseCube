#include "stm32f4xx.h" 
#include "page_manager.h"
#include "desktop_unit.h"
#include "status_bar.h"

static void Desktop_Page_Init(void)
{
	Create_Desktop_Unit();
	Create_Status_Bar();
}

static void Desktop_Page_Update(void)
{
	Update_Desktop_Unit();
	Update_Status_Bar();
}

static void Desktop_Page_Exit(void)
{
	Remove_Desktop_Unit();
	Remove_Status_Bar();
}

// 页面接口定义
const Page_Interface_t page_desktop_interface = {
    .id = PAGE_DESKTOP,
    .init = Desktop_Page_Init,
    .update = Desktop_Page_Update,
    .exit = Desktop_Page_Exit,
};
