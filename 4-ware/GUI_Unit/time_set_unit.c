#include "time_set_unit.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "rtc_clock.h"
#include "page_manager.h"
#include "variables.h"
#include "malloc.h" 

// 引入外部 STM32 标准库的时间结构体（如果 rtc_clock.h 里已经 extern 过这里可省略）
extern RTC_DateTypeDef now_date;
extern RTC_TimeTypeDef now_time;

static lv_obj_t * time_set_cont = NULL;

// 滚轮对象指针
static lv_obj_t * roller_year;
static lv_obj_t * roller_month;
static lv_obj_t * roller_day;
static lv_obj_t * roller_hour;
static lv_obj_t * roller_minute;
static lv_obj_t * roller_second;

// 滚轮选项字符串动态内存指针
static char * opts_year = NULL;
static char * opts_month = NULL;
static char * opts_day = NULL;
static char * opts_hour = NULL;
static char * opts_min_sec = NULL;

static bool opts_inited = false;

/**
 * @brief 动态分配内存并生成滚轮选项字符串，以 "\n" 分隔
 */
static void init_roller_options(void)
{
    if (opts_inited) return;

    opts_year    = (char *)malloc_ccm(256); 
    opts_month   = (char *)malloc_ccm(64);  
    opts_day     = (char *)malloc_ccm(128); 
    opts_hour    = (char *)malloc_ccm(100); 
    opts_min_sec = (char *)malloc_ccm(256); 

    if (!opts_year || !opts_month || !opts_day || !opts_hour || !opts_min_sec) {
        return; // 内存分配失败
    }

    int offset = 0;
    // 年份: 2020 - 2060
    for (int i = 20; i <= 60; i++) {
        offset += sprintf(opts_year + offset, "20%02d\n", i);
    }
    if(offset > 0) opts_year[offset - 1] = '\0';

    // 月份: 1 - 12
    offset = 0;
    for (int i = 1; i <= 12; i++) {
        offset += sprintf(opts_month + offset, "%02d\n", i);
    }
    if(offset > 0) opts_month[offset - 1] = '\0';

    // 日: 1 - 31
    offset = 0;
    for (int i = 1; i <= 31; i++) {
        offset += sprintf(opts_day + offset, "%02d\n", i);
    }
    if(offset > 0) opts_day[offset - 1] = '\0';

    // 时: 0 - 23
    offset = 0;
    for (int i = 0; i <= 23; i++) {
        offset += sprintf(opts_hour + offset, "%02d\n", i);
    }
    if(offset > 0) opts_hour[offset - 1] = '\0';

    // 分/秒: 0 - 59
    offset = 0;
    for (int i = 0; i <= 59; i++) {
        offset += sprintf(opts_min_sec + offset, "%02d\n", i);
    }
    if(offset > 0) opts_min_sec[offset - 1] = '\0';

    opts_inited = true;
}

/**
 * @brief 点击保存按钮的回调
 */
static void btn_save_event_cb(lv_event_t * e)
{
    if(!opts_inited) return; 

    // 将滚轮上选取的时间写回标准库结构体中
    now_date.RTC_Year  = lv_roller_get_selected(roller_year) + 20;
    now_date.RTC_Month = lv_roller_get_selected(roller_month) + 1;
    now_date.RTC_Date  = lv_roller_get_selected(roller_day) + 1;
    
    now_time.RTC_Hours   = lv_roller_get_selected(roller_hour);
    now_time.RTC_Minutes = lv_roller_get_selected(roller_minute);
    now_time.RTC_Seconds = lv_roller_get_selected(roller_second);

    // 为了兼容你之前代码里可能还在用的大写全局变量，建议这里也同步更新一下
    RTC_Year = now_date.RTC_Year;
    RTC_Moth = now_date.RTC_Month;
    RTC_Date = now_date.RTC_Date;
    RTC_Hour = now_time.RTC_Hours;
    RTC_Mint = now_time.RTC_Minutes;
    RTC_Secd = now_time.RTC_Seconds;

    // 调用底层更新 RTC 时间（注意：RTC_Set_Clock 内部需确保把 now_date/now_time 写进硬件寄存器）
    RTC_Set_Clock();
}

/**
 * @brief 创建统一样式的滚轮（保留60px高度，压缩内部间距，统一宽度）
 */
static lv_obj_t * create_custom_roller(lv_obj_t * parent, const char * opts, uint16_t sel_idx, lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t * roller = lv_roller_create(parent);
    
    if(opts) {
        lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
    } else {
        lv_roller_set_options(roller, "00", LV_ROLLER_MODE_NORMAL);
    }
    
    lv_obj_set_size(roller, w, 60); 
    lv_obj_set_style_text_font(roller, &lv_font_12, 0);
    lv_obj_set_style_text_line_space(roller, 5, 0); 
    lv_obj_set_style_pad_all(roller, 2, LV_PART_SELECTED);
    lv_obj_set_style_pad_all(roller, 2, 0); 
    lv_obj_align(roller, LV_ALIGN_TOP_LEFT, x, y);
    
    lv_roller_set_selected(roller, sel_idx, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(roller, lv_palette_main(LV_PALETTE_BLUE), LV_PART_SELECTED);
    
    return roller;
}

/**
 * @brief 创建单位文本标签，跟随滚轮垂直居中
 */
static void create_unit_label(lv_obj_t * parent, lv_obj_t * roller, const char * text)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_12, 0);
    lv_label_set_text(label, text);
    lv_obj_align_to(label, roller, LV_ALIGN_OUT_RIGHT_MID, 4, 0); 
}

