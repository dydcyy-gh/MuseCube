#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "systick_conf.h"
#include "page_manager.h" 
#include "variables.h"
#include "defines.h"
#include "lvgl.h"

void Lvgl_Task( void * pvParameters )
{
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
    Page_Manager_Init();

    TickType_t xLastWakeTime = xTaskGetTickCount();
	
    while(1) 
    {
        Page_Manager_Loop();
        
        lv_timer_handler(); 
		
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}
