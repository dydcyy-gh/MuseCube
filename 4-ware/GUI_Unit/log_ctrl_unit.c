#include "log_ctrl_unit.h"
#include "lvgl.h"
#include "debug.h"
#include "variables.h"
#include "page_manager.h"

// 内部静态变量记录选中的模式，默认一个都不选 (None)
static uint8_t selected_active_mode = Debug_Mode_None;

// 全局 UI 变量
static lv_obj_t * ctrl_cont = NULL;
static lv_obj_t * sw_debug = NULL;
static lv_obj_t * btn_mode_tsdb = NULL;
static lv_obj_t * btn_mode_usb = NULL;
static lv_obj_t * btn_mode_lvgl = NULL;
static lv_obj_t * btn_dump_usb = NULL;
static lv_obj_t * btn_dump_lvgl = NULL;

// 统一样式
static lv_style_t style_btn_base;
static lv_style_t style_btn_checked;
static lv_style_t style_btn_disabled;
static bool style_inited = false;

// 内部函数声明
static void update_all_buttons_state(void);
static void update_mode_buttons_ui(void);
static void sw_event_cb(lv_event_t * e);
static void mode_btn_event_cb(lv_event_t * e);
static void dump_usb_event_cb(lv_event_t * e);
static void dump_lvgl_event_cb(lv_event_t * e);

// 初始化通用 UI 样式
static void init_custom_styles(void)
{
    if (style_inited) return;

    /* 基础按钮样式 (非白色、去阴影、1px黑边框) */
    lv_style_init(&style_btn_base);
    lv_style_set_shadow_width(&style_btn_base, 0);                 // 去除阴影
    lv_style_set_border_width(&style_btn_base, 1);                 // 1像素边框
    lv_style_set_border_color(&style_btn_base, lv_color_black());  // 黑色边框
    lv_style_set_bg_color(&style_btn_base, lv_color_hex(0xE0E0E0));// 浅灰背景
    lv_style_set_text_color(&style_btn_base, lv_color_black());    // 黑色文字
    lv_style_set_pad_all(&style_btn_base, 0);                      // 消除内边距
    lv_style_set_radius(&style_btn_base, 6);                       // 圆角为 5

    /* 选中后样式 (深灰色) */
    lv_style_init(&style_btn_checked);
    lv_style_set_bg_color(&style_btn_checked, lv_color_hex(0x808080)); 
    lv_style_set_text_color(&style_btn_checked, lv_color_white()); // 选中后白字更清晰

    /* 禁用时样式 (更浅的灰色，表明不可点) */
    lv_style_init(&style_btn_disabled);
    lv_style_set_bg_color(&style_btn_disabled, lv_color_hex(0xF0F0F0));
    lv_style_set_text_color(&style_btn_disabled, lv_color_hex(0xA0A0A0));
    lv_style_set_border_color(&style_btn_disabled, lv_color_hex(0xC0C0C0));

    style_inited = true;
}

