#include "stm32f4xx.h"           
#include "systick_conf.h"
#include "lunar.h"
#include "variables.h"

volatile static uint8_t err_flag = 0;        //1-error 0-success
volatile static uint16_t Checksum = 0xAAAA;

uint8_t RTC_jieqi_moth[24];
uint8_t RTC_jieqi_date[24];

static uint8_t RTC_jieri_moth[30];
static uint8_t RTC_jieri_date[30];

// 辅助函数：24小时制转换为12小时制
static void Convert24To12(uint8_t *hour24, uint8_t *ampm)
{
    if (*hour24 == 0) {*hour24 = 12;*ampm = 0;} 
	else if (*hour24 < 12) {*hour24 = *hour24;*ampm = 0;} 
	else if (*hour24 == 12) {*hour24 = 12;*ampm = 1;} 
	else {*hour24 = *hour24 - 12;*ampm = 1;}
}

static void RTC_Set_Time(void)
{
	RTC_TimeTypeDef RTC_TimeTypeInitStructure;
	
	RTC_TimeTypeInitStructure.RTC_Hours   = RTC_Hour;
	RTC_TimeTypeInitStructure.RTC_Minutes = RTC_Mint;
	RTC_TimeTypeInitStructure.RTC_Seconds = RTC_Secd;
	
	err_flag = RTC_SetTime(RTC_Format_BIN,&RTC_TimeTypeInitStructure) ? 0 : 1;
}

static void RTC_Set_Date(void)
{
	RTC_DateTypeDef RTC_DateTypeInitStructure;
	
	RTC_DateTypeInitStructure.RTC_Date    = RTC_Date;
	RTC_DateTypeInitStructure.RTC_Month   = RTC_Moth;
	RTC_DateTypeInitStructure.RTC_WeekDay = RTC_Week;
	RTC_DateTypeInitStructure.RTC_Year    = RTC_Year;
	
	err_flag = RTC_SetDate(RTC_Format_BIN,&RTC_DateTypeInitStructure) ? 0 : 1;
}

uint8_t RTC_Set_Clock(void)
{
	RTC_EnterInitMode();
	
	RTC_InitTypeDef RTC_InitStructure;
	RTC_InitStructure.RTC_AsynchPrediv = 127;//32768/128/256
	RTC_InitStructure.RTC_SynchPrediv  = 255;//32768/128/256
	RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
	err_flag = RTC_Init(&RTC_InitStructure);
	
	RTC_Set_Time();
	RTC_Set_Date();
	
	RTC_ExitInitMode();
    RTC_WaitForSynchro();
	
	return err_flag;
}

uint8_t RTC_Clock_Update(void)
{
	uint8_t res = 0;
	static uint8_t today  = 0;
	static uint8_t toyear = 0;
	
	Solar_t now_solar;

	RTC_GetDate(RTC_Format_BIN, &now_date);
    RTC_GetTime(RTC_Format_BIN, &now_time);
	
	if(RTC_HFmt) Convert24To12(&now_time.RTC_Hours,&now_time.RTC_H12);
	
	now_solar.date = now_date.RTC_Date;
	now_solar.month = now_date.RTC_Month;
	now_solar.year = now_date.RTC_Year+2000;
		
	if(toyear != now_date.RTC_Year)
	{
		toyear = now_date.RTC_Year;
		res = The_24_solar_terms(now_date.RTC_Year+2000,RTC_jieqi_moth,RTC_jieqi_date);
		res = The_30_Festival(now_date.RTC_Year+2000,RTC_jieri_moth,RTC_jieri_date);
	}
	
	if(today != now_date.RTC_Date)
	{
		today = now_date.RTC_Date;
		res = Solar2Lunar(&now_solar,&now_lunar,RTC_jieri_moth,RTC_jieri_date,RTC_jieqi_moth,RTC_jieqi_date);
	}
	return res;
}

uint8_t RTC_Clock_Init(void)
{
	uint8_t retry = 0; 
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	
	PWR_BackupAccessCmd(ENABLE);
	
	if(RTC_ReadBackupRegister(RTC_BKP_DR0) != Checksum)
	{
		RCC_LSEConfig(RCC_LSE_ON);
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
		{
			if(retry++ > 20) 
			{
				err_flag = 1;
				break;
			}
			Delay_ms(5);
		}
		if(!err_flag)
		{
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);  
			RCC_RTCCLKCmd(ENABLE);
			
			RTC_WaitForSynchro();
			
			err_flag = RTC_Set_Clock();
			
			RTC_WriteBackupRegister(RTC_BKP_DR0,Checksum);
		}
	}
	else
	{
		RTC_WaitForSynchro();
	}
	
	err_flag = RTC_Clock_Update();
	
	return err_flag;
}
