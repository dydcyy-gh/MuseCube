#include "stm32f4xx.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "font_update_unit.h"

void Create_Font_Update_Page(void)
{
    Create_Font_Update_Unit();
}

void Update_Font_Update_Page(void)
{
    Update_Font_Update_Unit();
}

void Remove_Font_Update_Page(void)
{
    Remove_Font_Update_Unit();
}

const Page_Interface_t page_font_update_interface = {
    .id = PAGE_FONT_UPDATE,
    .init = Create_Font_Update_Page,
    .update = Update_Font_Update_Page,
    .exit = Remove_Font_Update_Page,
};
