#include "stm32f4xx.h" 
#include "lunar.h"

//时间范围 2000.2.5 - 2099
//节气范围 2001 - 2099

const uint32_t LUNAR_INFO[] = {
	0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5, //2000
	0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930, //2010
	0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, //2020
	0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45, //2030
	0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0, //2040
	0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06aa0, 0x1a6c4, 0x0aae0, //2050
	0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4, //2060
	0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0, //2070
	0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160, //2080
	0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252  //2090
};

// Zeller公式：返回 1=周一 ... 6=周六 0=周日
uint8_t GetWeekDay(uint16_t y, uint8_t m, uint8_t d) 
{
    if(m<3){m+=12; y--;}
    uint16_t c = y / 100;
    uint16_t year = y % 100;
    int32_t w = (d + 13*(m+1)/5 + year + year/4 + c/4 - 2*c - 1);
	w = (w % 7 + 7) % 7;
    return (uint8_t)w;
}

//计算闰年
uint8_t IsLeapYear(uint16_t year)
{
	if((year%4==0 && year%100!=0) || year%400==0) return 1;
	else return 0;
}

//返回某年某月的天数
uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
    uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) return IsLeapYear(year) ? 29 : 28;
    return mdays[month - 1];
}

//计算以2000正月初一为起点计算农历月日
uint32_t Solar_Duration(Solar_t *Solar)
{
    int16_t i = 0;
	
	//2000/2/5为正月初一
    Solar_t Solar_2000;
    Solar_2000.year = 2000;
    Solar_2000.month = 2;
    Solar_2000.date = 5;
	
    uint8_t MonthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t TotalDays1 = 0;    //Solar2000
    uint32_t TotalDays2 = 0;    //Solar
    uint32_t TotalDays = 0;     //记录间隔总天数

    //计算Solar1已年已过天数
    MonthDays[1] = (IsLeapYear(Solar_2000.year) ? 29 : 28);
    for (i = 1; i < Solar_2000.month; i++)
    {
        TotalDays1 += MonthDays[i - 1];
    }
    TotalDays1 += Solar_2000.date;

    //计算Solar2当年已过天数
    MonthDays[1] = (IsLeapYear(Solar->year) ? 29 : 28);
    for (i = 1; i < Solar->month; i++)
    {
        TotalDays2 += MonthDays[i - 1];
    }
    TotalDays2 += Solar->date;

    //同一年
    if (Solar_2000.year == Solar->year)
    {
        TotalDays = TotalDays2 - TotalDays1;
    }
    else
    {
        for (i = Solar_2000.year + 1; i < Solar->year; i++)
        {
            TotalDays += (IsLeapYear(i) ? 366 : 365);
        }
        TotalDays += ((IsLeapYear(Solar_2000.year) ? 366 : 365) - TotalDays1 + TotalDays2);
    }
    return TotalDays;
}

//计算某农历年是否有闰月
uint8_t Is_Lunar_Has_LeapMonth(uint16_t SolarYear, Lunar_t *Lunar)
{
    if (SolarYear < 2000 || SolarYear > 2099) return 1;

    if ((LUNAR_INFO[SolarYear - 2000] & 0x0000000f) == 0 || (LUNAR_INFO[SolarYear - 2000] & 0x0000000f) > 12)
    {
        Lunar->has_leap_month = 0;
        return 0;
    }
    /* 存在闰月 */
    Lunar->has_leap_month = 1;
    Lunar->leapWhichMonth = LUNAR_INFO[SolarYear - 2000] & 0x0000000f;
    Lunar->leapMonthis_30days = (LUNAR_INFO[SolarYear - 2000] >> 16) & 0x00000001;

    return 0;
}

//计算此年农历月（非闰月）的天数
uint8_t Lunar_Month_Days(uint16_t LunarYear, uint8_t LunarMonth)
{
    if (LunarYear < 2000 || LunarYear > 2099) return 1;
    if (LunarMonth < 1 || LunarMonth > 12) return 1;
    if ( ( LUNAR_INFO[LunarYear - 2000] >> (16 - LunarMonth) ) & 0x00000001 ) return 30;
    else return 29;
}

