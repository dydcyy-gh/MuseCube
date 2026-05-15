#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "variables.h"
#include "settings_unit.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include "page_manager.h"

static lv_obj_t * settings_cont = NULL;

typedef struct {
    const char * item_name;
    lv_event_cb_t click_cb;
} setting_item_t;

/* --- 预留的点击回调函数 --- */
static void key_test_click_cb(lv_event_t * e) {
	Page_Request_Switch(PAGE_KEY_TEST);
}

static void log_ctrl_click_cb(lv_event_t * e) {
	Page_Request_Switch(PAGE_LOG_CTRL);
}

static void about_click_cb(lv_event_t * e) {
	Page_Request_Switch(PAGE_ABOUT);
}

static void es9018_click_cb(lv_event_t * e) {
	Page_Request_Switch(PAGE_ES9018);
}

static void time_set_click_cb(lv_event_t * e) {
	Page_Request_Switch(PAGE_TIME_SET);
}

static const setting_item_t settings_list[] = {
    {"输入设备监控",     key_test_click_cb},
    {"日志设置与显示",   log_ctrl_click_cb},
    {"关于本机信息",     about_click_cb},
	{"DAC设置",          es9018_click_cb},
	{"日期与时间",       time_set_click_cb},
};

static void remove_default_style(lv_obj_t * obj)
{
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void Create_Settings_Unit(void)
{
    if (settings_cont != NULL) return;

    // 1. 创建主容器 240*180
    settings_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(settings_cont, 240, 180);
    lv_obj_center(settings_cont);
    
    // 移除默认样式，设置白色背景
    remove_default_style(settings_cont);
    lv_obj_set_style_bg_opa(settings_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_cont, lv_color_white(), LV_PART_MAIN);
    
    // 设置垂直滚动 (去掉 Flex 布局相关的代码，避免默认间距干扰)
    lv_obj_set_scroll_dir(settings_cont, LV_DIR_VER);
    
    // 2. 遍历设置项数组，自动生成列表 UI
    int item_count = sizeof(settings_list) / sizeof(settings_list[0]);
    for (int i = 0; i < item_count; i++) {
        // 创建每行的容器
        lv_obj_t * item_cont = lv_obj_create(settings_cont);
        lv_obj_set_size(item_cont, 240, 30); // 宽度240，行高30
        
        // 【核心修复】：直接通过 Y 轴坐标计算位置 (30 * i)
        lv_obj_set_pos(item_cont, 0, i * 30); 
        
        remove_default_style(item_cont);
        
        // 增加底部 1pix 灰色横线（仅底部显示边框）
        lv_obj_set_style_border_width(item_cont, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(item_cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(item_cont, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        
        // 增加交互背景色 (悬浮淡灰色，按下深灰色)
        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
        
        // 使整行可点击并绑定回调
        lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
        if (settings_list[i].click_cb != NULL) {
            lv_obj_add_event_cb(item_cont, settings_list[i].click_cb, LV_EVENT_CLICKED, (void *)settings_list[i].item_name);
        }
        
        // 创建汉字文本 Label
        lv_obj_t * text_label = lv_label_create(item_cont);
        lv_obj_set_style_text_font(text_label, &lv_font_16, LV_PART_MAIN); // 设置中文字体
        lv_label_set_text(text_label, settings_list[i].item_name);
        lv_obj_align(text_label, LV_ALIGN_LEFT_MID, 5, 0); // 距左侧 5pix，上下居中
    }
}

void Update_Settings_Unit(void)
{
    return;
}

void Remove_Settings_Unit(void)
{
    if (settings_cont != NULL) {
        lv_obj_del(settings_cont);
        settings_cont = NULL;
    }
}
