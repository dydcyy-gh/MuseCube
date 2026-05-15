#include "about_unit.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "page_manager.h"
#include "variables.h"
#include "rtc_clock.h"
#include "lunar.h"
#include "malloc.h"

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * cal_cont;
	lv_obj_t * day_view_cont;
	lv_obj_t * month_view_cont;

	lv_obj_t * year_month_label;
	lv_obj_t * day_label;
	lv_obj_t * week_label;
	lv_obj_t * lunar_label;
	lv_obj_t * gz_animal_label;

	lv_obj_t * calendar;
	lv_obj_t * month_year_month_label;

	uint16_t disp_y;
	uint8_t  disp_m;
	uint8_t  disp_d;
	bool     is_today_view;

	uint8_t temp_jieqi_m[24], temp_jieqi_d[24];
	uint8_t temp_jieri_m[30], temp_jieri_d[30];
	uint16_t last_calc_year;
} calendar_state_t;

static calendar_state_t *cs = NULL;

static const char *weekdays_str[] = {"日", "一", "二", "三", "四", "五", "六", "日"};
static const char *lunar_months[] = {"", "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "冬月", "腊月"};
static const char *lunar_days[] = {
	"", "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
	"十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
	"廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"
};
static const char *tian_gan_str[] = {"", "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char *di_zhi_str[] = {"", "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
static const char *animal_str[] = {"", "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};

// ================= 辅助/回调函数 =================

static void update_day_view_ui(void)
{
	char buf[64];
	sprintf(buf, "%04d年%d月", cs->disp_y, cs->disp_m);
	lv_label_set_text(cs->year_month_label, buf);

	sprintf(buf, "%02d", cs->disp_d);
	lv_label_set_text(cs->day_label, buf);

	uint8_t wd = GetWeekDay(cs->disp_y, cs->disp_m, cs->disp_d);
	sprintf(buf, "星期%s", weekdays_str[wd == 0 ? 7 : wd]);
	lv_label_set_text(cs->week_label, buf);

	if (cs->last_calc_year != cs->disp_y) {
		cs->last_calc_year = cs->disp_y;
		The_24_solar_terms(cs->disp_y, cs->temp_jieqi_m, cs->temp_jieqi_d);
		The_30_Festival(cs->disp_y, cs->temp_jieri_m, cs->temp_jieri_d);
	}

	Solar_t sol = {cs->disp_y, cs->disp_m, cs->disp_d};
	Lunar_t lun;
	Solar2Lunar(&sol, &lun, cs->temp_jieri_m, cs->temp_jieri_d, cs->temp_jieqi_m, cs->temp_jieqi_d);

	uint8_t lm = (lun.month >= 1 && lun.month <= 12) ? lun.month : 0;
	uint8_t ld = (lun.date >= 1 && lun.date <= 30) ? lun.date : 0;
	sprintf(buf, "农历 %s%s%s", lun.is_leap_month ? "闰" : "", lunar_months[lm], lunar_days[ld]);
	lv_label_set_text(cs->lunar_label, buf);

	uint8_t tg = (lun.tian_gan >= 1 && lun.tian_gan <= 10) ? lun.tian_gan : 0;
	uint8_t dz = (lun.di_zhi >= 1 && lun.di_zhi <= 12) ? lun.di_zhi : 0;
	uint8_t an = (lun.animal >= 1 && lun.animal <= 12) ? lun.animal : 0;
	sprintf(buf, "%s%s年 [%s]", tian_gan_str[tg], di_zhi_str[dz], animal_str[an]);
	lv_label_set_text(cs->gz_animal_label, buf);
}

static void update_month_banner_text(void)
{
	const lv_calendar_date_t * d = lv_calendar_get_showed_date(cs->calendar);
	if(d) {
		char buf[32];
		sprintf(buf, "%04d年%d月", d->year, d->month);
		lv_label_set_text(cs->month_year_month_label, buf);
	}
}

// === 日视图操作 ===
static void add_days(int days)
{
	int d = cs->disp_d + days;
	while(d > DaysInMonth(cs->disp_y, cs->disp_m)) {
		d -= DaysInMonth(cs->disp_y, cs->disp_m);
		cs->disp_m++;
		if(cs->disp_m > 12) { cs->disp_m = 1; cs->disp_y++; }
	}
	while(d < 1) {
		cs->disp_m--;
		if(cs->disp_m < 1) { cs->disp_m = 12; cs->disp_y--; }
		d += DaysInMonth(cs->disp_y, cs->disp_m);
	}
	cs->disp_d = d;
	update_day_view_ui();
}

static void btn_prev_cb(lv_event_t * e) { add_days(-1); cs->is_today_view = false; }
static void btn_next_cb(lv_event_t * e) { add_days(1); cs->is_today_view = false; }

static void banner_click_cb(lv_event_t * e)
{
	lv_obj_add_flag(cs->day_view_cont, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(cs->month_view_cont, LV_OBJ_FLAG_HIDDEN);

	lv_calendar_set_today_date(cs->calendar, 2000 + now_date.RTC_Year, now_date.RTC_Month, now_date.RTC_Date);
	lv_calendar_set_showed_date(cs->calendar, cs->disp_y, cs->disp_m);
	update_month_banner_text();
}

// === 月视图操作 ===
static void month_prev_cb(lv_event_t * e)
{
	const lv_calendar_date_t * d = lv_calendar_get_showed_date(cs->calendar);
	uint32_t y = d->year;
	uint32_t m = d->month - 1;
	if(m < 1) { m = 12; y--; }
	lv_calendar_set_showed_date(cs->calendar, y, m);
	update_month_banner_text();
}

static void month_next_cb(lv_event_t * e)
{
	const lv_calendar_date_t * d = lv_calendar_get_showed_date(cs->calendar);
	uint32_t y = d->year;
	uint32_t m = d->month + 1;
	if(m > 12) { m = 1; y++; }
	lv_calendar_set_showed_date(cs->calendar, y, m);
	update_month_banner_text();
}

static void month_banner_click_cb(lv_event_t * e)
{
	cs->disp_y = 2000 + now_date.RTC_Year;
	cs->disp_m = now_date.RTC_Month;
	cs->disp_d = now_date.RTC_Date;
	cs->is_today_view = true;

	lv_obj_add_flag(cs->month_view_cont, LV_OBJ_FLAG_HIDDEN);
	lv_obj_clear_flag(cs->day_view_cont, LV_OBJ_FLAG_HIDDEN);
	update_day_view_ui();
}

static void calendar_event_cb(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_VALUE_CHANGED) {
		lv_calendar_date_t date;
		if(lv_calendar_get_pressed_date(cs->calendar, &date)) {
			cs->disp_y = date.year; cs->disp_m = date.month; cs->disp_d = date.day;
			cs->is_today_view = false;

			lv_obj_add_flag(cs->month_view_cont, LV_OBJ_FLAG_HIDDEN);
			lv_obj_clear_flag(cs->day_view_cont, LV_OBJ_FLAG_HIDDEN);
			update_day_view_ui();
		}
	}
}

// ================= 主构建函数 =================

void Create_Calendar_Unit(void)
{
	if(cs) return;

	cs = (calendar_state_t *)malloc_ccm(sizeof(calendar_state_t));
	if (!cs) return;
	memset(cs, 0, sizeof(calendar_state_t));

	cs->cal_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(cs->cal_cont, 240, 180);
	lv_obj_center(cs->cal_cont);
	lv_obj_set_style_pad_all(cs->cal_cont, 0, 0);
	lv_obj_set_style_border_width(cs->cal_cont, 0, 0);
	lv_obj_clear_flag(cs->cal_cont, LV_OBJ_FLAG_SCROLLABLE);

	// --- 1. 日视图容器 ---
	cs->day_view_cont = lv_obj_create(cs->cal_cont);
	lv_obj_set_size(cs->day_view_cont, 240, 180);
	lv_obj_set_style_pad_all(cs->day_view_cont, 0, 0);
	lv_obj_set_style_border_width(cs->day_view_cont, 0, 0);
	lv_obj_clear_flag(cs->day_view_cont, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t * top_banner = lv_obj_create(cs->day_view_cont);
	lv_obj_set_size(top_banner, 240, 20);
	lv_obj_align(top_banner, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_pad_all(top_banner, 0, 0);
	lv_obj_set_style_border_width(top_banner, 0, 0);
	lv_obj_set_style_radius(top_banner, 0, 0);
	lv_obj_set_style_bg_color(top_banner, lv_palette_lighten(LV_PALETTE_BLUE, 1), 0);
	lv_obj_clear_flag(top_banner, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(top_banner, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(top_banner, banner_click_cb, LV_EVENT_CLICKED, NULL);

	cs->year_month_label = lv_label_create(top_banner);
	lv_obj_set_style_text_font(cs->year_month_label, &lv_font_16, 0);
	lv_obj_set_style_text_color(cs->year_month_label, lv_color_white(), 0);
	lv_obj_center(cs->year_month_label);

	// --- 日视图 前一天 按钮 ---
	lv_obj_t * btn_prev = lv_btn_create(top_banner);
	lv_obj_set_size(btn_prev, 40, 20);
	lv_obj_align(btn_prev, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_set_style_bg_opa(btn_prev, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(btn_prev, 0, 0);
	lv_obj_set_style_border_width(btn_prev, 0, 0); // 取消边框
	
	lv_obj_t * lbl_prev = lv_label_create(btn_prev);
	lv_obj_set_style_text_font(lbl_prev, &lv_font_16, 0); // 设置字体
	lv_label_set_text(lbl_prev, "<"); // 修改文本
	lv_obj_set_style_text_color(lbl_prev, lv_color_white(), 0);
	lv_obj_center(lbl_prev);
	lv_obj_add_event_cb(btn_prev, btn_prev_cb, LV_EVENT_CLICKED, NULL);

	// --- 日视图 后一天 按钮 ---
	lv_obj_t * btn_next = lv_btn_create(top_banner);
	lv_obj_set_size(btn_next, 40, 20);
	lv_obj_align(btn_next, LV_ALIGN_RIGHT_MID, 0, 0);
	lv_obj_set_style_bg_opa(btn_next, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(btn_next, 0, 0);
	lv_obj_set_style_border_width(btn_next, 0, 0); // 取消边框
	
	lv_obj_t * lbl_next = lv_label_create(btn_next);
	lv_obj_set_style_text_font(lbl_next, &lv_font_16, 0); // 设置字体
	lv_label_set_text(lbl_next, ">"); // 修改文本
	lv_obj_set_style_text_color(lbl_next, lv_color_white(), 0);
	lv_obj_center(lbl_next);
	lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);

	cs->day_label = lv_label_create(cs->day_view_cont);
	lv_obj_set_style_text_font(cs->day_label, &lv_font_40, 0);
	lv_obj_align(cs->day_label, LV_ALIGN_CENTER, 0, -30);

	cs->week_label = lv_label_create(cs->day_view_cont);
	lv_obj_set_style_text_font(cs->week_label, &lv_font_16, 0);
	lv_obj_align(cs->week_label, LV_ALIGN_CENTER, 0, 5);

	cs->lunar_label = lv_label_create(cs->day_view_cont);
	lv_obj_set_style_text_font(cs->lunar_label, &lv_font_12, 0);
	lv_obj_align(cs->lunar_label, LV_ALIGN_BOTTOM_MID, 0, -45);

	cs->gz_animal_label = lv_label_create(cs->day_view_cont);
	lv_obj_set_style_text_font(cs->gz_animal_label, &lv_font_12, 0);
	lv_obj_set_style_text_color(cs->gz_animal_label, lv_palette_main(LV_PALETTE_GREY), 0);
	lv_obj_align(cs->gz_animal_label, LV_ALIGN_BOTTOM_MID, 0, -28);

	// --- 2. 月视图容器 ---
	cs->month_view_cont = lv_obj_create(cs->cal_cont);
	lv_obj_set_size(cs->month_view_cont, 240, 180);
	lv_obj_set_style_pad_all(cs->month_view_cont, 0, 0);
	lv_obj_set_style_border_width(cs->month_view_cont, 0, 0);
	lv_obj_add_flag(cs->month_view_cont, LV_OBJ_FLAG_HIDDEN);

	lv_obj_t * month_banner = lv_obj_create(cs->month_view_cont);
	lv_obj_set_size(month_banner, 240, 20);
	lv_obj_align(month_banner, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_pad_all(month_banner, 0, 0);
	lv_obj_set_style_border_width(month_banner, 0, 0);
	lv_obj_set_style_radius(month_banner, 0, 0);
	lv_obj_set_style_bg_color(month_banner, lv_palette_lighten(LV_PALETTE_BLUE, 1), 0);
	lv_obj_clear_flag(month_banner, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(month_banner, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(month_banner, month_banner_click_cb, LV_EVENT_CLICKED, NULL);

	cs->month_year_month_label = lv_label_create(month_banner);
	lv_obj_set_style_text_font(cs->month_year_month_label, &lv_font_16, 0);
	lv_obj_set_style_text_color(cs->month_year_month_label, lv_color_white(), 0);
	lv_obj_center(cs->month_year_month_label);

	// --- 月视图 前一月 按钮 ---
	lv_obj_t * m_btn_prev = lv_btn_create(month_banner);
	lv_obj_set_size(m_btn_prev, 40, 20);
	lv_obj_align(m_btn_prev, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_set_style_bg_opa(m_btn_prev, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(m_btn_prev, 0, 0);
	lv_obj_set_style_border_width(m_btn_prev, 0, 0); // 取消边框
	
	lv_obj_t * m_lbl_prev = lv_label_create(m_btn_prev);
	lv_obj_set_style_text_font(m_lbl_prev, &lv_font_16, 0); // 设置字体
	lv_label_set_text(m_lbl_prev, "<"); // 修改文本
	lv_obj_set_style_text_color(m_lbl_prev, lv_color_white(), 0);
	lv_obj_center(m_lbl_prev);
	lv_obj_add_event_cb(m_btn_prev, month_prev_cb, LV_EVENT_CLICKED, NULL);

	// --- 月视图 后一月 按钮 ---
	lv_obj_t * m_btn_next = lv_btn_create(month_banner);
	lv_obj_set_size(m_btn_next, 40, 20);
	lv_obj_align(m_btn_next, LV_ALIGN_RIGHT_MID, 0, 0);
	lv_obj_set_style_bg_opa(m_btn_next, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(m_btn_next, 0, 0);
	lv_obj_set_style_border_width(m_btn_next, 0, 0); // 取消边框
	
	lv_obj_t * m_lbl_next = lv_label_create(m_btn_next);
	lv_obj_set_style_text_font(m_lbl_next, &lv_font_16, 0); // 设置字体
	lv_label_set_text(m_lbl_next, ">"); // 修改文本
	lv_obj_set_style_text_color(m_lbl_next, lv_color_white(), 0);
	lv_obj_center(m_lbl_next);
	lv_obj_add_event_cb(m_btn_next, month_next_cb, LV_EVENT_CLICKED, NULL);

	cs->calendar = lv_calendar_create(cs->month_view_cont);
	lv_obj_set_size(cs->calendar, 240, 160);
	lv_obj_align(cs->calendar, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_set_style_radius(cs->calendar, 0, 0);
	lv_obj_set_style_border_width(cs->calendar, 0, 0);
	lv_obj_set_style_pad_all(cs->calendar, 0, 0);

    lv_obj_clear_flag(cs->month_view_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cs->calendar, LV_OBJ_FLAG_SCROLLABLE);
	
	lv_obj_t * cal_btnm = lv_calendar_get_btnmatrix(cs->calendar);
	lv_obj_clear_flag(cal_btnm, LV_OBJ_FLAG_SCROLLABLE); 
	lv_obj_set_style_border_width(cal_btnm, 0, LV_PART_ITEMS | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(cal_btnm, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
	lv_obj_set_style_border_width(cal_btnm, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
	lv_obj_set_style_bg_opa(cal_btnm, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(cal_btnm, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_ITEMS | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(cal_btnm, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(cal_btnm, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(cal_btnm, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_ITEMS | LV_STATE_PRESSED);

	lv_obj_add_event_cb(cs->calendar, calendar_event_cb, LV_EVENT_ALL, NULL);

	// 初始化为"今天"
	cs->disp_y = 2000 + now_date.RTC_Year;
	cs->disp_m = now_date.RTC_Month;
	cs->disp_d = now_date.RTC_Date;
	cs->is_today_view = true;
	update_day_view_ui();
}

void Update_Calendar_Unit(void)
{
	if(!cs) return;

	if (cs->is_today_view) {
		if (cs->disp_d != now_date.RTC_Date || cs->disp_m != now_date.RTC_Month) {
			cs->disp_y = 2000 + now_date.RTC_Year;
			cs->disp_m = now_date.RTC_Month;
			cs->disp_d = now_date.RTC_Date;
			update_day_view_ui();
		}
	}
}

void Remove_Calendar_Unit(void)
{
	if (!cs) return;

	if (cs->cal_cont) {
		lv_obj_del(cs->cal_cont);
	}

	free_ccm(cs);
	cs = NULL;
}