//计算某个公历年的24节气对应公历日期 只支持2001-2099年
//计算结果存在jie_qi_months/date,按立春->大寒排列
uint8_t The_24_solar_terms(uint16_t SolarYear,uint8_t *jie_qi_months,uint8_t *jie_qi_dates)
{
    uint8_t i = 0;
    uint8_t Y = SolarYear % 100;   //年份后两位
    float D = 0.2422;
    float C_20xx[] =               //21世纪24个节气的C值
    {
        3.87, 18.73, 5.63, 20.646, 4.81,  20.1,
        5.52, 21.04, 5.678, 21.37, 7.108, 22.83,
        7.5, 23.13, 7.646, 23.042, 8.318, 23.438,
        7.438, 22.36, 7.18, 21.94, 5.4055, 20.12
    };
    
    uint8_t solar_term_months[24] = {
        2, 2, 3, 3, 4,  4,  5,  5,  6,  6,  7, 7,
        8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 1, 1
    };

    if (SolarYear < 2001 || SolarYear > 2099) return 1;

    for (i = 0; i < 24; i++)
    {
        jie_qi_months[i] = solar_term_months[i];
        
        if (i <= 1 || i >= 22)
            jie_qi_dates[i] = ((uint8_t)(Y * D + C_20xx[i])) - ((Y - 1) / 4);
        else
            jie_qi_dates[i] = ((uint8_t)(Y * D + C_20xx[i])) - (Y / 4);

		     if (SolarYear == 2026 && i == 1)  jie_qi_dates[i] -= 1; 
        else if (SolarYear == 2084 && i == 3)  jie_qi_dates[i] += 1;  
        else if (SolarYear == 2008 && i == 7)  jie_qi_dates[i] += 1; 
        else if (SolarYear == 2016 && i == 10) jie_qi_dates[i] += 1;  
        else if (SolarYear == 2002 && i == 12) jie_qi_dates[i] += 1;  
        else if (SolarYear == 2089 && i == 17) jie_qi_dates[i] += 1;
        else if (SolarYear == 2089 && i == 18) jie_qi_dates[i] += 1; 
        else if (SolarYear == 2021 && i == 21) jie_qi_dates[i] -= 1; 
        else if (SolarYear == 2082 && i == 23) jie_qi_dates[i] += 1;
    }
    return 0;
}

// 农历转公历
uint8_t Lunar2Solar(uint16_t year, uint8_t month, uint8_t date, uint8_t isLeap,
                   uint16_t *solar_year, uint8_t *solar_month, uint8_t *solar_date)
{
    if (year < 2000 || year > 2099) return 1;

    Lunar_t lunar;
    uint32_t duration = 0;
    uint16_t curYear = 2000;

    // 计算从2000年到目标年份前一年的总天数
    while (curYear < year)
    {
        for (uint8_t m = 1; m <= 12; m++)
            duration += (uint32_t)Lunar_Month_Days(curYear, m);
        Is_Lunar_Has_LeapMonth(curYear, &lunar);
		if(lunar.has_leap_month == 1)
            duration += (lunar.leapMonthis_30days ? 30 : 29);
        curYear++;
    }

    // 获取当前年的闰月信息
    Is_Lunar_Has_LeapMonth(year, &lunar);
    uint8_t leapMonth = lunar.has_leap_month ? lunar.leapWhichMonth : 0;

    // 计算当前年的月份天数
    for (uint8_t m = 1; m < month; m++)
    {
        duration += (uint32_t)Lunar_Month_Days(year, m);
        // 如果这个月是闰月，且闰月在目标月份之前，加上闰月天数
        if (m == leapMonth)
            duration += (lunar.leapMonthis_30days ? 30 : 29);
    }

    // 处理目标月份是闰月的情况
    if (isLeap && month == leapMonth)
    {
        duration += (uint32_t)Lunar_Month_Days(year, month);
    }
    duration += (uint32_t)(date - 1);  // 从初一算起

    // 基准公历：2000-02-05（对应农历2000正月初一）
    uint16_t y = 2000;
    uint8_t m = 2;
    uint8_t d = 5;

    // 把duration天逐日加到基准公历
    while (duration > 0)
    {
        d++;
        if (d > DaysInMonth(y, m))
        {
            d = 1;
            m++;
            if (m > 12) { m = 1; y++; if (y > 2099) return 1; }
        }
        duration--;
    }

    *solar_year = y;
    *solar_month = m;
    *solar_date = d;
    return 0;
}