void Create_Time_Set_Unit(void)
{
    if (time_set_cont != NULL) return;

    init_roller_options();

    time_set_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(time_set_cont, 240, 180);
    lv_obj_center(time_set_cont);
    
    lv_obj_set_style_bg_color(time_set_cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(time_set_cont, 0, 0);
    lv_obj_set_style_radius(time_set_cont, 0, 0);
    lv_obj_set_style_pad_all(time_set_cont, 0, 0);
    lv_obj_clear_flag(time_set_cont, LV_OBJ_FLAG_SCROLLABLE); 

    // 【修改点】：直接使用 now_date 和 now_time 结构体内的成员读取当前时刻
    int year_idx = (now_date.RTC_Year >= 20 && now_date.RTC_Year <= 60) ? (now_date.RTC_Year - 20) : 6; 
    int month_idx = (now_date.RTC_Month >= 1 && now_date.RTC_Month <= 12) ? (now_date.RTC_Month - 1) : 0;
    int day_idx = (now_date.RTC_Date >= 1 && now_date.RTC_Date <= 31) ? (now_date.RTC_Date - 1) : 0;
    int hour_idx = (now_time.RTC_Hours <= 23) ? now_time.RTC_Hours : 12;
    int min_idx = (now_time.RTC_Minutes <= 59) ? now_time.RTC_Minutes : 0;
    int sec_idx = (now_time.RTC_Seconds <= 59) ? now_time.RTC_Seconds : 0;

    // 精细计算居中坐标
    lv_coord_t uniform_w = 45; 
    lv_coord_t col1_x = 18;
    lv_coord_t col2_x = 90;  
    lv_coord_t col3_x = 162; 

    // 1. 第一排：年月日 (高度固定为60，y=10)
    lv_coord_t y_date = 10;
    roller_year  = create_custom_roller(time_set_cont, opts_year, year_idx, col1_x, y_date, uniform_w);
    create_unit_label(time_set_cont, roller_year, "年");

    roller_month = create_custom_roller(time_set_cont, opts_month, month_idx, col2_x, y_date, uniform_w);
    create_unit_label(time_set_cont, roller_month, "月");

    roller_day   = create_custom_roller(time_set_cont, opts_day, day_idx, col3_x, y_date, uniform_w);
    create_unit_label(time_set_cont, roller_day, "日");

    // 2. 第二排：时分秒 (高度固定为60，y=80)
    lv_coord_t y_time = 80;
    roller_hour  = create_custom_roller(time_set_cont, opts_hour, hour_idx, col1_x, y_time, uniform_w);
    create_unit_label(time_set_cont, roller_hour, "时");

    roller_minute = create_custom_roller(time_set_cont, opts_min_sec, min_idx, col2_x, y_time, uniform_w);
    create_unit_label(time_set_cont, roller_minute, "分");

    roller_second = create_custom_roller(time_set_cont, opts_min_sec, sec_idx, col3_x, y_time, uniform_w);
    create_unit_label(time_set_cont, roller_second, "秒");

    // 3. 保存按钮 (下移至 y=150)
    lv_obj_t * btn_save = lv_btn_create(time_set_cont);
    lv_obj_set_size(btn_save, 100, 22);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_set_style_radius(btn_save, 5, 0);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_color(btn_save, lv_color_black(), 0);
    
    // 移除按钮阴影
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    
    lv_obj_add_event_cb(btn_save, btn_save_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_label = lv_label_create(btn_save);
    lv_obj_set_style_text_font(btn_label, &lv_font_12, 0);
    lv_label_set_text(btn_label, "保存设置");
    lv_obj_center(btn_label);
}

void Update_Time_Set_Unit(void)
{
	
}

void Remove_Time_Set_Unit(void)
{
    if (time_set_cont != NULL) {
        lv_obj_del(time_set_cont);
        time_set_cont = NULL;
    }
    
    // 销毁页面时释放分配给滚轮内容的内存
    if (opts_year)    { free_ccm(opts_year);    opts_year = NULL; }
    if (opts_month)   { free_ccm(opts_month);   opts_month = NULL; }
    if (opts_day)     { free_ccm(opts_day);     opts_day = NULL; }
    if (opts_hour)    { free_ccm(opts_hour);    opts_hour = NULL; }
    if (opts_min_sec) { free_ccm(opts_min_sec); opts_min_sec = NULL; }
    
    opts_inited = false; // 标记置位，下次重新进入时可重新分配
}
