//use difine.h to avoid magic number

#ifndef __DEFINES_H__
#define __DEFINES_H__

//BASIC_task
#define BASIC_PRIO         3
#define BASIC_STACK_SIZE   512

//LCD lvgl task
#define LVGL_PRIO         2
#define LVGL_STACK_SIZE   512

//Video task
#define VIDEO_PRIO         2
#define VIDEO_STACK_SIZE   512

//Game task
#define GAME_PRIO         2
#define GAME_STACK_SIZE   512

//USB task
#define USB_PRIO          4  
#define USB_STACK_SIZE    512

//usb ep0 thread 5
//usb host psc thread 5

//Music task
#define MUSIC_PRIO         6
#define MUSIC_STACK_SIZE   512

//TASK_MANAGER
#define TASK_MANAGER_PRIO         7
#define TASK_MANAGER_STACK_SIZE   512

//START_TASK
#define START_TASK_PRIO         8
#define START_TASK_STACK_SIZE   512

//...define for Rtos Task Settings...//
#define Task_N_Basic     0
#define Task_N_LVGL      1 
#define Task_N_USB       2
#define Task_N_Music     3
#define Task_N_Video     4
#define Task_N_Game      5


//...define for task manager : task status...// (T-temporary  P-presistent)
#define Task_P_Null      0 //0-Null (Inital Value and Auto set after 5)
#define Task_T_Creat     1 //1-Creat the task 
#define Task_P_Running   2 //2-The task is running(Auto set after 1/5)
#define Task_T_Suspend   3 //3-Suspend the task
#define Task_P_Stop 	 4 //4-The task has suspended(Auto set after 3)
#define Task_T_Resume 	 5 //5-Resume the task when Task_T_Suspend
#define Task_T_Delete    6 //6-Delete the task



//...define for usb connect status ...//
#define TYPEC_NO_FIND 0 //host:4095+4095  		device:2+2   		//no connect
#define TYPEC_CC_IDLE 1 //host:4000+120 		device:2+2   		//just connect a CtoC data cable
#define TYPEC_AC_IDLE 2 //host:4076+2490 		device:2+2   		//just connect a AtoC data cable
#define TYPEC_AC_OKEY 3 //host:4076+4095  		device:2+500        //AtoC cbable ,connect as device
#define TYPEC_CC_OKEY 4 //host:120+500/3972 	device:2+1150/2050  //CtoC cbable ,connect as device
#define TYPEC_IS_HOST 5 //host:4076+500     	device:2+2          //connect as host directly
#define TYPEC_CC_HOST 6 //host:500+120      	device:2+2          //as host through a CtoC cbable 



//...define for variable Music_Status ...//
#define Music_None     0
#define Music_Init     1  //外部置位,用于打开MUSIC播放
#define Song_Prepare   2 
#define Song_Playing   3 
#define Song_End       4  
#define Song_Next      5  //外部置位,无视Music_Switch_Method切歌
#define Song_Previous  6  //外部置位,无视Music_Switch_Method切歌
#define Song_File      7  //外部置位,通过选择文件列表切歌
#define Song_Error     8  //外部置位,用于退出MUSIC播放
#define Music_Exit     9  //外部置位,用于退出MUSIC播放



//...define for variable Music_Switch_Method ...//
#define Play_In_Order  0
#define Play_Randomly  1
#define Play_Repeatly  2 



//...define for variable fatfs ...//
#define DEV_SD      0
#define DEV_USB     1



//...define for usb functions ...//
#define USB_NONE        0
#define USBD_CDC        1
#define USBD_MSC        2
#define USBD_UAC1       3
#define USBD_UAC2       4
#define USBD_DISP       5
#define USBD_GMPD       6
#define USBD_KBD        7
#define USBD_MOU        8
#define USBH_CDC        9
#define USBH_MSC        10
#define USBH_GMPD       11
#define USBH_HID        12


#define USB_OTG_HS_PERIPH_BASE  0x40040000UL
#define USB_OTG_FS_PERIPH_BASE  0x50000000UL


#define Debug_Mode_None 0  // 关闭日志
#define Debug_Mode_TSDB 1  // 暂存模式 (保存到 FlashDB)
#define Debug_Mode_USBD 2  // 实时 USB 输出
#define Debug_Mode_LVGL 3  // 实时 屏幕 输出

//g_lcd_user
#define LCD_USER_LVGL (1 << 0)
#define LCD_USER_DISP (1 << 1)
#define LCD_USER_VDEO (1 << 2)
#define LCD_USER_GAME (1 << 3)

//global error bumber
#define NO_ERROR 0

#endif
