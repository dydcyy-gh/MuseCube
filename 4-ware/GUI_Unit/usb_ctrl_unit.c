#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "malloc.h"
#include "keyboard.h"

LV_FONT_DECLARE(lv_font_16);

// 外部引用新增的UAC配置变量
extern volatile uint8_t kv_uac1_enable;

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * usb_ctrl_cont;
	
	lv_obj_t * btn_nav_up;        // 向上导航按钮
	lv_obj_t * roller_func;       // 中间功能滚轮 (只读三行显示)
	lv_obj_t * btn_nav_down;      // 向下导航按钮
	
	lv_obj_t * cont_status_card;  // 右上方状态卡片
	lv_obj_t * label_status_val;  // 状态卡片内的状态文本

	lv_obj_t * btn_start_task;    // 启动按钮
	lv_obj_t * btn_end_task;      // 结束按钮

	int last_usb_status;
	uint8_t last_usb_function;

	// UI 样式
	lv_style_t style_btn_nav;
	lv_style_t style_btn_start;
	lv_style_t style_btn_end;
	lv_style_t style_btn_disabled;
} usb_state_t;

static usb_state_t *us = NULL;

// 内部函数声明
static void update_controls_state(void);
static void btn_nav_up_cb(lv_event_t * e);
static void btn_nav_down_cb(lv_event_t * e);
static void start_task_event_cb(lv_event_t * e);
static void end_task_event_cb(lv_event_t * e);

// 按键配置结构体 (配合 Roller 索引)
typedef struct {
	uint8_t is_host;       // 0代表从机功能，1代表主机功能
	uint8_t function_val;  // 对应的启动宏，解码耳放设为0xFF特殊处理
} usb_func_t;

// 保证顺序与 Roller 中字符串的顺序严格一致！共 8 项 (索引 0-7)
static const usb_func_t func_list[8] = {
	{0, USBD_MSC},  // 0: 虚拟U盘
	{0, 0xFF},      // 1: 解码耳放
	{0, USBD_DISP}, // 2: 电脑副屏
	{0, USBD_GMPD}, // 3: 模拟手柄
	{0, USBD_KBD},  // 4: 模拟键盘
	{0, USBD_MOU},  // 5: 模拟鼠标
	{1, USBH_MSC},  // 6: U盘读取
	{1, USBH_HID}   // 7: 外设输入
};

// 获取完整USB状态描述
static const char *get_status_string(uint8_t status, uint8_t function) {
	if (function != USB_NONE) {
		switch(function) {
			case USBD_LOG:  return "日志输出";
			case USBD_CMD:  return "CMD调试";
			case USBD_MSC:  return "虚拟U盘";
			case USBD_UAC1: return "解码耳放";
			case USBD_UAC2: return "解码耳放";
			case USBD_DISP: return "电脑副屏";
			case USBD_GMPD: return "模拟手柄";
			case USBD_KBD:  return "模拟键盘";
			case USBD_MOU:  return "模拟鼠标";
			case USBH_MSC:  return "U盘读取";
			case USBH_HID:  return "外设输入";
			case USBH_CDC:  return "串口助手";
			default: return "未知功能";
		}
	} else {
		switch(status) {
			case TYPEC_AC_OKEY:
			case TYPEC_CC_OKEY: return "仅充电";
			case TYPEC_IS_HOST:
			case TYPEC_CC_HOST: return "仅供电";
			case TYPEC_CC_IDLE: return "C-C线缆空闲";
			case TYPEC_AC_IDLE: return "A-C线缆空闲";
			case TYPEC_NO_FIND: 
			default: return "未检测到连接";
		}
	}
}

