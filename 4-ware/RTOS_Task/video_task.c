#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "systick_conf.h"
#include "debug.h"
#include "variables.h"
#include "defines.h"
#include "video.h"
#include "mjpeg.h"
#include "jpg.h"
#include "gif.h"
#include "png.h"
#include "bmp.h"
#include "task_manager.h"
#include "page_manager.h"
#include "fatfs.h"
#include "lv_port_disp.h"
#include <string.h>

#define FILE_OTHER 0
#define FILE_MJPEG 1
#define FILE_RAW   2
#define FILE_BMP   3
#define FILE_JPG   4
#define FILE_PNG   5
#define FILE_GIF   6

static uint8_t file_format = 0;

void Video_Task( void * pvParameters )
{
    uint8_t res = 0;
    
    strcpy(current_path, "0:/VIDEO");
    
    Page_Request_Switch(PAGE_FILE);
    
    while(Page_Get_Current() != PAGE_FILE){}
    
    // 1. 文件选择
    while(Page_Get_Current() == PAGE_FILE || Page_Get_Current() == PAGE_VIDEO)
    {
        Delay_ms(20);
        
        if(g_file_chosen)
        {
            uint8_t *ext = fatfs_get_extension(chosen_file_path);

				 if (strcmp((char*)ext, "mjpeg") == 0) file_format = FILE_MJPEG;
            else if (strcmp((char*)ext, "raw") == 0)   file_format = FILE_RAW;
            else if (strcmp((char*)ext, "bmp") == 0)   file_format = FILE_BMP;
            else if (strcmp((char*)ext, "jpeg") == 0)  file_format = FILE_JPG;
			else if (strcmp((char*)ext, "jpg") == 0)   file_format = FILE_JPG;
            else if (strcmp((char*)ext, "png") == 0)   file_format = FILE_PNG;
            else if (strcmp((char*)ext, "gif") == 0)   file_format = FILE_GIF;
            else file_format = FILE_OTHER;
            
            if(file_format != FILE_OTHER) g_file_chosen = 0;
            res = 0;
        }
        
        // 2. 多媒体播放 / 图片显示
        if(file_format != FILE_OTHER)
        {
			g_lcd_user = LCD_USER_VDEO;
			Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
			lv_port_disp_deinit();
			Page_Request_Switch(PAGE_VIDEO);
			
            if(file_format == FILE_MJPEG)
            {
                res = video_mjpeg_play_init(chosen_file_path);
                while(!g_key_L_M_RT && !res)
                {
                    res = video_mjpeg_play_task();
                }
                video_mjpeg_play_deinit();
            }
            else if(file_format == FILE_RAW)
            {
                res = video_play_init(chosen_file_path);
                while(!g_key_L_M_RT && !res)
                {
                    res = video_play_task();
                }
                video_play_deinit();
            }
            else if(file_format == FILE_BMP || file_format == FILE_JPG || file_format == FILE_PNG)
            {
                if (file_format == FILE_BMP) res = Decode_BMP_Picture(chosen_file_path);
                else if (file_format == FILE_JPG) res = Decode_JPG_Picture(chosen_file_path);
                else if (file_format == FILE_PNG) res = Decode_PNG_Picture(chosen_file_path);
				
                while(!g_key_L_M_RT && !res)
                {
                    Delay_ms(20);
                }
            }
            else if(file_format == FILE_GIF)
            {
                res = Decode_GIF_Init(chosen_file_path);
                while(!g_key_L_M_RT && !res)
                {
                    res = Decode_GIF_Task();
                }
                Decode_GIF_Deinit();
            }
			
            g_lcd_user = LCD_USER_LVGL;
            lv_port_disp_reinit();
            if (lv_scr_act() != NULL) lv_obj_invalidate(lv_scr_act());
            Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
			Page_Back();
			file_format = FILE_OTHER;
        }
    }

    // 4. 删除视频任务
    Taskmanager_Ctrl(Task_N_Video, Task_T_Delete, 0);
    
    while(1) {
        Delay_ms(20);
    }
}