//计算某个公历年的30个节日的公历日期
//计算结果按照提供的索引顺序存在 jie_ri_months/date
uint8_t The_30_Festival(uint16_t SolarYear,uint8_t *jie_ri_months,uint8_t *jie_ri_dates)
{
    if(SolarYear < 2000 || SolarYear > 2099) return 1;

    // 初始化全部节日为未设定
    for(uint8_t i=0;i<30;i++)
    {
        jie_ri_months[i] = 0;
        jie_ri_dates[i]  = 0;
    }

	uint8_t w0 = GetWeekDay(SolarYear, 5, 1); 
	uint8_t d0 = (w0 == 0) ? 1 : (8 - w0);
	uint8_t w1 = GetWeekDay(SolarYear, 6, 1);
    uint8_t d1 = (w1 == 0) ? 1 : (8 - w1);
    uint8_t w2 = GetWeekDay(SolarYear, 11, 1);
    uint8_t d2 = (w2 <= 4) ? (4 - w2) : (11 - w2);
	
    jie_ri_months[0]  = 1;  jie_ri_dates[0]  = 1;  		//元旦
    jie_ri_months[1]  = 2;  jie_ri_dates[1]  = 14; 		//情人节
    jie_ri_months[2]  = 3;  jie_ri_dates[2]  = 8;  		//妇女节
    jie_ri_months[3]  = 3;  jie_ri_dates[3]  = 12; 		//植树节
    jie_ri_months[4]  = 4;  jie_ri_dates[4]  = 1;  		//愚人节
    jie_ri_months[5]  = 5;  jie_ri_dates[5]  = 1;  		//劳动节
    jie_ri_months[6]  = 5;  jie_ri_dates[6]  = 4;  		//青年节
    jie_ri_months[7]  = 5;  jie_ri_dates[7]  = d0+7;    //母亲节
    jie_ri_months[8]  = 6;  jie_ri_dates[8]  = 1;       //儿童节
    jie_ri_months[9]  = 6;  jie_ri_dates[9]  = d1+14;   //父亲节
    jie_ri_months[10] = 7;  jie_ri_dates[10] = 1; 		//建党节
    jie_ri_months[11] = 8;  jie_ri_dates[11] = 1; 		//建军节
    jie_ri_months[12] = 9;  jie_ri_dates[12] = 10;		//教师节
    jie_ri_months[13] = 10; jie_ri_dates[13] = 1; 		//国庆节
    jie_ri_months[14] = 10; jie_ri_dates[14] = 31;		//万圣夜
    jie_ri_months[15] = 11; jie_ri_dates[15] = d2+22;	//感恩节
    jie_ri_months[16] = 12; jie_ri_dates[16] = 24;		//平安夜
    jie_ri_months[17] = 12; jie_ri_dates[17] = 25;		//圣诞节
	
    uint8_t sm, sd;

    //农历节日的日期
	//春节 元宵节 龙抬头 端午节 七夕节 中元节 中秋节 重阳节 腊八节 北方小年 南方小年 除夕
    uint8_t L_m[12]  = {1,1, 2,5,7,7, 8, 9,12,12,12,12};
    uint8_t L_d[12]  = {1,15,2,5,7,15,15,9,8, 23,24,0};
	L_d[11] = Lunar_Month_Days(SolarYear-1,12);

    for (uint8_t i = 0; i < 12; i++)
    {
		uint16_t smy;
		if (Lunar2Solar(SolarYear, L_m[i], L_d[i], 0, &smy, &sm, &sd) == 0) 
		{
			if(smy != SolarYear)
			{
				if (Lunar2Solar(SolarYear-1, L_m[i], L_d[i], 0, &smy, &sm, &sd) != 0)
				{
					sm = 0;sd = 0;
				}
			}
		}
		else
		{
			sm = 0;sd = 0;
		}
		jie_ri_months[18+i] = sm;
		jie_ri_dates[18+i]  = sd;
    }
	
    return 0;
}

