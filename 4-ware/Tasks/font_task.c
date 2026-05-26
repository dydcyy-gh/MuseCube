#include "FreeRTOS.h"
#include "task.h"
#include "fontupd.h"
#include "pin_ctrl.h"
#include "sdio_sdcard.h"
#include "fatfs.h"
#include "defines.h"
#include "variables.h"
#include "page_manager.h"

void Font_Task(void *pvParameters)
{
	if (!g_font_need_update)
	{
		Font_Task_handler = NULL;
		Font_Task_Status = Task_P_Null;
		vTaskDelete(NULL);
		return;
	}

	while (!g_TFcard_inited)
	{
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	uint8_t res = update_font((uint8_t*)"0:");

	if (!res)
	{
		g_font_need_update = 0;
		Page_Request_Switch(PAGE_START);
	}

	Font_Task_handler = NULL;
	Font_Task_Status = Task_P_Null;
	vTaskDelete(NULL);
}
