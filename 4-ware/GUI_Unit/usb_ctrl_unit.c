#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "malloc.h"

LV_FONT_DECLARE(lv_font_16);

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * usb_ctrl_cont;
	lv_obj_t * label_status_val;
	lv_obj_t * btn_end_task;

	int last_usb_status;
	uint8_t last_usb_function;

	lv_obj_t * btns_slave[8];
	lv_obj_t * btns_host[4];

	lv_style_t style_btn_base;
	lv_style_t style_btn_checked;
	lv_style_t style_btn_disabled;
} usb_state_t;

static usb_state_t *us = NULL;

// 内部函数声明
static void update_buttons_state(void);
static void btn_event_cb(lv_event_t * e);
static void end_task_event_cb(lv_event_t * e);

// 按键配置结构体
typedef struct {
	const char * name;
	uint8_t function_val;
} usb_btn_cfg_t;

static const usb_btn_cfg_t slave_btn_cfg[8] = {
	{"虚拟串口", USBD_CDC},
	{"虚拟U盘", USBD_MSC},
	{"解码耳放", USBD_UAC1},
	{"解码耳放", USBD_UAC2},
	{"电脑投屏", USBD_DISP},
	{"模拟手柄", USBD_GMPD},
	{"模拟键盘", USBD_KBD},
	{"模拟鼠标", USBD_MOU}
};

static const usb_btn_cfg_t host_btn_cfg[4] = {
	{"串口调试", USBH_CDC},
	{"U盘读取", USBH_MSC},
	{"手柄输入", USBH_GMPD},
	{"键鼠输入", USBH_HID}
};