void Create_Log_Control(void)
{
    if (ctrl_cont != NULL) return;

    init_custom_styles();

    if (debug_mode != Debug_Mode_None) {
        selected_active_mode = debug_mode;
    } else {
        selected_active_mode = Debug_Mode_None;
    }

    /* ==========================================
       1. 创建主容器 (240x180)
       ========================================== */
    ctrl_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ctrl_cont, 240, 180);
    lv_obj_center(ctrl_cont);
    
    // 禁用滚动条，去除边框，去除默认的内边距
    lv_obj_clear_flag(ctrl_cont, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_border_width(ctrl_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_pad_all(ctrl_cont, 0, LV_PART_MAIN);

    /* ==========================================
       2. 第一行：开启调试功能 (Y = 10)
       排版：左标签 X=20，右开关宽度60 X=160，总宽 = 200，对齐右边界 220
       ========================================== */
    lv_obj_t * label_sw = lv_label_create(ctrl_cont);
    lv_label_set_text(label_sw, "开启调试功能:");
    lv_obj_set_style_text_font(label_sw, &lv_font_16, LV_PART_MAIN); // 应用16号字体
    lv_obj_align(label_sw, LV_ALIGN_TOP_LEFT, 20, 15); // 配合开关高度微调Y轴

    sw_debug = lv_switch_create(ctrl_cont);
    lv_obj_set_size(sw_debug, 60, 25); // 大小修改为 60x25
    lv_obj_align(sw_debug, LV_ALIGN_TOP_LEFT, 160, 10); // 修改坐标保证右边缘与第三个按钮对齐
    
    // 去除开关各部件的阴影，并统一设置圆角为 4
    lv_obj_set_style_shadow_width(sw_debug, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sw_debug, 4, LV_PART_MAIN);

    lv_obj_set_style_shadow_width(sw_debug, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sw_debug, 4, LV_PART_INDICATOR);

    lv_obj_set_style_shadow_width(sw_debug, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(sw_debug, 4, LV_PART_KNOB);
    
    if (debug_mode != Debug_Mode_None) {
        lv_obj_add_state(sw_debug, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw_debug, sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ==========================================
       3. 第二行：三种模式选择 (标题 Y = 60, 按钮 Y = 80)
       ========================================== */
    // 实时日志输出方式 - 标签
    lv_obj_t * label_rt_mode = lv_label_create(ctrl_cont);
    lv_label_set_text(label_rt_mode, "实时日志输出方式");
    lv_obj_align(label_rt_mode, LV_ALIGN_TOP_LEFT, 20, 60);

    // TSDB 按钮
    btn_mode_tsdb = lv_btn_create(ctrl_cont);
    lv_obj_set_size(btn_mode_tsdb, 60, 25); // 高度改为 25
    lv_obj_add_flag(btn_mode_tsdb, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_mode_tsdb, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_mode_tsdb, &style_btn_checked, LV_STATE_CHECKED);
    lv_obj_add_style(btn_mode_tsdb, &style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_align(btn_mode_tsdb, LV_ALIGN_TOP_LEFT, 20, 80);
    lv_obj_t * label_tsdb = lv_label_create(btn_mode_tsdb);
    lv_label_set_text(label_tsdb, "TSDB");
    lv_obj_center(label_tsdb);
    lv_obj_add_event_cb(btn_mode_tsdb, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_TSDB);

    // USB 按钮
    btn_mode_usb = lv_btn_create(ctrl_cont);
    lv_obj_set_size(btn_mode_usb, 60, 25); // 高度改为 25
    lv_obj_add_flag(btn_mode_usb, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_mode_usb, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_mode_usb, &style_btn_checked, LV_STATE_CHECKED);
    lv_obj_add_style(btn_mode_usb, &style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_align(btn_mode_usb, LV_ALIGN_TOP_LEFT, 90, 80);
    lv_obj_t * label_usb_mode = lv_label_create(btn_mode_usb);
    lv_label_set_text(label_usb_mode, "USB");
    lv_obj_center(label_usb_mode);
    lv_obj_add_event_cb(btn_mode_usb, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_USBD);

    // LVGL 按钮
    btn_mode_lvgl = lv_btn_create(ctrl_cont);
    lv_obj_set_size(btn_mode_lvgl, 60, 25); // 高度改为 25
    lv_obj_add_flag(btn_mode_lvgl, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_mode_lvgl, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_mode_lvgl, &style_btn_checked, LV_STATE_CHECKED);
    lv_obj_add_style(btn_mode_lvgl, &style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_align(btn_mode_lvgl, LV_ALIGN_TOP_LEFT, 160, 80);
    lv_obj_t * label_lvgl_mode = lv_label_create(btn_mode_lvgl);
    lv_label_set_text(label_lvgl_mode, "LVGL");
    lv_obj_center(label_lvgl_mode);
    lv_obj_add_event_cb(btn_mode_lvgl, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_LVGL);

    update_mode_buttons_ui(); // 刷新选中状态

    /* ==========================================
       4. 第三行：导出按钮 (标题 Y = 120, 按钮 Y = 140)
       ========================================== */
    // 储存日志输出方式 - 标签
    lv_obj_t * label_dump_mode = lv_label_create(ctrl_cont);
    lv_label_set_text(label_dump_mode, "储存日志输出方式");
    lv_obj_align(label_dump_mode, LV_ALIGN_TOP_LEFT, 20, 120);

    // Dump USB 
    btn_dump_usb = lv_btn_create(ctrl_cont);
    lv_obj_set_size(btn_dump_usb, 95, 25); // 高度改为 25
    lv_obj_add_style(btn_dump_usb, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_dump_usb, &style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_align(btn_dump_usb, LV_ALIGN_TOP_LEFT, 20, 140);
    lv_obj_t * label_dump_usb = lv_label_create(btn_dump_usb);
    lv_label_set_text(label_dump_usb, "Dump USB");
    lv_obj_center(label_dump_usb);
    lv_obj_add_event_cb(btn_dump_usb, dump_usb_event_cb, LV_EVENT_CLICKED, NULL);

    // Dump LVGL 
    btn_dump_lvgl = lv_btn_create(ctrl_cont);
    lv_obj_set_size(btn_dump_lvgl, 95, 25); // 高度改为 25
    lv_obj_add_style(btn_dump_lvgl, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_dump_lvgl, &style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_align(btn_dump_lvgl, LV_ALIGN_TOP_LEFT, 125, 140); // 精确对齐右边界 220
    lv_obj_t * label_dump_lvgl = lv_label_create(btn_dump_lvgl);
    lv_label_set_text(label_dump_lvgl, "Dump LVGL");
    lv_obj_center(label_dump_lvgl);
    lv_obj_add_event_cb(btn_dump_lvgl, dump_lvgl_event_cb, LV_EVENT_CLICKED, NULL);

    // 初始化所有按钮状态
    update_all_buttons_state();
}

void Update_Log_Control(void)
{
	
}

void Remove_Log_Control(void)
{
    if (ctrl_cont != NULL) {
        lv_obj_del(ctrl_cont);
        ctrl_cont = NULL;
        sw_debug = NULL;
        btn_mode_tsdb = NULL;
        btn_mode_usb = NULL;
        btn_mode_lvgl = NULL;
        btn_dump_usb = NULL;
        btn_dump_lvgl = NULL;
    }
}

// ---------------- 内部回调与状态更新实现 ----------------

// 根据主开关状态，联动更新两排按钮的使能(Enabled/Disabled)状态
static void update_all_buttons_state(void)
{
    // 如果主开关关闭，或者是初始化时并未选中任何模式
    if (!lv_obj_has_state(sw_debug, LV_STATE_CHECKED)) {
        // 【未开启Debug】允许Dump，禁止选择输出模式
        lv_obj_clear_state(btn_dump_usb, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_dump_lvgl, LV_STATE_DISABLED);

        lv_obj_add_state(btn_mode_tsdb, LV_STATE_DISABLED);
        lv_obj_add_state(btn_mode_usb, LV_STATE_DISABLED);
        lv_obj_add_state(btn_mode_lvgl, LV_STATE_DISABLED);
    } else {
        // 【已开启Debug】禁止Dump，允许选择输出模式
        lv_obj_add_state(btn_dump_usb, LV_STATE_DISABLED);
        lv_obj_add_state(btn_dump_lvgl, LV_STATE_DISABLED);

        lv_obj_clear_state(btn_mode_tsdb, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_mode_usb, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_mode_lvgl, LV_STATE_DISABLED);
    }
}

// 刷新中间三个模式按钮的 UI 选中状态
static void update_mode_buttons_ui(void)
{
    lv_obj_clear_state(btn_mode_tsdb, LV_STATE_CHECKED);
    lv_obj_clear_state(btn_mode_usb, LV_STATE_CHECKED);
    lv_obj_clear_state(btn_mode_lvgl, LV_STATE_CHECKED);

    if (selected_active_mode == Debug_Mode_TSDB) {
        lv_obj_add_state(btn_mode_tsdb, LV_STATE_CHECKED);
    } else if (selected_active_mode == Debug_Mode_USBD) {
        lv_obj_add_state(btn_mode_usb, LV_STATE_CHECKED);
    } else if (selected_active_mode == Debug_Mode_LVGL) {
        lv_obj_add_state(btn_mode_lvgl, LV_STATE_CHECKED);
    }
}

// 第一行：开关切换事件
static void sw_event_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        // 刚打开时，默认一个都不选
        selected_active_mode = Debug_Mode_None;
        debug_mode = Debug_Mode_None;
    } else {
        // 关闭时，清空状态并应用到全局
        selected_active_mode = Debug_Mode_None;
        debug_mode = Debug_Mode_None;
    }
    
    update_all_buttons_state();
    update_mode_buttons_ui();
}

// 第二行：模式按钮点击事件
static void mode_btn_event_cb(lv_event_t * e)
{
    uint8_t mode_val = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    selected_active_mode = mode_val;
    
    update_mode_buttons_ui();

    // 因为这排按钮只能在开关打开时点击，所以直接赋值给全局变量
    if (lv_obj_has_state(sw_debug, LV_STATE_CHECKED)) {
        debug_mode = selected_active_mode;
        
        // 优化体验：如果开启了实时 LVGL 模式，自动切换到显示页面
        if (debug_mode == Debug_Mode_LVGL) {
            Page_Request_Switch(PAGE_DEBUG);
        }
    }
}

// 第三行：点击导出到 USB
static void dump_usb_event_cb(lv_event_t * e)
{
    Debug_Dump_TSDB_To_USB(50);
}

// 第三行：点击导出到 LVGL
extern volatile int tsdb_lvgl_need_dump;

static void dump_lvgl_event_cb(lv_event_t * e)
{
    tsdb_lvgl_need_dump = 1;
    Page_Request_Switch(PAGE_DEBUG); // 切换页面
}