// 初始化高级 UI 样式
static void init_custom_styles(void)
{
	// 导航按钮样式 (浅灰圆角底色)
	lv_style_init(&us->style_btn_nav);
	lv_style_set_radius(&us->style_btn_nav, 8);
	lv_style_set_bg_color(&us->style_btn_nav, lv_color_hex(0xE0E0E0));
	lv_style_set_text_color(&us->style_btn_nav, lv_color_hex(0x333333));
	lv_style_set_border_width(&us->style_btn_nav, 0);
	lv_style_set_shadow_width(&us->style_btn_nav, 0);

	// 启动按钮样式 (淡蓝色)
	lv_style_init(&us->style_btn_start);
	lv_style_set_radius(&us->style_btn_start, 8);
	lv_style_set_bg_color(&us->style_btn_start, lv_color_hex(0x74C0FC)); 
	lv_style_set_text_color(&us->style_btn_start, lv_color_white());
	lv_style_set_border_width(&us->style_btn_start, 0);
	lv_style_set_shadow_width(&us->style_btn_start, 0);

	// 结束按钮样式 (淡红色)
	lv_style_init(&us->style_btn_end);
	lv_style_set_radius(&us->style_btn_end, 8);
	lv_style_set_bg_color(&us->style_btn_end, lv_color_hex(0xFF8A80));
	lv_style_set_text_color(&us->style_btn_end, lv_color_white());
	lv_style_set_border_width(&us->style_btn_end, 0);
	lv_style_set_shadow_width(&us->style_btn_end, 0);

	// 禁用按钮样式 (灰白淡化)
	lv_style_init(&us->style_btn_disabled);
	lv_style_set_bg_color(&us->style_btn_disabled, lv_color_hex(0xE0E0E0));
	lv_style_set_text_color(&us->style_btn_disabled, lv_color_hex(0xA0A0A0));
}

