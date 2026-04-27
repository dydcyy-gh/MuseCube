#include "stm32f4xx.h" 
#include "page_manager.h"
#include "music_unit.h"
#include "status_bar.h"

static void Music_Page_Init(void)
{
	Create_Music_Unit();
	Create_Status_Bar();
}

static void Music_Page_Update(void)
{
	Update_Music_Unit();
	Update_Status_Bar();
}

static void Music_Page_Exit(void)
{
	Remove_Music_Unit();
	Remove_Status_Bar();
}

// 页面接口定义
const Page_Interface_t page_music_interface = {
    .id = PAGE_MUSIC,
    .init = Music_Page_Init,
    .update = Music_Page_Update,
    .exit = Music_Page_Exit,
};
