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
#include "file_unit.h"
#include <string.h>
#include "avi.h"

void Video_Task( void * pvParameters )
{
    uint8_t res = 0;
    uint8_t last_L = 0;

    Delay_ms(50);

    uint8_t *ext = fatfs_get_extension(chosen_file_path);

    g_lcd_user = LCD_USER_VDEO;
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
    Page_Push_History(Page_Get_Current());
    lv_port_disp_deinit();
    Page_Request_Switch(PAGE_VIDEO);

    if (strcmp((char*)ext, "mjpeg") == 0)
    {
        res = video_mjpeg_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (!g_key_L_M_RT && last_L) break;
            last_L = g_key_L_M_RT;
            res = video_mjpeg_play_task();
        }
        video_mjpeg_play_deinit();
    }
    else if (strcmp((char*)ext, "raw") == 0)
    {
        res = video_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (!g_key_L_M_RT && last_L) break;
            last_L = g_key_L_M_RT;
            res = video_play_task();
        }
        video_play_deinit();
    }
	else if (strcmp((char*)ext, "avi") == 0)
    {
        res = video_avi_play_init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (!g_key_L_M_RT && last_L) break;
            last_L = g_key_L_M_RT;
            res = video_avi_play_task();
        }
        video_avi_play_deinit();
    }
    else if (strcmp((char*)ext, "bmp") == 0 ||
             strcmp((char*)ext, "jpeg") == 0 ||
             strcmp((char*)ext, "jpg") == 0 ||
             strcmp((char*)ext, "png") == 0)
    {
        if (strcmp((char*)ext, "bmp") == 0)       
			res = Decode_BMP_Picture(chosen_file_path);
        else if (strcmp((char*)ext, "png") == 0)   
			res = Decode_PNG_Picture(chosen_file_path);
        else                                        
			res = Decode_JPG_Picture(chosen_file_path);
        chosen_file_path_free();

        while (!res)
        {
            if (!g_key_L_M_RT && last_L) break;
            last_L = g_key_L_M_RT;
            Delay_ms(20);
        }
    }
    else // gif
    {
        res = Decode_GIF_Init(chosen_file_path);
        chosen_file_path_free();
        while (!res)
        {
            if (!g_key_L_M_RT && last_L) break;
            last_L = g_key_L_M_RT;
            res = Decode_GIF_Task();
        }
        Decode_GIF_Deinit();
    }

    g_lcd_user = LCD_USER_LVGL;
    lv_port_disp_reinit();
    if (lv_scr_act() != NULL) lv_obj_invalidate(lv_scr_act());
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
    Page_Back();

    Taskmanager_Ctrl(Task_N_Video, Task_T_Delete, 0);
    while(1) { Delay_ms(20); }
}