void Create_USB_Unit(void)
{
	if (us != NULL) return;

	us = (usb_state_t *)malloc_ccm(sizeof(usb_state_t));
	if (!us) return;
	memset(us, 0, sizeof(usb_state_t));

	us->last_usb_status = -1;
	us->last_usb_function = 255;

	init_custom_styles();

	// 1. 主板容器 (240x180)
	us->usb_ctrl_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(us->usb_ctrl_cont, 240, 180);
	lv_obj_center(us->usb_ctrl_cont);
	lv_obj_clear_flag(us->usb_ctrl_cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(us->usb_ctrl_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(us->usb_ctrl_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(us->usb_ctrl_cont, lv_color_hex(0xF0F2F5), LV_PART_MAIN);

	// ================= 左侧区域：导航按键 + 核心显示拨轮 =================
	
	// 向上导航按钮 (高度恢复30，贴顶)
	us->btn_nav_up = lv_btn_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->btn_nav_up, 100, 30);
	lv_obj_align(us->btn_nav_up, LV_ALIGN_TOP_LEFT, 10, 10);
	lv_obj_add_style(us->btn_nav_up, &us->style_btn_nav, LV_PART_MAIN);
	lv_obj_add_event_cb(us->btn_nav_up, btn_nav_up_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * label_up = lv_label_create(us->btn_nav_up);
	lv_label_set_text(label_up, "上一个");
	lv_obj_set_style_text_font(label_up, &lv_font_16, 0);
	lv_obj_align(label_up, LV_ALIGN_CENTER, 0, 0);

	// 中间滚轮组件
	us->roller_func = lv_roller_create(us->usb_ctrl_cont);
	lv_roller_set_options(us->roller_func, 
		"虚拟 U 盘\n"
		"解码耳放\n"
		"电脑副屏\n"
		"模拟手柄\n"
		"模拟键盘\n"
		"模拟鼠标\n"
		"U 盘读取\n"
		"外设输入",
		LV_ROLLER_MODE_NORMAL);
	
	// 【重点修改】：完全摒弃 visible_row_count，直接锁死高度为 90pix，宽度为 100pix
	lv_obj_set_size(us->roller_func, 100, 90);
	lv_obj_align(us->roller_func, LV_ALIGN_LEFT_MID, 10, 0); // 在左侧完美垂直居中 (上下各留出 5pix 的距离)
	lv_obj_set_style_text_line_space(us->roller_func, 10, LV_PART_MAIN); // 把行距稍微加大，让三行恰好填满 90pix
	
	// 禁用触摸，将其变为纯显示组件，依靠上下按钮切换
	lv_obj_clear_flag(us->roller_func, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(us->roller_func, LV_OBJ_FLAG_SCROLLABLE);
	
	// 滚轮美化
	lv_obj_set_style_bg_color(us->roller_func, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_border_width(us->roller_func, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(us->roller_func, 8, LV_PART_MAIN);
	lv_obj_set_style_text_font(us->roller_func, &lv_font_16, LV_PART_MAIN);
	
	// 选中项美化 (淡蓝选定框)
	lv_obj_set_style_bg_color(us->roller_func, lv_color_hex(0x74C0FC), LV_PART_SELECTED);
	lv_obj_set_style_text_color(us->roller_func, lv_color_white(), LV_PART_SELECTED);
	lv_obj_set_style_text_font(us->roller_func, &lv_font_16, LV_PART_SELECTED);

	// 向下导航按钮 (高度恢复30，贴底)
	us->btn_nav_down = lv_btn_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->btn_nav_down, 100, 30);
	lv_obj_align(us->btn_nav_down, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	lv_obj_add_style(us->btn_nav_down, &us->style_btn_nav, LV_PART_MAIN);
	lv_obj_add_event_cb(us->btn_nav_down, btn_nav_down_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * label_down = lv_label_create(us->btn_nav_down);
	lv_label_set_text(label_down, "下一个");
	lv_obj_set_style_text_font(label_down, &lv_font_16, 0);
	lv_obj_align(label_down, LV_ALIGN_CENTER, 0, 0);


	// ================= 右侧区域：状态仪与控制 =================
	
	// 状态仪表卡片
	us->cont_status_card = lv_obj_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->cont_status_card, 110, 60);
	lv_obj_align(us->cont_status_card, LV_ALIGN_TOP_RIGHT, -10, 10);
	lv_obj_clear_flag(us->cont_status_card, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_radius(us->cont_status_card, 10, LV_PART_MAIN);
	lv_obj_set_style_border_width(us->cont_status_card, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(us->cont_status_card, lv_color_hex(0x333333), LV_PART_MAIN); // 深灰背景
	lv_obj_set_style_pad_all(us->cont_status_card, 5, LV_PART_MAIN);

	lv_obj_t * label_title = lv_label_create(us->cont_status_card);
	lv_label_set_text(label_title, "运行状态:");
	lv_obj_set_style_text_font(label_title, &lv_font_16, 0);
	lv_obj_set_style_text_color(label_title, lv_color_hex(0xAAAAAA), 0);
	lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 0, 0);

	us->label_status_val = lv_label_create(us->cont_status_card);
	lv_obj_set_width(us->label_status_val, 100);
	lv_label_set_long_mode(us->label_status_val, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_obj_set_style_text_font(us->label_status_val, &lv_font_16, 0);
	lv_obj_set_style_text_color(us->label_status_val, lv_color_hex(0x00E676), 0); // 荧光绿
	lv_label_set_text(us->label_status_val, "未连接");
	lv_obj_align(us->label_status_val, LV_ALIGN_BOTTOM_LEFT, 0, -2);


	// 启动按钮
	us->btn_start_task = lv_btn_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->btn_start_task, 110, 40);
	lv_obj_align(us->btn_start_task, LV_ALIGN_TOP_RIGHT, -10, 80);
	lv_obj_add_style(us->btn_start_task, &us->style_btn_start, LV_PART_MAIN);
	lv_obj_add_style(us->btn_start_task, &us->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_add_event_cb(us->btn_start_task, start_task_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * label_start = lv_label_create(us->btn_start_task);
	lv_label_set_text(label_start, "启 动 功 能");
	lv_obj_set_style_text_font(label_start, &lv_font_16, 0);
	lv_obj_align(label_start, LV_ALIGN_CENTER, 0, 0);


	// 结束按钮
	us->btn_end_task = lv_btn_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->btn_end_task, 110, 40);
	lv_obj_align(us->btn_end_task, LV_ALIGN_TOP_RIGHT, -10, 130);
	lv_obj_add_style(us->btn_end_task, &us->style_btn_end, LV_PART_MAIN);
	lv_obj_add_event_cb(us->btn_end_task, end_task_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * label_end = lv_label_create(us->btn_end_task);
	lv_label_set_text(label_end, "终 止 连 接");
	lv_obj_set_style_text_font(label_end, &lv_font_16, 0);
	lv_obj_align(label_end, LV_ALIGN_CENTER, 0, 0);

	us->last_usb_status = g_usb_status;
	us->last_usb_function = g_usb_function;
	
	// 初始化时强制更新一次
	update_controls_state();
}

void Update_USB_Unit(void)
{
	if (us == NULL) return;

	Update_Keyboard();
	
	bool is_slave = (g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY);
	bool is_host  = (g_usb_status == TYPEC_CC_HOST || g_usb_status == TYPEC_IS_HOST);
	
	// 断开线缆时强制清零功能
	if (!is_slave && !is_host && g_usb_function != USB_NONE) {
		g_usb_function = USB_NONE;
		Remove_Keyboard();
	}

	if (us->last_usb_status != g_usb_status || us->last_usb_function != g_usb_function) {
		us->last_usb_status = g_usb_status;
		us->last_usb_function = g_usb_function;
		update_controls_state();
	}
}

void Remove_USB_Unit(void)
{
	if (us == NULL) return;

	if (us->usb_ctrl_cont != NULL) {
		lv_obj_del(us->usb_ctrl_cont);
	}

	free_ccm(us);
	us = NULL;
}

// ======================= 核心交互逻辑 =======================

// 统一更新按钮可用性与状态仪表盘
static void update_controls_state(void)
{
	// 1. 刷新状态指示器文本
	lv_label_set_text(us->label_status_val, get_status_string(g_usb_status, g_usb_function));

	// 2. 检查启动按钮合法性
	uint16_t current_idx = lv_roller_get_selected(us->roller_func);
	bool is_slave = (g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY);
	bool is_host  = (g_usb_status == TYPEC_CC_HOST || g_usb_status == TYPEC_IS_HOST);
	
	bool can_start = false;

	if (current_idx < 8) {
		if (is_slave && func_list[current_idx].is_host == 0) can_start = true;
		if (is_host  && func_list[current_idx].is_host == 1) can_start = true;
	}

	if (can_start) {
		lv_obj_clear_state(us->btn_start_task, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(us->btn_start_task, LV_STATE_DISABLED);
	}
}

// 左侧 向上导航按钮 回调
static void btn_nav_up_cb(lv_event_t * e)
{
	uint16_t idx = lv_roller_get_selected(us->roller_func);
	if (idx > 0) {
		idx--;
		lv_roller_set_selected(us->roller_func, idx, LV_ANIM_ON);
		update_controls_state(); // 手动触发状态更新验证可用性
	}
}

// 左侧 向下导航按钮 回调
static void btn_nav_down_cb(lv_event_t * e)
{
	uint16_t idx = lv_roller_get_selected(us->roller_func);
	if (idx < 7) { // 共有 8 项，最大索引 7
		idx++;
		lv_roller_set_selected(us->roller_func, idx, LV_ANIM_ON);
		update_controls_state();
	}
}

// 启动功能按钮回调
static void start_task_event_cb(lv_event_t * e)
{
	uint16_t idx = lv_roller_get_selected(us->roller_func);
	if (idx >= 8) return;

	uint8_t target_func = func_list[idx].function_val;

	// 解码耳放特殊判断 (依赖外置设置宏)
	if (target_func == 0xFF) {
		target_func = kv_uac1_enable ? USBD_UAC1 : USBD_UAC2;
	}

	g_usb_function = target_func;
	us->last_usb_function = g_usb_function;
	
	// KBD 开启虚拟键盘
	if (g_usb_function == USBD_KBD) {
		Create_Keyboard_EN(NULL);
	} else {
		Remove_Keyboard();
	}

	update_controls_state();
}

// 结束功能按钮回调
static void end_task_event_cb(lv_event_t * e)
{
	g_usb_function = USB_NONE;
	us->last_usb_function = g_usb_function;
	
	Remove_Keyboard(); // 释放在 USB 键盘模式可能分配的虚拟键盘组件
	
	update_controls_state();
}
