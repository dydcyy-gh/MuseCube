#include "stm32f4xx.h" 
#include "pin_ctrl.h"
#include "key.h"
#include "nvic_conf.h"
#include "usbd_msc_conf.h"
#include "bootuf2.h"
#include "systick_conf.h"
#include "lcd_bsp.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"

#define APP_ADDRESS 0x08010000
typedef void (*pFunction)(void);

// 跳转到主程序
void Jump_To_Main(void)
{
    if (((*(__IO uint32_t*)APP_ADDRESS) & 0x2FFE0000) == 0x20000000)
    {
        __disable_irq(); 
        pFunction JumpToApplication = (pFunction) *(__IO uint32_t*) (APP_ADDRESS + 4);
        __set_MSP(*(__IO uint32_t*) APP_ADDRESS);
        JumpToApplication();
    }
	else
	{
		while(1){}
	}
}

TaskHandle_t    Task_handler;
void Task( void * pvParameters );

int main(void)
{
	Nvic_Init();    //NVIC
	Systick_init(); //systick
	tlsf_init();    //mem
    Pin_Ctrl_Init();  //pin
    Wakeup_Key_Init();//wakeup

	if(Read_Wakeup_Key())
	{
		msc_bootuf2_init(0, 0x40040000UL);
		xTaskCreate(Task,"Task",512,NULL,3,&Task_handler);
		vTaskStartScheduler();
    }
	else
	{
		Jump_To_Main();
	}
}

void Task( void * pvParameters )
{
	LCD_Init();
	for(uint8_t i=0;i<240;i+=20)
	{
		Display_None(i);
	}
	Display_Icon(100);
	while(1)
	{
		uint32_t total = bootuf2_get_total_blocks();
		uint32_t written = bootuf2_get_written_blocks();
		if (total == 0 || total == 0xFFFFFFFF) 
			Progress_Bar(130, 0);
		else
			Progress_Bar(130, written*170/total);
		
		if (bootuf2_is_write_done()) 
			NVIC_SystemReset();
	}
}
