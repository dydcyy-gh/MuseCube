#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "rtc_clock.h"
#include "page_manager.h"
#include "variables.h"

// 引入外部 CCM 内存分配函数
extern void *malloc_ccm(uint32_t bytes);
extern void free_ccm(void* ptr);

// 引入外部 STM32 标准库的时间结构体
extern RTC_DateTypeDef now_date;
extern RTC_TimeTypeDef now_time;

// 字体声明
LV_FONT_DECLARE(lv_font_12);
LV_FONT_DECLARE(lv_font_16);
LV_FONT_DECLARE(lv_font_40);

// ================= 全局 UI 对象 =================
static lv_obj_t * app_cont = NULL;

// 底部导航栏对象
static lv_obj_t * nav_bar = NULL;
static lv_obj_t * nav_btns[4] = {NULL};

static lv_obj_t * page_clock = NULL;
static lv_obj_t * page_timer = NULL;
static lv_obj_t * page_stopwatch = NULL;
static lv_obj_t * page_alarm = NULL;

// 时钟页面 UI
static lv_obj_t * clock_time_label = NULL;
static lv_obj_t * clock_date_label = NULL;

// 计时器页面 UI
static lv_obj_t * timer_val_label = NULL;
static lv_obj_t * timer_set_cont = NULL; 
static lv_obj_t * roller_t_h, * roller_t_m, * roller_t_s;
static lv_obj_t * btn_t_ctrl, * label_t_ctrl; 
static lv_obj_t * btn_t_stop;

// 秒表页面 UI
static lv_obj_t * sw_val_label = NULL;
static lv_obj_t * btn_sw_ctrl, * label_sw_ctrl;
static lv_obj_t * btn_sw_reset;

// 闹钟页面 UI
static lv_obj_t * roller_a_h, * roller_a_m;
static lv_obj_t * sw_alarm_en;
static lv_obj_t * alarm_alert_box = NULL;

// ================= 逻辑状态变量 =================
static uint8_t current_tab = 0;

typedef enum { T_IDLE, T_RUNNING, T_PAUSED } timer_state_t;
static timer_state_t timer_state = T_IDLE;
static uint32_t timer_start_tick = 0;
static uint32_t timer_remain_ms = 0;

typedef enum { SW_IDLE, SW_RUNNING, SW_PAUSED } sw_state_t;
static sw_state_t sw_state = SW_IDLE;
static uint32_t sw_start_tick = 0;
static uint32_t sw_elapsed_ms = 0;

static bool alarm_enabled = false;
static uint8_t alarm_h = 0;
static uint8_t alarm_m = 0;
static bool alarm_triggered = false; 

static uint8_t last_s = 60;
static uint32_t last_timer_s = 0xFFFFFFFF;
static uint32_t last_sw_ms = 0xFFFFFFFF;

// 大块内存从数组改为指针
static char *opts_24 = NULL;
static char *opts_60 = NULL;
static bool opts_inited = false;

// ================= 辅助/回调函数 =================

static void init_roller_options(void)
{
    if (opts_inited) return;
    
    opts_24 = (char *)malloc_ccm(100);
    opts_60 = (char *)malloc_ccm(200);

    if (opts_24) {
        int offset = 0;
        for(int i = 0; i <= 23; i++) offset += sprintf(opts_24 + offset, "%02d\n", i);
        if(offset > 0) opts_24[offset - 1] = '\0';
    }
    
    if (opts_60) {
        int offset = 0;
        for(int i = 0; i <= 59; i++) offset += sprintf(opts_60 + offset, "%02d\n", i);
        if(offset > 0) opts_60[offset - 1] = '\0';
    }
    
    opts_inited = true;
}

static lv_obj_t * create_custom_roller(lv_obj_t * parent, const char * opts, uint16_t sel_idx, lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t * roller = lv_roller_create(parent);
    if(opts) lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
    
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

static void create_unit_label(lv_obj_t * parent, lv_obj_t * roller, const char * text)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_12, 0);
    lv_label_set_text(label, text);
    lv_obj_align_to(label, roller, LV_ALIGN_OUT_RIGHT_MID, 4, 0); 
}

