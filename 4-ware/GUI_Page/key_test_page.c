#include "stm32f4xx.h" 
#include "page_manager.h"
#include "key_test_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "status_bar.h"

static void Main_Page_Init(void)
{
	Create_Stick_Label_L();
	Create_Stick_Label_R();
	Create_Button_Label();
	Create_Navigation_Bar("外设状态");
	Create_Status_Bar();
}

static void Main_Page_Update(void)
{
	Update_Stick_Label_L();
	Update_Stick_Label_R();
	Update_Button_Label();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

static void Main_Page_Exit(void)
{
	Remove_Stick_Label_L();
	Remove_Stick_Label_R();
	Remove_Button_Label();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 页面接口定义
const Page_Interface_t page_key_test_interface = {
    .id = PAGE_KEY_TEST,
    .init = Main_Page_Init,
    .update = Main_Page_Update,
    .exit = Main_Page_Exit,
};
