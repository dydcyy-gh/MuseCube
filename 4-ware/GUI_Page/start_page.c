#include "stm32f4xx.h" 
#include "page_manager.h"
#include "start_unit.h"

static void Start_Page_Init(void)
{
    Create_Start_Unit();
}

static void Start_Page_Update(void)
{
    Update_Start_Unit();
}

static void Start_Page_Exit(void)
{
    Remove_Start_Unit();
}

// 页面接口定义
const Page_Interface_t page_start_interface = {
    .id = PAGE_START,
    .init = Start_Page_Init,
    .update = Start_Page_Update,
    .exit = Start_Page_Exit,
};