// 初始化通用 UI 样式
static void init_custom_styles(void)
{
	lv_style_init(&us->style_btn_base);
	lv_style_set_shadow_width(&us->style_btn_base, 0);
	lv_style_set_border_width(&us->style_btn_base, 1);
	lv_style_set_border_color(&us->style_btn_base, lv_color_black());
	lv_style_set_bg_color(&us->style_btn_base, lv_color_hex(0xE0E0E0));
	lv_style_set_text_color(&us->style_btn_base, lv_color_black());
	lv_style_set_pad_all(&us->style_btn_base, 0);
	lv_style_set_radius(&us->style_btn_base, 4);

	lv_style_init(&us->style_btn_checked);
	lv_style_set_bg_color(&us->style_btn_checked, lv_color_hex(0x808080));
	lv_style_set_text_color(&us->style_btn_checked, lv_color_white());

	lv_style_init(&us->style_btn_disabled);
	lv_style_set_bg_color(&us->style_btn_disabled, lv_color_hex(0xF0F0F0));
	lv_style_set_text_color(&us->style_btn_disabled, lv_color_hex(0xA0A0A0));
	lv_style_set_border_color(&us->style_btn_disabled, lv_color_hex(0xC0C0C0));
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

	us->usb_ctrl_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(us->usb_ctrl_cont, 240, 180);
	lv_obj_center(us->usb_ctrl_cont);
	lv_obj_clear_flag(us->usb_ctrl_cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(us->usb_ctrl_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(us->usb_ctrl_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(us->usb_ctrl_cont, lv_color_white(), LV_PART_MAIN);

	int btn_w = 55, btn_h = 32, space_x = 3, space_y = 8, start_x = 5, start_y = 55;
	int end_btn_x = start_x + 3 * (btn_w + space_x);

	us->btn_end_task = lv_btn_create(us->usb_ctrl_cont);
	lv_obj_set_size(us->btn_end_task, btn_w, btn_h);
	lv_obj_add_style(us->btn_end_task, &us->style_btn_base, LV_PART_MAIN);
	lv_obj_align(us->btn_end_task, LV_ALIGN_TOP_LEFT, end_btn_x, 5);

	lv_obj_t * label_end = lv_label_create(us->btn_end_task);
	lv_label_set_text(label_end, "结束");
	lv_obj_align(label_end, LV_ALIGN_CENTER, 1, 0);
	lv_obj_add_event_cb(us->btn_end_task, end_task_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * label_status_title = lv_label_create(us->usb_ctrl_cont);
	lv_label_set_text(label_status_title, "USB状态:");
	lv_obj_set_style_text_font(label_status_title, &lv_font_16, 0);
	lv_obj_align(label_status_title, LV_ALIGN_TOP_LEFT, start_x, 12);

	us->label_status_val = lv_label_create(us->usb_ctrl_cont);
	lv_label_set_text(us->label_status_val, "未知");
	lv_obj_set_style_text_font(us->label_status_val, &lv_font_16, 0);
	lv_obj_align_to(us->label_status_val, label_status_title, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);

	// 前两排：从机模式功能 (8个)
	for (int i = 0; i < 8; i++) {
		int row = i / 4, col = i % 4;

		us->btns_slave[i] = lv_btn_create(us->usb_ctrl_cont);
		lv_obj_set_size(us->btns_slave[i], btn_w, btn_h);
		lv_obj_add_flag(us->btns_slave[i], LV_OBJ_FLAG_CHECKABLE);
		lv_obj_add_style(us->btns_slave[i], &us->style_btn_base, LV_PART_MAIN);
		lv_obj_add_style(us->btns_slave[i], &us->style_btn_checked, LV_STATE_CHECKED);
		lv_obj_add_style(us->btns_slave[i], &us->style_btn_disabled, LV_STATE_DISABLED);
		lv_obj_align(us->btns_slave[i], LV_ALIGN_TOP_LEFT, start_x + col * (btn_w + space_x), start_y + row * (btn_h + space_y));

		lv_obj_t * label = lv_label_create(us->btns_slave[i]);
		lv_label_set_text(label, slave_btn_cfg[i].name);
		lv_obj_align(label, LV_ALIGN_CENTER, 1, 0);

		lv_obj_add_event_cb(us->btns_slave[i], btn_event_cb, LV_EVENT_CLICKED, (void *)&slave_btn_cfg[i]);
	}

	// 第三排：主机模式功能 (4个)
	for (int i = 0; i < 4; i++) {
		int row = 2, col = i;

		us->btns_host[i] = lv_btn_create(us->usb_ctrl_cont);
		lv_obj_set_size(us->btns_host[i], btn_w, btn_h);
		lv_obj_add_flag(us->btns_host[i], LV_OBJ_FLAG_CHECKABLE);
		lv_obj_add_style(us->btns_host[i], &us->style_btn_base, LV_PART_MAIN);
		lv_obj_add_style(us->btns_host[i], &us->style_btn_checked, LV_STATE_CHECKED);
		lv_obj_add_style(us->btns_host[i], &us->style_btn_disabled, LV_STATE_DISABLED);
		lv_obj_align(us->btns_host[i], LV_ALIGN_TOP_LEFT, start_x + col * (btn_w + space_x), start_y + row * (btn_h + space_y));

		lv_obj_t * label = lv_label_create(us->btns_host[i]);
		lv_label_set_text(label, host_btn_cfg[i].name);
		lv_obj_align(label, LV_ALIGN_CENTER, 1, 0);

		lv_obj_add_event_cb(us->btns_host[i], btn_event_cb, LV_EVENT_CLICKED, (void *)&host_btn_cfg[i]);
	}

	us->last_usb_status = g_usb_status;
	us->last_usb_function = g_usb_function;
	update_buttons_state();
}

void Update_USB_Unit(void)
{
	if (us == NULL) return;

	if (us->last_usb_status != g_usb_status || us->last_usb_function != g_usb_function) {
		us->last_usb_status = g_usb_status;
		us->last_usb_function = g_usb_function;
		update_buttons_state();
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

// ---------------- 内部回调与状态更新实现 ----------------

static void update_buttons_state(void)
{
	bool is_slave = (g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY);
	bool is_host  = (g_usb_status == TYPEC_CC_HOST || g_usb_status == TYPEC_IS_HOST);

	if (is_slave) {
		for (int i = 0; i < 8; i++) lv_obj_clear_state(us->btns_slave[i], LV_STATE_DISABLED);
		for (int i = 0; i < 4; i++) lv_obj_add_state(us->btns_host[i], LV_STATE_DISABLED);
	} else if (is_host) {
		for (int i = 0; i < 8; i++) lv_obj_add_state(us->btns_slave[i], LV_STATE_DISABLED);
		for (int i = 0; i < 4; i++) lv_obj_clear_state(us->btns_host[i], LV_STATE_DISABLED);
	} else {
		for (int i = 0; i < 8; i++) lv_obj_add_state(us->btns_slave[i], LV_STATE_DISABLED);
		for (int i = 0; i < 4; i++) lv_obj_add_state(us->btns_host[i], LV_STATE_DISABLED);
	}

	for (int i = 0; i < 8; i++) lv_obj_clear_state(us->btns_slave[i], LV_STATE_CHECKED);
	for (int i = 0; i < 4; i++) lv_obj_clear_state(us->btns_host[i], LV_STATE_CHECKED);

	if (g_usb_function == USB_NONE) {
		if (is_slave) {
			lv_label_set_text(us->label_status_val, "仅充电");
		} else if (is_host) {
			lv_label_set_text(us->label_status_val, "仅供电");
		} else {
			if (g_usb_status == TYPEC_CC_IDLE) {
				lv_label_set_text(us->label_status_val, "C-C线缆接入");
			} else if (g_usb_status == TYPEC_AC_IDLE) {
				lv_label_set_text(us->label_status_val, "A-C线缆接入");
			} else {
				lv_label_set_text(us->label_status_val, "未连接");
			}
		}
	} else {
		bool found = false;

		if (is_slave) {
			for (int i = 0; i < 8; i++) {
				if (slave_btn_cfg[i].function_val == g_usb_function) {
					lv_obj_add_state(us->btns_slave[i], LV_STATE_CHECKED);
					lv_label_set_text(us->label_status_val, slave_btn_cfg[i].name);
					found = true;
					break;
				}
			}
		} else if (is_host) {
			for (int i = 0; i < 4; i++) {
				if (host_btn_cfg[i].function_val == g_usb_function) {
					lv_obj_add_state(us->btns_host[i], LV_STATE_CHECKED);
					lv_label_set_text(us->label_status_val, host_btn_cfg[i].name);
					found = true;
					break;
				}
			}
		}

		if (!found) {
			lv_label_set_text(us->label_status_val, "状态异常");
		}
	}
}

static void btn_event_cb(lv_event_t * e)
{
	usb_btn_cfg_t * cfg = (usb_btn_cfg_t *)lv_event_get_user_data(e);
	g_usb_function = cfg->function_val;
	us->last_usb_function = g_usb_function;
	update_buttons_state();
}

static void end_task_event_cb(lv_event_t * e)
{
	g_usb_function = USB_NONE;
	us->last_usb_function = g_usb_function;
	update_buttons_state();
}
