//use variables.h to define gloal variables
#include "stm32f4xx.h" 
#include "lunar.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#ifndef __VARIABLES_H__
#define __VARIABLES_H__

//进出临界区保证原子性
#define GLOBAL(...)  do { \
    taskENTER_CRITICAL(); \
    __VA_ARGS__;          \
    taskEXIT_CRITICAL();  \
} while(0)
//GLOBAL(global_counter = 1);

//Semaphore
extern SemaphoreHandle_t xI2SSemaphore;//music dma 传输完成信号量
extern SemaphoreHandle_t xTaskManagerSemaphore;//taskmanager信号量
extern SemaphoreHandle_t xFDBSemaphore;//FDB信号量

//event group
extern EventGroupHandle_t xLcdEventGroup;

//pin_ctrl.c
extern volatile uint8_t g_vbus_status;      //usb不向外供电时有效 0-低电平 1-高电平
extern volatile uint8_t g_charge_status;//0-不在充电 1-正在充电 2-充电完成
extern volatile uint8_t g_headphone_status;//1--耳机插入 0-无插入
extern volatile uint8_t g_TFcard_status;//0-无插入 1-TF卡入 2-TF卡故障
extern volatile uint8_t g_maintain_status;//0-free 1-maintain
extern volatile uint8_t g_max98357_ststus;    //1--供电 0-无
extern volatile uint8_t g_es9018_status;     //1--供电 0-无

//key.c
extern volatile uint8_t g_key_WKP_RT;
extern volatile uint8_t g_key_L_M_RT;
extern volatile uint8_t g_key_R_M_RT;

//adc.c
extern volatile uint8_t g_adc_dma_finished;

extern volatile uint16_t g_host_cc1_value;
extern volatile uint16_t g_host_cc2_value;
extern volatile uint16_t g_slave_cc1_value;
extern volatile uint16_t g_slave_cc2_value;

extern volatile float g_battery_voltage;

extern volatile int8_t g_key_L_X;
extern volatile int8_t g_key_L_Y;
extern volatile int8_t g_key_R_X;
extern volatile int8_t g_key_R_Y;

//usb_cc.c
extern volatile uint8_t g_usb_status;

//v2p_bat.c
extern volatile int8_t g_battery_percent;

//systick_conf.h
extern volatile uint32_t RTOS_OK;

//RTC Global Settings
extern volatile uint8_t RTC_HFmt; //0-24 1-12
extern volatile uint8_t RTC_AMPM; //0-am 0x40-pm (only @RTC_HFmt=12)
extern volatile uint8_t RTC_Week; //1-7
extern volatile uint8_t RTC_Year; //0-99
extern volatile uint8_t RTC_Moth; //1-12
extern volatile uint8_t RTC_Date; //1-31
extern volatile uint8_t RTC_Hour; //0-12/24
extern volatile uint8_t RTC_Mint; //0-60
extern volatile uint8_t RTC_Secd; //0-60

//RTC Global Variable
extern RTC_DateTypeDef now_date;//RTC_WeekDay  RTC_Month  RTC_Date  RTC_Year
extern RTC_TimeTypeDef now_time;//RTC_Hours  RTC_Minutes  RTC_Seconds  RTC_H12
extern Lunar_t now_lunar;

//music
extern volatile uint8_t Music_Status;
extern volatile uint8_t Music_Suspend_Flag;
extern volatile uint8_t Music_Switch_Method;

//usb
extern volatile uint8_t g_usb_function;

//task manager.c
extern volatile uint8_t Basic_Task_Status;
extern volatile uint8_t LVGL_Task_Status;
extern volatile uint8_t USB_Task_Status;
extern volatile uint8_t Music_Task_Status;
extern volatile uint8_t Game_Task_Status;
extern volatile uint8_t Video_Task_Status;

//debug
extern volatile uint8_t debug_mode;

//lcd
extern volatile uint8_t g_lcd_user;

extern volatile uint8_t g_screen_pwm;

extern volatile uint8_t g_hdp0_or_spk1;
extern volatile uint8_t g_hdp_value;
extern volatile uint8_t g_spk_value;
extern volatile uint8_t g_brightness;

extern volatile uint8_t music_bitdepth;

//file_unit
extern char current_path[256];
extern char chosen_file_path[256];
extern volatile uint8_t g_file_chosen;

#endif
