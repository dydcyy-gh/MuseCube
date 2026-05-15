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
#include "file_unit.h"
#include <string.h>

void Game_Task( void * pvParameters )
{
    Delay_ms(50);

    g_lcd_user = LCD_USER_GAME;
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Suspend, 0);
    lv_port_disp_deinit();
    Page_Request_Switch(PAGE_GAME);

    nes_init(chosen_file_path);
    chosen_file_path_free();

    while (!g_key_L_M_RT)
    {
        nes_task();
    }

    nes_deinit();

    g_lcd_user = LCD_USER_LVGL;
    lv_port_disp_reinit();
    if (lv_scr_act() != NULL) lv_obj_invalidate(lv_scr_act());
    Taskmanager_Ctrl(Task_N_LVGL, Task_T_Resume, 0);
    Page_Back();

    Taskmanager_Ctrl(Task_N_Game, Task_T_Delete, 0);
    while(1) { Delay_ms(20); }
}
