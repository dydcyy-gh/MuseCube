#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "systick_conf.h"
#include "variables.h"
#include "defines.h"
#include "nes_main.h"
#include "lvgl.h"
#include "task_manager.h"
#include "page_manager.h"
#include "fatfs.h"
#include "lv_port_disp.h"
#include <string.h>

#define FILE_OTHER 0
#define FILE_NES   1
	
static uint8_t file_format = 0;

void Game_Task( void * pvParameters )
{
    strcpy(current_path, "0:/GAME");
    
    Page_Request_Switch(PAGE_FILE);
    
    while(Page_Get_Current() != PAGE_FILE){}

    while(Page_Get_Current() == PAGE_FILE || Page_Get_Current() == PAGE_GAME)
    {
        Delay_ms(20);
        
        if(g_file_chosen)
        {
            uint8_t *ext = fatfs_get_extension(chosen_file_path);

            if (strcmp((char*)ext, "nes") == 0) file_format = FILE_NES;
            else file_format = FILE_OTHER;
            
            if(file_format != FILE_OTHER) g_file_chosen = 0;
        }
        
        // 2. 游戏运行
        if(file_format != FILE_OTHER)
        {
            g_lcd_user = LCD_USER_GAME;
            Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
            lv_port_disp_deinit();
			Page_Request_Switch(PAGE_GAME);

            if(file_format == FILE_NES)
            {
                // 启动nes，传入选中的文件路径
                nes_init(chosen_file_path);
                
                // 循环运行游戏，直到按下退出键
                while(!g_key_L_M_RT)
                {
                    nes_task();
                }
                
                // 退出游戏，释放资源
                nes_deinit();
            }
            
            // 恢复LVGL
            g_lcd_user = LCD_USER_LVGL;
            lv_port_disp_reinit();
            if (lv_scr_act() != NULL) lv_obj_invalidate(lv_scr_act());
            Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
            Page_Back();
			
            file_format = FILE_OTHER;
        }
    }

    // 3. 删除游戏任务
    Taskmanager_Ctrl(Task_N_Game, Task_T_Delete, 0);
    
    while(1) {
        Delay_ms(20);
    }
}