//以2000正月初一为起点计算农历月日
uint8_t Solar2Lunar(
	Solar_t *solar, Lunar_t *lunar, 
	uint8_t *jie_ri_months, uint8_t *jie_ri_dates, 
	uint8_t *jie_qi_months, uint8_t *jie_qi_dates
)
{
	//计算间隔
	uint32_t Duartion;
	Duartion = Solar_Duration(solar) + 1;
	
    uint16_t SolarYear = 2000;
    uint32_t DI_temp = 0;
    uint8_t err;

    lunar->has_leap_month = 0;
    lunar->leapWhichMonth = 0;
    lunar->leapMonthis_30days = 0;
    lunar->is_leap_month = 0;      //2000农历1月非闰月
    lunar->month = 1;              //正月
    lunar->date = 1;               //初一
    lunar->animal = 5;             //龙
    lunar->tian_gan = 7;           //庚
    lunar->di_zhi = 5;             //辰
    lunar->jie_qi = 0;             //初始化为0，表示无节气

    while (1)
    {
        //查询此年是否存在闰月
        err = Is_Lunar_Has_LeapMonth(SolarYear, lunar);
        if (err == 1) return 1;
        
        //如果当前月为闰月
        if (lunar->is_leap_month == 1)
            DI_temp += lunar->leapMonthis_30days ? 30 : 29;
        else
            DI_temp += Lunar_Month_Days(SolarYear, lunar->month);
		
        //总天数已达
        if (DI_temp >= Duartion)
        {
            if (lunar->is_leap_month == 1)
                lunar->date = (lunar->leapMonthis_30days ? 30 : 29) - (DI_temp - Duartion);
            else
                lunar->date = Lunar_Month_Days(SolarYear, lunar->month) - (DI_temp - Duartion);
            
            // 计算当天是否节气
			for (uint8_t i = 0; i < 24; i++){
				if (solar->month == jie_qi_months[i] && solar->date == jie_qi_dates[i]){
					lunar->jie_qi = i + 1;break;
				}
			}
			
			// 计算当天是否节日
			for (uint8_t i = 0; i < 30; i++){
				if (solar->month == jie_ri_months[i] && solar->date == jie_ri_dates[i]){
					lunar->jie_ri = i + 1;break;
				}
			}
			
            return 0;
        }

        //此年有闰月 并且今年未到过闰月
        if ((lunar->has_leap_month == 1) && (lunar->is_leap_month == 0))
        {
            if (lunar->leapWhichMonth == lunar->month) lunar->is_leap_month = 1;
            else lunar->month += 1;
        }
        //此年有闰月且一月为闰月 或者 此年无闰月
        else if ( ((lunar->has_leap_month == 1) && (lunar->is_leap_month == 1)) || (lunar->has_leap_month == 0) )
        {
            lunar->is_leap_month = 0;  //闰月下一个月为非闰月
            lunar->month += 1;
        }
		
        //溢出处理 生肖 天干地支
        if (lunar->month > 12)
        {
            if (++SolarYear > 2099) return 1;
            lunar->month = 1;
            if (++lunar->animal > 12) lunar->animal = 1;
            if (++lunar->tian_gan > 10) lunar->tian_gan = 1;
        }
        lunar->di_zhi = lunar->animal;
    }
}
