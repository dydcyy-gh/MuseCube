#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"

// 声明外部字体
LV_FONT_DECLARE(lv_font_16);

// 全局 UI 变量
static lv_obj_t * usb_ctrl_cont = NULL;
static lv_obj_t * label_status_val = NULL;
static lv_obj_t * btn_end_task = NULL;

// 记录上一次的状态，用于在 Update_USB_Unit 中判断是否需要刷新UI
static int last_usb_status = -1;
static uint8_t last_usb_function = 255; // 用255作为初始无效值，强制首次刷新

// 按键数组 (0-7 为从机模式，8-11 为主机模式)
static lv_obj_t * btns_slave[8];
static lv_obj_t * btns_host[4];

// 统一样式
static lv_style_t style_btn_base;
static lv_style_t style_btn_checked;
static lv_style_t style_btn_disabled;
static bool style_inited = false;

// 内部函数声明
static void update_buttons_state(void);
static void btn_event_cb(lv_event_t * e);
static void end_task_event_cb(lv_event_t * e);

// 按键配置结构体
typedef struct {
    const char * name;
    uint8_t function_val;
} usb_btn_cfg_t;

// 从机按键配置映射
static const usb_btn_cfg_t slave_btn_cfg[8] = {
    {"虚拟串口", USBD_CDC},
    {"虚拟U盘", USBD_MSC},
    {"解码耳放", USBD_UAC1}, // UAC1
    {"解码耳放", USBD_UAC2}, // UAC2
    {"电脑投屏", USBD_DISP},
    {"模拟手柄", USBD_GMPD},
    {"模拟键盘", USBD_KBD},
    {"模拟鼠标", USBD_MOU}
};

// 主机按键配置映射
static const usb_btn_cfg_t host_btn_cfg[4] = {
    {"串口调试", USBH_CDC},
    {"U盘读取", USBH_MSC},
    {"手柄输入", USBH_GMPD},
    {"键鼠输入", USBH_HID}
};

// 初始化通用 UI 样式
static void init_custom_styles(void)
{
    if (style_inited) return;

    /* 基础按钮样式 */
    lv_style_init(&style_btn_base);
    lv_style_set_shadow_width(&style_btn_base, 0);
    lv_style_set_border_width(&style_btn_base, 1);
    lv_style_set_border_color(&style_btn_base, lv_color_black());
    lv_style_set_bg_color(&style_btn_base, lv_color_hex(0xE0E0E0));
    lv_style_set_text_color(&style_btn_base, lv_color_black());
    lv_style_set_pad_all(&style_btn_base, 0);
    lv_style_set_radius(&style_btn_base, 4);

    /* 选中后样式 */
    lv_style_init(&style_btn_checked);
    lv_style_set_bg_color(&style_btn_checked, lv_color_hex(0x808080)); 
    lv_style_set_text_color(&style_btn_checked, lv_color_white());

    /* 禁用时样式 */
    lv_style_init(&style_btn_disabled);
    lv_style_set_bg_color(&style_btn_disabled, lv_color_hex(0xF0F0F0));
    lv_style_set_text_color(&style_btn_disabled, lv_color_hex(0xA0A0A0));
    lv_style_set_border_color(&style_btn_disabled, lv_color_hex(0xC0C0C0));

    style_inited = true;
}

