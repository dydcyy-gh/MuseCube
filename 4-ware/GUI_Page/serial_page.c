#include "stm32f4xx.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "malloc.h"
#include "serial_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"

static void Serial_Page_Init(void)
{
	Create_Serial_Unit();
	Create_Status_Bar();
}

static void Serial_Page_Update(void)
{
	Update_Serial_Unit();
	Update_Status_Bar();
}

static void Serial_Page_Exit(void)
{
	Remove_Serial_Unit();
	Remove_Status_Bar();
}

const Page_Interface_t page_serial_interface = {
	.id = PAGE_SERIAL,
	.init = Serial_Page_Init,
	.update = Serial_Page_Update,
	.exit = Serial_Page_Exit,
};
