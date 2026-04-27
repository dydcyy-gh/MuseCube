#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "keyboard.h"

// ==================== 全局 UI 变量 ====================
static lv_obj_t * note_cont = NULL;
static lv_obj_t * ta_note = NULL;

// 统一样式（这里只给文本框用一个简单样式）
static lv_style_t style_note_bg;
static bool style_inited = false;

// 内部函数声明
static void note_ta_event_cb(lv_event_t * e);

// 初始化样式（和 usb_ctrl_unit 结构保持一致）
static void init_custom_styles(void)
{
    if (style_inited) return;

    lv_style_init(&style_note_bg);
    lv_style_set_radius(&style_note_bg, 4);
    lv_style_set_border_width(&style_note_bg, 1);
    lv_style_set_border_color(&style_note_bg, lv_color_black());
    lv_style_set_bg_color(&style_note_bg, lv_color_white());
    lv_style_set_bg_opa(&style_note_bg, LV_OPA_100);
    lv_style_set_pad_all(&style_note_bg, 8);
    lv_style_set_text_color(&style_note_bg, lv_color_black());

    style_inited = true;
}

// ==================== 创建 Note Unit（240×180） ====================
void Create_Note_Unit(void)
{
    if (note_cont != NULL) return;

    init_custom_styles();

    /* ==========================================
       1. 创建主容器 (和 USB_Unit 完全一致尺寸)
       ========================================== */
    note_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(note_cont, 240, 180);
    lv_obj_center(note_cont);
    
    lv_obj_clear_flag(note_cont, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_border_width(note_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_pad_all(note_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(note_cont, lv_color_hex(0xE8E8E8), LV_PART_MAIN);

    /* ==========================================
       2. 只创建一个大文本框（用于测试键盘）
       ========================================== */
    ta_note = lv_textarea_create(note_cont);
    lv_obj_set_size(ta_note, 220, 160);                 // 留出边距，居中显示
    lv_obj_align(ta_note, LV_ALIGN_CENTER, 0, 0);
    
    lv_textarea_set_placeholder_text(ta_note, "在这里输入笔记（支持英文+数字）...");
    lv_textarea_set_one_line(ta_note, false);           // 多行模式，更适合记笔记
    lv_textarea_set_max_length(ta_note, 1024);          // 可输入较多内容
    lv_textarea_set_cursor_click_pos(ta_note, true);
    
    /* 应用样式 + 绑定键盘事件 */
    lv_obj_add_style(ta_note, &style_note_bg, LV_PART_MAIN);
    lv_obj_add_event_cb(ta_note, note_ta_event_cb, LV_EVENT_ALL, NULL);

    // 初始时可自动聚焦（方便测试）
    lv_obj_add_state(ta_note, LV_STATE_FOCUSED);
}

// ==================== 更新（留空，后面可扩展实时刷新） ====================
void Update_Note_Unit(void)
{
    if (note_cont == NULL) return;
    // 如果以后需要同步全局变量到文本框，在这里写
}

// ==================== 删除 ====================
void Remove_Note_Unit(void)
{
    if (note_cont != NULL) {
        lv_obj_del(note_cont);
        note_cont = NULL;
        ta_note = NULL;
    }
}

// ---------------- 内部回调 ----------------
// 文本框聚焦时自动弹出我们之前做的 240×120 键盘
static void note_ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        Create_Keyboard(ta);          // 只保留这一行
    }
}