void Create_USB_Unit(void)
{
    if (usb_ctrl_cont != NULL) return;

    init_custom_styles();

    /* ==========================================
       1. 创建主容器 (适配 240x180)
       ========================================== */
    usb_ctrl_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(usb_ctrl_cont, 240, 180);
    lv_obj_center(usb_ctrl_cont);
    
    lv_obj_clear_flag(usb_ctrl_cont, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_border_width(usb_ctrl_cont, 0, LV_PART_MAIN); 
    lv_obj_set_style_pad_all(usb_ctrl_cont, 0, LV_PART_MAIN);
    // 背景颜色改为白色
    lv_obj_set_style_bg_color(usb_ctrl_cont, lv_color_white(), LV_PART_MAIN);

    /* ==========================================
       2. 顶部状态栏
       ========================================== */
    // 修改 start_y 为 55，拉开第一排与下方按钮矩阵的间距
    int btn_w = 55, btn_h = 32, space_x = 3, space_y = 8, start_x = 5, start_y = 55; 
    
    // 结束任务按钮计算第四列起始横坐标: start_x + 3 * (btn_w + space_x) = 5 + 3 * 58 = 179
    int end_btn_x = start_x + 3 * (btn_w + space_x);
    
    btn_end_task = lv_btn_create(usb_ctrl_cont);
    lv_obj_set_size(btn_end_task, btn_w, btn_h); 
    lv_obj_add_style(btn_end_task, &style_btn_base, LV_PART_MAIN);
    lv_obj_align(btn_end_task, LV_ALIGN_TOP_LEFT, end_btn_x, 5);
    
    lv_obj_t * label_end = lv_label_create(btn_end_task);
    lv_label_set_text(label_end, "结束");
    // 文字向右偏移 1px
    lv_obj_align(label_end, LV_ALIGN_CENTER, 1, 0);
    lv_obj_add_event_cb(btn_end_task, end_task_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label_status_title = lv_label_create(usb_ctrl_cont);
    lv_label_set_text(label_status_title, "USB状态:");
    lv_obj_set_style_text_font(label_status_title, &lv_font_16, 0);
    lv_obj_align(label_status_title, LV_ALIGN_TOP_LEFT, start_x, 12);

    label_status_val = lv_label_create(usb_ctrl_cont);
    lv_label_set_text(label_status_val, "未知");
    lv_obj_set_style_text_font(label_status_val, &lv_font_16, 0);
    // 紧贴 title 的右侧，不留空隙
    lv_obj_align_to(label_status_val, label_status_title, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);

    /* ==========================================
       3. 创建功能按钮阵列
       ========================================== */

    // 前两排：从机模式功能 (8个)
    for (int i = 0; i < 8; i++) {
        int row = i / 4, col = i % 4;
        
        btns_slave[i] = lv_btn_create(usb_ctrl_cont);
        lv_obj_set_size(btns_slave[i], btn_w, btn_h);
        lv_obj_add_flag(btns_slave[i], LV_OBJ_FLAG_CHECKABLE); 
        lv_obj_add_style(btns_slave[i], &style_btn_base, LV_PART_MAIN);
        lv_obj_add_style(btns_slave[i], &style_btn_checked, LV_STATE_CHECKED);
        lv_obj_add_style(btns_slave[i], &style_btn_disabled, LV_STATE_DISABLED);
        lv_obj_align(btns_slave[i], LV_ALIGN_TOP_LEFT, start_x + col * (btn_w + space_x), start_y + row * (btn_h + space_y));
        
        lv_obj_t * label = lv_label_create(btns_slave[i]);
        lv_label_set_text(label, slave_btn_cfg[i].name);
        // 文字向右偏移 1px
        lv_obj_align(label, LV_ALIGN_CENTER, 1, 0);
        
        lv_obj_add_event_cb(btns_slave[i], btn_event_cb, LV_EVENT_CLICKED, (void *)&slave_btn_cfg[i]);
    }

    // 第三排：主机模式功能 (4个)
    for (int i = 0; i < 4; i++) {
        int row = 2, col = i;
        
        btns_host[i] = lv_btn_create(usb_ctrl_cont);
        lv_obj_set_size(btns_host[i], btn_w, btn_h);
        lv_obj_add_flag(btns_host[i], LV_OBJ_FLAG_CHECKABLE); 
        lv_obj_add_style(btns_host[i], &style_btn_base, LV_PART_MAIN);
        lv_obj_add_style(btns_host[i], &style_btn_checked, LV_STATE_CHECKED);
        lv_obj_add_style(btns_host[i], &style_btn_disabled, LV_STATE_DISABLED);
        lv_obj_align(btns_host[i], LV_ALIGN_TOP_LEFT, start_x + col * (btn_w + space_x), start_y + row * (btn_h + space_y));
        
        lv_obj_t * label = lv_label_create(btns_host[i]);
        lv_label_set_text(label, host_btn_cfg[i].name);
        // 文字向右偏移 1px
        lv_obj_align(label, LV_ALIGN_CENTER, 1, 0);
        
        lv_obj_add_event_cb(btns_host[i], btn_event_cb, LV_EVENT_CLICKED, (void *)&host_btn_cfg[i]);
    }

    // 强制初始化当前状态并刷新
    last_usb_status = g_usb_status;
    last_usb_function = g_usb_function;
    update_buttons_state();
}

void Update_USB_Unit(void)
{
    if (usb_ctrl_cont == NULL) return;

    // 任何一个状态发生了改变，就触发UI更新
    if (last_usb_status != g_usb_status || last_usb_function != g_usb_function) {
        last_usb_status = g_usb_status;
        last_usb_function = g_usb_function;
        update_buttons_state();
    }
}

void Remove_USB_Unit(void)
{
    if (usb_ctrl_cont != NULL) {
        lv_obj_del(usb_ctrl_cont);
        usb_ctrl_cont = NULL;
    }
}

// ---------------- 内部回调与状态更新实现 ----------------

// 集中处理所有状态同步（使能/禁用、选中高亮、文本更新）
static void update_buttons_state(void)
{
    bool is_slave = (g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY);
    bool is_host  = (g_usb_status == TYPEC_CC_HOST || g_usb_status == TYPEC_IS_HOST);

    /* 1. 更新按钮的使能/禁用状态 */
    if (is_slave) {
        for (int i = 0; i < 8; i++) lv_obj_clear_state(btns_slave[i], LV_STATE_DISABLED);
        for (int i = 0; i < 4; i++) lv_obj_add_state(btns_host[i], LV_STATE_DISABLED);
    } else if (is_host) {
        for (int i = 0; i < 8; i++) lv_obj_add_state(btns_slave[i], LV_STATE_DISABLED);
        for (int i = 0; i < 4; i++) lv_obj_clear_state(btns_host[i], LV_STATE_DISABLED);
    } else {
        for (int i = 0; i < 8; i++) lv_obj_add_state(btns_slave[i], LV_STATE_DISABLED);
        for (int i = 0; i < 4; i++) lv_obj_add_state(btns_host[i], LV_STATE_DISABLED);
    }

    /* 2. 清除所有按钮的选中状态 */
    for (int i = 0; i < 8; i++) lv_obj_clear_state(btns_slave[i], LV_STATE_CHECKED);
    for (int i = 0; i < 4; i++) lv_obj_clear_state(btns_host[i], LV_STATE_CHECKED);

    /* 3. 根据全局变量 g_usb_function 设置选中高亮并更新文本 */
    if (g_usb_function == USB_NONE) {
        if (is_slave) {
            lv_label_set_text(label_status_val, "仅充电");
        } else if (is_host) {
            lv_label_set_text(label_status_val, "仅供电");
        } else {
            if (g_usb_status == TYPEC_CC_IDLE) {
                lv_label_set_text(label_status_val, "C-C线缆接入");
            } else if (g_usb_status == TYPEC_AC_IDLE) {
                lv_label_set_text(label_status_val, "A-C线缆接入");
            } else {
                lv_label_set_text(label_status_val, "未连接");
            }
        }
    } else {
        bool found = false;
        
        // 遍历寻找当前功能对应的按键，开启高亮并提取名称
        if (is_slave) {
            for (int i = 0; i < 8; i++) {
                if (slave_btn_cfg[i].function_val == g_usb_function) {
                    lv_obj_add_state(btns_slave[i], LV_STATE_CHECKED);
                    lv_label_set_text(label_status_val, slave_btn_cfg[i].name);
                    found = true;
                    break;
                }
            }
        } else if (is_host) {
            for (int i = 0; i < 4; i++) {
                if (host_btn_cfg[i].function_val == g_usb_function) {
                    lv_obj_add_state(btns_host[i], LV_STATE_CHECKED);
                    lv_label_set_text(label_status_val, host_btn_cfg[i].name);
                    found = true;
                    break;
                }
            }
        }

        // 异常处理：当前功能宏与所在的硬件状态不匹配（如在主机模式下强行改成了从机宏）
        if (!found) {
            lv_label_set_text(label_status_val, "状态异常");
        }
    }
}

// 功能按钮点击事件
static void btn_event_cb(lv_event_t * e)
{
    // 提取保存在 user_data 中的配置信息
    usb_btn_cfg_t * cfg = (usb_btn_cfg_t *)lv_event_get_user_data(e);

    // 更新全局变量
    g_usb_function = cfg->function_val;
    last_usb_function = g_usb_function; // 同步缓存，防止重复刷新
    
    // 调用统一状态机刷新 UI
    update_buttons_state();
    
    // TODO: 可以在此处添加底层硬件切换代码，或通过其他任务监控 g_usb_function 来执行
}

// 结束任务按钮点击事件
static void end_task_event_cb(lv_event_t * e)
{
    // 重置全局变量
    g_usb_function = USB_NONE;
    last_usb_function = g_usb_function;
    
    // 调用统一状态机恢复默认状态
    update_buttons_state();
    
    // TODO: 在此处添加底层停止任务的代码
}
