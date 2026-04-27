#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "desktop_page.h"
#include "rtc_clock.h"   
#include "lunar.h"       
#include "variables.h"  

// 引入外部的鼠标坐标变量
extern float cursor_x;
extern float cursor_y;

// UI 对象指针
static lv_obj_t * bg_img = NULL;
static lv_obj_t * rtc_label_3 = NULL;
static lv_obj_t * rtc_label_4 = NULL;
static lv_obj_t * top_img = NULL;

// 记录日期更新状态
static uint8_t last_date = 0;

LV_IMG_DECLARE(pic_top);
LV_IMG_DECLARE(pic_back);

static void wallpaper_click_cb(lv_event_t * e)
{
    Page_Request_Switch(PAGE_DESKTOP);
}

void Create_Start_Unit(void)
{
    if (bg_img != NULL || top_img != NULL) return;

    // 清除屏幕的可滚动标志，强制隐藏滚动条
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* --------- 1. 创建底层背景 (最底层) --------- */
    bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(bg_img, &pic_back);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bg_img, wallpaper_click_cb, LV_EVENT_CLICKED, NULL);

    /* --------- 2. 创建时钟时间 (中层) --------- */
    rtc_label_3 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(rtc_label_3, &lv_font_56, 0);
    lv_obj_align(rtc_label_3, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_color(rtc_label_3, lv_color_hex(0xFFB3B3), 0);
    lv_label_set_text_fmt(rtc_label_3, "%02d:%02d", now_time.RTC_Hours, now_time.RTC_Minutes);

    /* --------- 3. 创建时钟日期 (中层) --------- */
    rtc_label_4 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(rtc_label_4, &lv_font_12, 0);
    lv_obj_align(rtc_label_4, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(rtc_label_4, lv_color_hex(0xFFB3B3), 0);
    last_date = 0; // 重置日期状态以强制在下一次 Update 时刷新文本

    /* --------- 4. 创建顶层透明图片 (最顶层) --------- */
    top_img = lv_img_create(lv_scr_act());
    lv_img_set_src(top_img, &pic_top); 
    lv_obj_clear_flag(top_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(top_img, 80, 50);
}

void Update_Start_Unit(void)
{
    /* --------- 1. 更新时钟时间 --------- */
    if (rtc_label_3 != NULL)
    {
        static uint8_t last_min = 0;
        static uint8_t last_apm = 2;
        if(now_time.RTC_Minutes != last_min || RTC_HFmt != last_apm)
        {
            last_min = now_time.RTC_Minutes;
            last_apm = RTC_HFmt;
            lv_label_set_text_fmt(rtc_label_3, "%02d:%02d", now_time.RTC_Hours, now_time.RTC_Minutes);
        }
    }

    /* --------- 2. 更新时钟日期 --------- */
    if (rtc_label_4 != NULL && now_date.RTC_Date != last_date)
    {
        last_date = now_date.RTC_Date;
        
        const char *weekday[7]  = {"一","二","三","四","五","六","日"};
        const char *tiangan[10] = {"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
        const char *dizhi[12]   = {"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
        const char *leap[2]     = {"","闰"};
        const char *month[12]   = {"正","二","三","四","五","六","七","八","九","十","冬","腊"};
        const char *xun[3]      = {"初","十","廿"};
        const char *date[10]    = {"一","二","三","四","五","六","七","八","九","十"};
        const char *jieqi[25]   = {"",  "立春", "雨水", "惊蛰", "春分", "清明", "谷雨",
                                        "立夏", "小满", "芒种", "夏至", "小暑", "大暑", 
                                        "立秋", "处暑", "白露", "秋分", "寒露", "霜降",
                                        "立冬", "小雪", "大雪", "冬至", "小寒", "大寒"  };
        const char *jieri[31]   = {"",  "元旦",   "情人节", "妇女节", "植树节",   "愚人节",   "劳动节", 
                                        "青年节", "母亲节", "儿童节", "父亲节",   "建党节",   "建军节",
                                        "教师节", "国庆节", "万圣夜", "感恩节",   "平安夜",   "圣诞节",
                                        "春节",   "元宵节", "龙抬头", "端午节",   "七夕节",   "中元节",
                                        "中秋节", "重阳节", "腊八节", "北方小年", "南方小年", "除夕"};
        
        if (now_lunar.jie_ri != 0 || now_lunar.jie_qi != 0) 
        {
            lv_label_set_text_fmt(rtc_label_4, "%d月%d日周%s %s%s月%s%s %s%s",
                now_date.RTC_Month, now_date.RTC_Date, weekday[now_date.RTC_WeekDay-1],
                leap[now_lunar.is_leap_month],month[now_lunar.month-1],
                xun[(now_lunar.date-1)/10], date[(now_lunar.date-1)%10],
                jieqi[now_lunar.jie_qi], jieri[now_lunar.jie_ri]);
        } 
        else 
        {
            lv_label_set_text_fmt(rtc_label_4, "%d月%d日周%s %s%s年%s%s月%s%s",
                now_date.RTC_Month, now_date.RTC_Date, weekday[now_date.RTC_WeekDay-1],
                tiangan[now_lunar.tian_gan-1], dizhi[now_lunar.di_zhi-1],
                leap[now_lunar.is_leap_month], month[now_lunar.month-1],
                xun[(now_lunar.date-1)/10], date[(now_lunar.date-1)%10]);
        }
    }

    /* --------- 3. 更新顶层图片跟随鼠标 --------- */
    if (top_img != NULL) 
    {
        int16_t offset_x = ((int16_t)cursor_x - 120) / 12;
        int16_t offset_y = ((int16_t)cursor_y - 120) / 12;
        
        // 限幅保护 ±20 像素
        if(offset_x > 10) offset_x = 10;
        if(offset_x < -10) offset_x = -10;
        if(offset_y > 10) offset_y = 10;
        if(offset_y < -10) offset_y = -10;

        int16_t pos_x = 80 + offset_x;
        int16_t pos_y = 50 + offset_y;

        lv_obj_set_pos(top_img, pos_x, pos_y);
    }
}

void Remove_Start_Unit(void)
{
    // 按照从上到下的顺序进行销毁
    if (top_img != NULL) {
        lv_obj_del(top_img);
        top_img = NULL;
    }
    if (rtc_label_4 != NULL) {
        lv_obj_del(rtc_label_4);
        rtc_label_4 = NULL;
    }
    if (rtc_label_3 != NULL) {
        lv_obj_del(rtc_label_3);
        rtc_label_3 = NULL;
    }
    if (bg_img != NULL) {
        lv_obj_del(bg_img);
        bg_img = NULL;
    }
    
    last_date = 0;
}