// 【全新】导航栏按钮点击事件
static void nav_btn_event_cb(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target(e);
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    
    // 如果点击当前已选中的，强制其保持选中状态
    if(id == current_tab) {
        lv_obj_add_state(target, LV_STATE_CHECKED);
        return;
    }
    
    // 清除其他按钮的选中状态
    for(int i = 0; i < 4; i++) {
        if (i != id && nav_btns[i]) {
            lv_obj_clear_state(nav_btns[i], LV_STATE_CHECKED);
        }
    }
    
    lv_obj_add_flag(page_clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_timer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_stopwatch, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
    
    current_tab = id;
    if(id == 0) lv_obj_clear_flag(page_clock, LV_OBJ_FLAG_HIDDEN);
    else if(id == 1) lv_obj_clear_flag(page_timer, LV_OBJ_FLAG_HIDDEN);
    else if(id == 2) lv_obj_clear_flag(page_stopwatch, LV_OBJ_FLAG_HIDDEN);
    else if(id == 3) lv_obj_clear_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
}

static void timer_ctrl_event_cb(lv_event_t * e)
{
    if(timer_state == T_IDLE) {
        uint32_t h = lv_roller_get_selected(roller_t_h);
        uint32_t m = lv_roller_get_selected(roller_t_m);
        uint32_t s = lv_roller_get_selected(roller_t_s);
        timer_remain_ms = (h * 3600 + m * 60 + s) * 1000;
        if(timer_remain_ms == 0) return;

        timer_start_tick = lv_tick_get();
        timer_state = T_RUNNING;
        
        lv_obj_add_flag(timer_set_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(timer_val_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_t_ctrl, "暂停");
    } 
    else if(timer_state == T_RUNNING) {
        // 【修改点】：在暂停时，计算出已经经过的时间，更新总剩余时间
        uint32_t elapsed = lv_tick_get() - timer_start_tick;
        if (timer_remain_ms > elapsed) {
            timer_remain_ms -= elapsed; // 扣除已流逝的时间
        } else {
            timer_remain_ms = 0;
        }

        timer_state = T_PAUSED;
        lv_label_set_text(label_t_ctrl, "继续");
    } 
    else if(timer_state == T_PAUSED) {
        // 【修改点】：继续时，只需要刷新起始 tick，因为 timer_remain_ms 已经是暂停后的剩余时间了
        timer_start_tick = lv_tick_get();
        timer_state = T_RUNNING;
        lv_label_set_text(label_t_ctrl, "暂停");
    }
}

static void timer_stop_event_cb(lv_event_t * e)
{
    timer_state = T_IDLE;
    lv_obj_clear_flag(timer_set_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(timer_val_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(label_t_ctrl, "开始");
    last_timer_s = 0xFFFFFFFF; 
}

static void sw_ctrl_event_cb(lv_event_t * e)
{
    if(sw_state == SW_IDLE || sw_state == SW_PAUSED) {
        sw_start_tick = lv_tick_get() - sw_elapsed_ms;
        sw_state = SW_RUNNING;
        lv_label_set_text(label_sw_ctrl, "暂停");
    } else {
        sw_state = SW_PAUSED;
        lv_label_set_text(label_sw_ctrl, "继续");
    }
}

static void sw_reset_event_cb(lv_event_t * e)
{
    sw_state = SW_IDLE;
    sw_elapsed_ms = 0;
    lv_label_set_text(label_sw_ctrl, "开始");
    lv_label_set_text(sw_val_label, "00:00.00");
    last_sw_ms = 0xFFFFFFFF;
}

static void alarm_switch_event_cb(lv_event_t * e)
{
    alarm_enabled = lv_obj_has_state(sw_alarm_en, LV_STATE_CHECKED);
    alarm_h = lv_roller_get_selected(roller_a_h);
    alarm_m = lv_roller_get_selected(roller_a_m);
    alarm_triggered = false;
}

static void alarm_msgbox_close_cb(lv_event_t * e)
{
    if(alarm_alert_box) {
        lv_obj_del_async(alarm_alert_box); 
        alarm_alert_box = NULL;
    }
}


// ================= 主构建函数 =================
void Create_Clock_Unit(void)
{
    if(app_cont) return;
    init_roller_options();
    
    app_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(app_cont, 240, 180);
    lv_obj_center(app_cont);
    lv_obj_set_style_pad_all(app_cont, 0, 0);
    lv_obj_set_style_border_width(app_cont, 0, 0);
    lv_obj_clear_flag(app_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 【修改点】：全新设计的自定义独立导航栏，完全摒弃矩阵按钮
    nav_bar = lv_obj_create(app_cont);
    lv_obj_set_size(nav_bar, 240, 30);
    lv_obj_align(nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 去除所有边距/边框/圆角/阴影，并设置灰色背景
    lv_obj_set_style_pad_all(nav_bar, 0, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_radius(nav_bar, 0, 0);
    lv_obj_set_style_shadow_width(nav_bar, 0, 0);
    lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0xDDDDDD), 0); // 导航栏底色（灰色）
    lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char * titles[] = {"时钟", "计时", "秒表", "闹钟"};
    for(int i = 0; i < 4; i++) {
        nav_btns[i] = lv_btn_create(nav_bar);
        lv_obj_set_size(nav_btns[i], 60, 30); // 1/4大小
        lv_obj_align(nav_btns[i], LV_ALIGN_LEFT_MID, i * 60, 0);
        
        lv_obj_set_style_pad_all(nav_btns[i], 0, 0);
        lv_obj_set_style_border_width(nav_btns[i], 0, 0);
        lv_obj_set_style_radius(nav_btns[i], 0, 0); // 去除圆角，呈方块形
        lv_obj_set_style_shadow_width(nav_btns[i], 0, 0); // 无阴影
        
        // 未选中时透明，透出导航栏的灰色；选中时变为淡蓝色
        lv_obj_set_style_bg_opa(nav_btns[i], LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(nav_btns[i], LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(nav_btns[i], lv_palette_lighten(LV_PALETTE_BLUE, 3), LV_STATE_CHECKED);
        
        lv_obj_add_flag(nav_btns[i], LV_OBJ_FLAG_CHECKABLE); // 允许成为 Checkable 状态
        
        // 创建内部文本
        lv_obj_t * label = lv_label_create(nav_btns[i]);
        lv_obj_set_style_text_font(label, &lv_font_12, 0);
        // 使文本颜色在任意状态下都是黑色，更容易看清
        lv_obj_set_style_text_color(label, lv_color_black(), LV_STATE_DEFAULT); 
        lv_label_set_text(label, titles[i]);
        lv_obj_center(label);
        
        // 传递 i 作为 user_data 给回调
        lv_obj_add_event_cb(nav_btns[i], nav_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
    
    // 默认选中第0个（时钟）
    current_tab = 0;
    lv_obj_add_state(nav_btns[0], LV_STATE_CHECKED);

    lv_obj_t * pages[4];
    for(int i=0; i<4; i++) {
        pages[i] = lv_obj_create(app_cont);
        lv_obj_set_size(pages[i], 240, 150);
        lv_obj_align(pages[i], LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_pad_all(pages[i], 0, 0);
        lv_obj_set_style_border_width(pages[i], 0, 0);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
        if(i != 0) lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    page_clock = pages[0]; page_timer = pages[1]; page_stopwatch = pages[2]; page_alarm = pages[3];

    // ------ 1. 时钟页面 ------
    clock_time_label = lv_label_create(page_clock);
    lv_obj_set_style_text_font(clock_time_label, &lv_font_40, 0);
    lv_label_set_text(clock_time_label, "00:00:00");
    lv_obj_align(clock_time_label, LV_ALIGN_CENTER, 0, -15);
    
    clock_date_label = lv_label_create(page_clock);
    lv_obj_set_style_text_font(clock_date_label, &lv_font_16, 0);
    lv_label_set_text(clock_date_label, "1月1日 星期一");
    lv_obj_align(clock_date_label, LV_ALIGN_CENTER, 0, 25);

    // ------ 2. 计时器页面 ------
    timer_set_cont = lv_obj_create(page_timer);
    lv_obj_set_size(timer_set_cont, 240, 100);
    lv_obj_align(timer_set_cont, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_border_width(timer_set_cont, 0, 0);
    
    lv_obj_clear_flag(timer_set_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(timer_set_cont, 0, 0);
    
    roller_t_h = create_custom_roller(timer_set_cont, opts_24, 0, 18, 10, 45);
    create_unit_label(timer_set_cont, roller_t_h, "时");

    roller_t_m = create_custom_roller(timer_set_cont, opts_60, 0, 90, 10, 45);
    create_unit_label(timer_set_cont, roller_t_m, "分");

    roller_t_s = create_custom_roller(timer_set_cont, opts_60, 0, 162, 10, 45);
    create_unit_label(timer_set_cont, roller_t_s, "秒");

    timer_val_label = lv_label_create(page_timer);
    lv_obj_set_style_text_font(timer_val_label, &lv_font_40, 0);
    lv_label_set_text(timer_val_label, "00:00:00");
    lv_obj_align(timer_val_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(timer_val_label, LV_OBJ_FLAG_HIDDEN);

    btn_t_ctrl = lv_btn_create(page_timer); lv_obj_set_size(btn_t_ctrl, 70, 30); lv_obj_align(btn_t_ctrl, LV_ALIGN_BOTTOM_LEFT, 40, -10);
    lv_obj_set_style_shadow_width(btn_t_ctrl, 0, 0); 
    label_t_ctrl = lv_label_create(btn_t_ctrl); lv_obj_set_style_text_font(label_t_ctrl, &lv_font_12, 0); lv_label_set_text(label_t_ctrl, "开始"); lv_obj_center(label_t_ctrl);
    lv_obj_add_event_cb(btn_t_ctrl, timer_ctrl_event_cb, LV_EVENT_CLICKED, NULL);

    btn_t_stop = lv_btn_create(page_timer); lv_obj_set_size(btn_t_stop, 70, 30); lv_obj_align(btn_t_stop, LV_ALIGN_BOTTOM_RIGHT, -40, -10);
    lv_obj_set_style_shadow_width(btn_t_stop, 0, 0); 
    lv_obj_t * lbl2 = lv_label_create(btn_t_stop); lv_obj_set_style_text_font(lbl2, &lv_font_12, 0); lv_label_set_text(lbl2, "结束"); lv_obj_center(lbl2);
    lv_obj_add_event_cb(btn_t_stop, timer_stop_event_cb, LV_EVENT_CLICKED, NULL);

    // ------ 3. 秒表页面 ------
    sw_val_label = lv_label_create(page_stopwatch);
    lv_obj_set_style_text_font(sw_val_label, &lv_font_40, 0);
    lv_label_set_text(sw_val_label, "00:00.00");
    lv_obj_align(sw_val_label, LV_ALIGN_CENTER, 0, -20);

    btn_sw_ctrl = lv_btn_create(page_stopwatch); lv_obj_set_size(btn_sw_ctrl, 70, 30); lv_obj_align(btn_sw_ctrl, LV_ALIGN_BOTTOM_LEFT, 40, -10);
    lv_obj_set_style_shadow_width(btn_sw_ctrl, 0, 0); 
    label_sw_ctrl = lv_label_create(btn_sw_ctrl); lv_obj_set_style_text_font(label_sw_ctrl, &lv_font_12, 0); lv_label_set_text(label_sw_ctrl, "开始"); lv_obj_center(label_sw_ctrl);
    lv_obj_add_event_cb(btn_sw_ctrl, sw_ctrl_event_cb, LV_EVENT_CLICKED, NULL);

    btn_sw_reset = lv_btn_create(page_stopwatch); lv_obj_set_size(btn_sw_reset, 70, 30); lv_obj_align(btn_sw_reset, LV_ALIGN_BOTTOM_RIGHT, -40, -10);
    lv_obj_set_style_shadow_width(btn_sw_reset, 0, 0); 
    lv_obj_t * lbl3 = lv_label_create(btn_sw_reset); lv_obj_set_style_text_font(lbl3, &lv_font_12, 0); lv_label_set_text(lbl3, "归零"); lv_obj_center(lbl3);
    lv_obj_add_event_cb(btn_sw_reset, sw_reset_event_cb, LV_EVENT_CLICKED, NULL);

    // ------ 4. 闹钟页面 ------
    roller_a_h = create_custom_roller(page_alarm, opts_24, 0, 54, 30, 45);
    create_unit_label(page_alarm, roller_a_h, "时");

    roller_a_m = create_custom_roller(page_alarm, opts_60, 0, 125, 30, 45);
    create_unit_label(page_alarm, roller_a_m, "分");
    
    sw_alarm_en = lv_switch_create(page_alarm); 
    lv_obj_set_size(sw_alarm_en, 40, 20); 
    lv_obj_align(sw_alarm_en, LV_ALIGN_BOTTOM_MID, 25, -20);
    lv_obj_add_event_cb(sw_alarm_en, alarm_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * lbl4 = lv_label_create(page_alarm); 
    lv_obj_set_style_text_font(lbl4, &lv_font_12, 0); 
    lv_label_set_text(lbl4, "闹钟开关"); 
    lv_obj_align_to(lbl4, sw_alarm_en, LV_ALIGN_OUT_LEFT_MID, -10, 0);
}

// ================= 更新函数 (每20ms调用) =================
void Update_Clock_Unit(void)
{
    if(!app_cont) return;
    
    if (now_time.RTC_Seconds != last_s) {
        last_s = now_time.RTC_Seconds;
        
        char buf[32];
        sprintf(buf, "%02d:%02d:%02d", now_time.RTC_Hours, now_time.RTC_Minutes, now_time.RTC_Seconds);
        lv_label_set_text(clock_time_label, buf);
        
        const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
        sprintf(buf, "%d月%d日 星期%s", now_date.RTC_Month, now_date.RTC_Date, weekdays[now_date.RTC_WeekDay % 7]);
        lv_label_set_text(clock_date_label, buf);
    }
    
    if (timer_state == T_RUNNING) {
        uint32_t elapsed = lv_tick_get() - timer_start_tick;
        if (elapsed >= timer_remain_ms) {
            timer_state = T_IDLE;
            lv_label_set_text(timer_val_label, "00:00:00");
            lv_label_set_text(label_t_ctrl, "开始");
        } else {
            uint32_t cur_remain = timer_remain_ms - elapsed;
            uint32_t total_s = cur_remain / 1000;
            if(total_s != last_timer_s) {
                last_timer_s = total_s;
                char buf[16];
                sprintf(buf, "%02d:%02d:%02d", (int)(total_s / 3600), (int)((total_s % 3600) / 60), (int)(total_s % 60));
                lv_label_set_text(timer_val_label, buf);
            }
        }
    }
    
    if (sw_state == SW_RUNNING) {
        sw_elapsed_ms = lv_tick_get() - sw_start_tick;
    }
    if((sw_state == SW_RUNNING) && (sw_elapsed_ms / 20 != last_sw_ms)) {
        last_sw_ms = sw_elapsed_ms / 20;
        char buf[16];
        uint32_t ms = sw_elapsed_ms;
        uint32_t total_s = ms / 1000;
        if(total_s < 3600) {
            sprintf(buf, "%02d:%02d.%02d", (int)(total_s / 60), (int)(total_s % 60), (int)((ms % 1000) / 10));
        } else {
            sprintf(buf, "%02d:%02d:%02d", (int)(total_s / 3600), (int)((total_s % 3600) / 60), (int)(total_s % 60));
        }
        lv_label_set_text(sw_val_label, buf);
    }
    
    if(alarm_enabled && !alarm_triggered) {
        if(now_time.RTC_Hours == alarm_h && now_time.RTC_Minutes == alarm_m) {
            alarm_triggered = true;
            if(!alarm_alert_box) {
                static const char * btns[] = {"关闭", ""};
                // false：关闭右上角默认关闭按钮
                alarm_alert_box = lv_msgbox_create(app_cont, "闹钟", "时间到了！", btns, false);
                lv_obj_center(alarm_alert_box);
                
                lv_obj_t * msgbox_btns = lv_msgbox_get_btns(alarm_alert_box);
                if (msgbox_btns) {
                    lv_obj_set_style_shadow_width(msgbox_btns, 0, LV_PART_ITEMS);       
                    lv_obj_set_style_text_font(msgbox_btns, &lv_font_12, LV_PART_ITEMS); 
                    // 【关键修复点】：将点击事件直接绑定给这个底部按钮对象！保证100%收到点击反馈
                    lv_obj_add_event_cb(msgbox_btns, alarm_msgbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
                }
            }
        }
    } else if (now_time.RTC_Minutes != alarm_m) {
        alarm_triggered = false; 
    }
}

// ================= 删除函数 =================
void Remove_Clock_Unit(void)
{
    if(app_cont) {
        lv_obj_del(app_cont);
        app_cont = NULL;
    }
    
    if (opts_24) { free_ccm(opts_24); opts_24 = NULL; }
    if (opts_60) { free_ccm(opts_60); opts_60 = NULL; }
    opts_inited = false;
    
    timer_state = T_IDLE;
    sw_state = SW_IDLE;
    alarm_alert_box = NULL;
}
