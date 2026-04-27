//use variables.h to define gloal variables
#include "stm32f4xx.h" 
#include "lunar.h"
#include "defines.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "semphr.h"
#include "event_groups.h"

//Semaphore
SemaphoreHandle_t xI2SSemaphore = NULL;//music dma 传输完成信号量
SemaphoreHandle_t xTaskManagerSemaphore = NULL;//taskmanager信号量
SemaphoreHandle_t xFDBSemaphore = NULL;//FDB信号量

//event group
EventGroupHandle_t xLcdEventGroup = NULL;

//pin_ctrl.c
volatile uint8_t g_vbus_status = 0;      //usb不向外供电时有效 0-低电平 1-高电平
volatile uint8_t g_charge_status = 0;    //0-不在充电 1-正在充电 2-充电完成
volatile uint8_t g_headphone_status = 0; //1--耳机插入 0-无插入
volatile uint8_t g_TFcard_status = 0;    //0-无插入 1-TF卡正常

volatile uint8_t g_maintain_status = 0;   //1--不断电 0-无
volatile uint8_t g_max98357_ststus = 0;   //1--供电 0-无
volatile uint8_t g_es9018_status = 0;     //1--工作 0-无
volatile uint8_t g_hdp0_or_spk1 = 0;      //0--耳机 1-喇叭

//key.c
volatile uint8_t g_key_WKP_RT = 0;
volatile uint8_t g_key_L_M_RT = 0;
volatile uint8_t g_key_R_M_RT = 0;

//adc.c
volatile uint8_t g_adc_dma_finished = 0;

volatile uint16_t g_host_cc1_value = 4095;
volatile uint16_t g_host_cc2_value = 4095;
volatile uint16_t g_slave_cc1_value = 0;
volatile uint16_t g_slave_cc2_value = 0;

volatile float g_battery_voltage  = 0.0f;

volatile int8_t g_key_L_X = 0;
volatile int8_t g_key_L_Y = 0;
volatile int8_t g_key_R_X = 0;
volatile int8_t g_key_R_Y = 0;

//usb_cc.c
volatile uint8_t g_usb_status = 0;

//v2p_bat.c
volatile int8_t g_battery_percent = 0;

//systick_conf.c
volatile uint32_t RTOS_OK = 0;

//rtc_clock.h
volatile uint8_t RTC_HFmt = 0;  //0-24 1-12
volatile uint8_t RTC_Week = 7;  //1-7
volatile uint8_t RTC_Year = 26; //0-99
volatile uint8_t RTC_Moth = 4;  //1-12
volatile uint8_t RTC_Date = 26; //1-31
volatile uint8_t RTC_Hour = 14; //0-24
volatile uint8_t RTC_Mint = 00; //0-60
volatile uint8_t RTC_Secd = 0;  //0-60

RTC_DateTypeDef now_date;//RTC_WeekDay  RTC_Month  RTC_Date  RTC_Year
RTC_TimeTypeDef now_time;//RTC_Hours  RTC_Minutes  RTC_Seconds  RTC_H12
Lunar_t now_lunar;

//music.c
volatile uint8_t Music_Status = 0;
volatile uint8_t Music_Suspend_Flag = 0;
volatile uint8_t Music_Switch_Method = 0;
//usb
volatile uint8_t g_usb_function = 0;

//task manager.c
volatile uint8_t Basic_Task_Status = Task_P_Null;
volatile uint8_t LVGL_Task_Status = Task_P_Null;
volatile uint8_t USB_Task_Status = Task_P_Null;
volatile uint8_t Music_Task_Status = Task_P_Null;
volatile uint8_t Game_Task_Status = Task_P_Null;
volatile uint8_t Video_Task_Status = Task_P_Null;

//debug
volatile uint8_t debug_mode = Debug_Mode_None;

//lcd
volatile uint8_t g_lcd_user = LCD_USER_LVGL;

//音量
volatile uint8_t g_screen_pwm = 0;

volatile uint8_t g_hdp_value = 32;
volatile uint8_t g_spk_value = 32;
volatile uint8_t g_brightness = 32;

volatile uint8_t music_bitdepth = 16;

//file_unit
char current_path[256] = "";
char chosen_file_path[256] = "";
volatile uint8_t g_file_chosen = 0;

