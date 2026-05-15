#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "status_bar.h"
#include "navigation_bar.h"
#include "words_unit.h"

void Create_Words_Page(void)
{
	Create_Words_Unit();
	Create_Navigation_Bar("背单词");
	Create_Status_Bar();
}

void Update_Words_Page(void)
{
	Update_Words_Unit();
	Update_Navigation_Bar();
	Update_Status_Bar();
}

void Remove_Words_Page(void)
{
	Remove_Words_Unit();
	Remove_Navigation_Bar();
	Remove_Status_Bar();
}

// 导出页面接口
const Page_Interface_t page_words_interface = {
    .id = PAGE_WORDS,
    .init = Create_Words_Page,
    .update = Update_Words_Page,
    .exit = Remove_Words_Page,
};
