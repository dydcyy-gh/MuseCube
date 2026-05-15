#include "stm32f4xx.h"                  // Device header

#ifndef __LUNAR_H__
#define __LUNAR_H__

//时间范围 2000.2.5 - 2099
//节气范围 2001 - 2099

typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  date;
} Solar_t;  //公历

typedef struct
{
    uint8_t has_leap_month;     // 1 此年有闰月, 0 无
    uint8_t leapWhichMonth;     // 此年闰月是哪个月份
    uint8_t leapMonthis_30days; // 1 此年闰大月, 0 闰小月
    uint8_t month;              // 1-12当前农历月份
    uint8_t is_leap_month;      // 1 当前农历月是闰月 0非闰月
    uint8_t date;               // 1-30当前农历日
    uint8_t animal;             // 此农历年生肖 1 - 12 : 鼠 - 猪
    uint8_t tian_gan;           // 1-10甲、乙、丙、丁、戊、己、庚、辛、壬、癸
    uint8_t di_zhi;             // 1-12子、丑、寅、卯、辰、巳、午、未、申、酉、戌、亥
	uint8_t jie_qi;             // 1-24节气,否则为0
	uint8_t jie_ri;             // 节日,1-30顺序见下,否则为0
} Lunar_t;  //农历

//1. 元旦   1.1 
//2. 情人节 2.14 
//3. 妇女节 3.8 
//4. 植树节 3.12 
//5. 愚人节 4.1 
//6. 劳动节 5.1 
//7. 青年节 5.4 
//8. 母亲节 5月第二个星期日
//9. 儿童节 6.1 
//10.父亲节 6月第三个星期日
//11.建党节 7.1 
//12.建军节 8.1 
//13.教师节 9.10 
//14.国庆节 10.1 
//15.万圣夜 10.31 
//16.感恩节 11月第四个星期四
//17.平安夜 12.24 
//18.圣诞节 12.25 

//19.春节     1.1
//20.元宵节   1.15
//21.龙抬头   2.2
//22.端午节   5.5
//23.七夕节   7.7
//24.中元节   7.15
//25.中秋节   8.15
//26.重阳节   9.9
//27.腊八节   12.8
//28.北方小年 12.23
//29.南方小年 12.24
//30.除夕     12月最后一天

//计算某个公历年的30个节日的公历日期
//计算结果按照提供的索引顺序存在 jie_ri_months/date
uint8_t The_30_Festival(uint16_t SolarYear,uint8_t *jie_ri_months,uint8_t *jie_ri_dates);

//计算某个公历年的24节气对应公历日期 只支持2001-2099年
//计算结果存在jie_qi_months/date,按立春->大寒排列
uint8_t The_24_solar_terms(uint16_t SolarYear,uint8_t *jie_qi_months,uint8_t *jie_qi_dates);

uint8_t DaysInMonth(uint16_t year, uint8_t month);
uint8_t GetWeekDay(uint16_t y, uint8_t m, uint8_t d);

//以2000正月初一为起点计算农历月日
uint8_t Solar2Lunar(
	Solar_t *solar, Lunar_t *lunar, 
	uint8_t *jie_ri_months, uint8_t *jie_ri_dates, 
	uint8_t *jie_qi_months, uint8_t *jie_qi_dates
);

#endif
