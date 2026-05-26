//use variables.h to define gloal variables
#include "stm32f4xx.h"
#include "lunar.h"
#include "es9018k2m.h"
#include "defines.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "flashdb.h"

//global error bumber
volatile uint8_t g_error_num = NO_ERROR;

//freertos 互斥锁/信号量 统一定义,各自初始化
SemaphoreHandle_t xI2SSemaphore = NULL;//music dma 传输完成信号量
SemaphoreHandle_t xTaskManagerSemaphore = NULL;//taskmanager信号量
SemaphoreHandle_t xFDBSemaphore = NULL;//FDB信号量
SemaphoreHandle_t xSDcardMutex = NULL;//sdcard互斥锁
SemaphoreHandle_t xSDcardSemaphore = NULL;//sdcard计数型信号量
SemaphoreHandle_t xFlashMutex = NULL;//w25q128互斥锁
SemaphoreHandle_t xFlashSemaphore = NULL;//w25q128计数型信号量
SemaphoreHandle_t xBSCMutex = NULL;//tlsf互斥锁
SemaphoreHandle_t xCCMMutex = NULL;//tlsf互斥锁
SemaphoreHandle_t xIICMutex = NULL;//iic互斥锁
SemaphoreHandle_t usb_tx_cplt_sem = NULL; // 发送完成信号量
SemaphoreHandle_t usb_tx_mutex = NULL;    // 线程安全互斥锁
EventGroupHandle_t xLcdEventGroup = NULL; // lcd

//freertos所有任务句柄
TaskHandle_t Basic_Task_handler   = NULL;
TaskHandle_t Lvgl_Task_handler    = NULL;
TaskHandle_t USB_Task_handler     = NULL;
TaskHandle_t Music_Task_handler   = NULL;
TaskHandle_t Video_Task_handler   = NULL;
TaskHandle_t Game_Task_handler    = NULL;
TaskHandle_t Font_Task_handler    = NULL;
TaskHandle_t Start_Task_handler   = NULL;
TaskHandle_t Task_Manager_handler = NULL;

//adc.c
volatile uint8_t  g_adc_dma_finished = 0;

volatile uint16_t g_host_cc1_value = 4095;
volatile uint16_t g_host_cc2_value = 4095;
volatile uint16_t g_slave_cc1_value = 0;
volatile uint16_t g_slave_cc2_value = 0;

volatile float g_battery_voltage  = 0.0f;

volatile int8_t g_key_L_X = 0;
volatile int8_t g_key_L_Y = 0;
volatile int8_t g_key_R_X = 0;
volatile int8_t g_key_R_Y = 0;

volatile uint8_t g_usb_status = 0;

//key.c
volatile uint8_t g_key_WKP_RT = 0;
volatile uint8_t g_key_L_M_RT = 0;
volatile uint8_t g_key_R_M_RT = 0;

//pin_ctrl.c
volatile uint8_t g_vbus_status = 0;      //usb不向外供电时有效 0-低电平 1-高电平
volatile uint8_t g_charge_status = 0;    //0-不在充电 1-正在充电 2-充电完成
volatile uint8_t g_headphone_status = 0; //1--耳机插入 0-无插入
volatile uint8_t g_TFcard_status = 0;    //0-无插入 1-TF卡正常

volatile uint8_t g_maintain_status = 0;   //1--不断电 0-无
volatile uint8_t g_max98357_inited = 0;   //1--供电 0-无
volatile uint8_t g_es9018_inited = 0;     //1--工作 0-无
volatile uint8_t kv_hdp0_or_spk1 = 0;      //0--耳机 1-喇叭

volatile uint8_t kv_es9018_status = 0;
volatile uint8_t kv_max98357_ststus = 0;

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
volatile uint8_t kv_music_switch_method = 0;

//usb
volatile uint8_t g_usb_function = 0;
volatile uint8_t g_lvgl_input_disabled = 0;

//task manager.c
volatile uint8_t Basic_Task_Status = Task_P_Null;
volatile uint8_t LVGL_Task_Status = Task_P_Null;
volatile uint8_t USB_Task_Status = Task_P_Null;
volatile uint8_t Music_Task_Status = Task_P_Null;
volatile uint8_t Game_Task_Status = Task_P_Null;
volatile uint8_t Video_Task_Status = Task_P_Null;
volatile uint8_t Font_Task_Status = Task_P_Null;

//debug
volatile uint8_t kv_debug_mode = Debug_Mode_None;

//lcd
volatile uint8_t g_lcd_user = LCD_USER_LVGL;


volatile uint8_t kv_screen_status = 0;//pwm

volatile uint8_t g_pwm_inited = 0;//pwm

volatile uint8_t kv_hdp_value = 32;
volatile uint8_t kv_spk_value = 32;
volatile uint8_t kv_brightness = 32;
volatile ES9018_Config_t kv_es9018_cfg = {0,0,104,2,0,0,0,0,0,5,0,1,5,1,0,0,0,0};

volatile uint8_t music_bitdepth = 16;

//file_unit
char *current_path = NULL;
char *chosen_file_path = NULL;
volatile uint8_t g_file_chosen = 0;

//flashdb
struct fdb_kvdb kvdb = { 0 };
struct fdb_tsdb tsdb = { 0 };

volatile uint8_t g_TFcard_inited = 0;

//font update progress
volatile uint8_t g_font_need_update = 0;

volatile uint8_t g_font_update_state = 0;
volatile uint8_t g_font_update_progress = 0;
volatile uint8_t g_font_update_file_index = 0;
volatile uint8_t g_font_update_error = 0;

// variables.c 底部
volatile uint8_t g_host_kbd_key = 0;      // 接收到的 USB 键码
volatile uint8_t g_host_kbd_mod = 0;      // 接收到的修饰键 (Shift等)
volatile uint8_t g_host_kbd_trigger = 0;  // 键按下触发标志位

volatile uint8_t g_usb_kbd_modifier = 0; // Shift, Ctrl, Alt 等修饰键
volatile uint8_t g_usb_kbd_key = 0;      // 实际的键码 (Keycode)
volatile uint8_t g_usb_kbd_trigger = 0;  // 状态机：0=空闲, 1=请求按下, 2=请求松开

// variables.c 底部
volatile int16_t g_usb_joy_L_X = 0;
volatile int16_t g_usb_joy_L_Y = 0;
volatile int16_t g_usb_joy_R_X = 0;
volatile int16_t g_usb_joy_R_Y = 0;

volatile int16_t g_usb_mouse_dx = 0;
volatile int16_t g_usb_mouse_dy = 0;
volatile uint8_t g_usb_mouse_btn = 0;
