#include "stm32f4xx.h" 
#include "mem_monitor_page.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "malloc.h"
#include "usb_ctrl_unit.h"
#include "status_bar.h"
#include "navigation_bar.h"

void Create_USB_Page(void)
{
	Create_USB_Unit();
	Create_Navigation_Bar("USB设置");
}

void Update_USB_Page(void)
{
	Update_USB_Unit();
	Update_Navigation_Bar();
}

void Remove_USB_Page(void)
{
	Remove_USB_Unit();
	Remove_Navigation_Bar();
}

// 导出页面接口
Page_Interface_t page_usb_interface = {
    .id = PAGE_USB_CTRL,
    .init = Create_USB_Page,
    .update = Update_USB_Page,
    .exit = Remove_USB_Page,
	.state = PAGE_STATE_UNINIT
};
