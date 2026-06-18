#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "variables.h"
#include "settings_unit.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include "page_manager.h"
// 为了持久化保存设置
#include "kvdb_ctrl.h"

static lv_obj_t * settings_cont = NULL;
extern volatile uint8_t kv_uac1_enable; // 引用UAC配置

// 定义列表项类型
typedef enum {
    ITEM_TYPE_BTN = 0,
    ITEM_TYPE_SWITCH
} item_type_t;

typedef struct {
    const char * item_name;
    item_type_t type;
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

// UAC兼容性开关回调
static void uac_switch_cb(lv_event_t * e) {
	lv_obj_t * sw = lv_event_get_target(e);
	kv_uac1_enable = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
    
    // 如您的工程中存在 KVDB，建议在此添加持久化保存: 
    // kvdb_persist_mark(KV_IDX_kv_uac1_enable);
}

static const setting_item_t settings_list[] = {
    {"输入设备监控",     ITEM_TYPE_BTN, key_test_click_cb},
    {"日志设置与显示",   ITEM_TYPE_BTN, log_ctrl_click_cb},
    {"关于本机信息",     ITEM_TYPE_BTN, about_click_cb},
	{"DAC设置",          ITEM_TYPE_BTN, es9018_click_cb},
	{"日期与时间",       ITEM_TYPE_BTN, time_set_click_cb},
	{"提升UAC兼容性",    ITEM_TYPE_SWITCH, uac_switch_cb}, // 新增：UAC1开关
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
    
    remove_default_style(settings_cont);
    lv_obj_set_style_bg_opa(settings_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_cont, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_scroll_dir(settings_cont, LV_DIR_VER);
    
    // 2. 遍历设置项数组生成UI
    int item_count = sizeof(settings_list) / sizeof(settings_list[0]);
    for (int i = 0; i < item_count; i++) {
        
        lv_obj_t * item_cont = lv_obj_create(settings_cont);
        lv_obj_set_size(item_cont, 240, 32); 
        lv_obj_set_pos(item_cont, 0, i * 32); 
        
        remove_default_style(item_cont);
        
        // 底部 1pix 灰色横线
        lv_obj_set_style_border_width(item_cont, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(item_cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(item_cont, lv_color_hex(0xE0E0E0), LV_PART_MAIN);

        // 创建中文文本
        lv_obj_t * text_label = lv_label_create(item_cont);
        lv_obj_set_style_text_font(text_label, &lv_font_16, LV_PART_MAIN);
        lv_label_set_text(text_label, settings_list[i].item_name);
        lv_obj_align(text_label, LV_ALIGN_LEFT_MID, 8, 0); 
        
        // 根据类型创建控件与交互
        if (settings_list[i].type == ITEM_TYPE_BTN) {
            // 类型：普通按键 -> 给予背景颜色反馈并绑定整个行的点击
            lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
            lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
            lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
            lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
            
            lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
            if (settings_list[i].click_cb != NULL) {
                lv_obj_add_event_cb(item_cont, settings_list[i].click_cb, LV_EVENT_CLICKED, (void *)settings_list[i].item_name);
            }
        } 
        else if (settings_list[i].type == ITEM_TYPE_SWITCH) {
            // 类型：开关控件 -> 行不可点击，依靠右侧Switch点击
            lv_obj_clear_flag(item_cont, LV_OBJ_FLAG_CLICKABLE); 
            
            lv_obj_t * sw = lv_switch_create(item_cont);
            lv_obj_set_size(sw, 40, 20);
            lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
            
            // 根据当前全局变量赋值初始状态
            if (kv_uac1_enable) {
                lv_obj_add_state(sw, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(sw, LV_STATE_CHECKED);
            }
            
            if (settings_list[i].click_cb != NULL) {
                lv_obj_add_event_cb(sw, settings_list[i].click_cb, LV_EVENT_VALUE_CHANGED, NULL);
            }
        }
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
