#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "music.h"
#include "systick_conf.h"
#include "variables.h"
#include "defines.h"
#include "es9018k2m.h"
#include "pin_ctrl.h"
#include "task_manager.h"
#include "page_manager.h"

void Music_Task( void * pvParameters )
{
	while(1)
	{
		if(Music_Status == Music_None)
		{
			if(Page_Get_Current() == PAGE_MUSIC) 
			{
				Music_Status = Music_Init;
			}
			else
			{
				Delay_ms(20);
			}
		}
		else if(Music_Status == Music_Init)
		{
			if(audio_play_prepare()) Music_Status = Music_Exit;
			else Music_Status = Song_Prepare;
		}
		else if(Music_Status == Music_Exit)
		{
			audio_play_clear();
			Music_Status = Music_None;
		}
		else
		{
			static uint32_t last_page = PAGE_NONE;
			static uint32_t current_page = PAGE_NONE;
            
			uint32_t temp_page = Page_Get_Current();
			
			if(temp_page != current_page) 
			{
				last_page = current_page;
				current_page = temp_page;
			}
			
			//从桌面进入到不是音乐的页面
			if(current_page != PAGE_MUSIC && last_page == PAGE_DESKTOP) 
			{
				Music_Status = Music_Exit;
			}
            
			//从音乐进入文件页面
			if(current_page == PAGE_FILE && last_page == PAGE_MUSIC) 
			{
				if(g_file_chosen)
				{
					Music_Status = Song_File;
					Page_Back();
					g_file_chosen = 0;
				}
			}
			
			audio_play_task();
		}
	}
}
