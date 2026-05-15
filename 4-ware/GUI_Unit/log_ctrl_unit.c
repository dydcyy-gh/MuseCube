#include "log_ctrl_unit.h"
#include "lvgl.h"
#include "debug.h"
#include "variables.h"
#include "page_manager.h"
#include "malloc.h"

// --- 状态结构体 ---
typedef struct {
	uint8_t selected_active_mode;

	lv_obj_t * ctrl_cont;
	lv_obj_t * sw_debug;
	lv_obj_t * btn_mode_tsdb;
	lv_obj_t * btn_mode_usb;
	lv_obj_t * btn_mode_lvgl;
	lv_obj_t * btn_dump_usb;
	lv_obj_t * btn_dump_lvgl;

	lv_style_t style_btn_base;
	lv_style_t style_btn_checked;
	lv_style_t style_btn_disabled;
} log_state_t;

static log_state_t *ls = NULL;

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
	lv_style_init(&ls->style_btn_base);
	lv_style_set_shadow_width(&ls->style_btn_base, 0);
	lv_style_set_border_width(&ls->style_btn_base, 1);
	lv_style_set_border_color(&ls->style_btn_base, lv_color_black());
	lv_style_set_bg_color(&ls->style_btn_base, lv_color_hex(0xE0E0E0));
	lv_style_set_text_color(&ls->style_btn_base, lv_color_black());
	lv_style_set_pad_all(&ls->style_btn_base, 0);
	lv_style_set_radius(&ls->style_btn_base, 6);

	lv_style_init(&ls->style_btn_checked);
	lv_style_set_bg_color(&ls->style_btn_checked, lv_color_hex(0x808080));
	lv_style_set_text_color(&ls->style_btn_checked, lv_color_white());

	lv_style_init(&ls->style_btn_disabled);
	lv_style_set_bg_color(&ls->style_btn_disabled, lv_color_hex(0xF0F0F0));
	lv_style_set_text_color(&ls->style_btn_disabled, lv_color_hex(0xA0A0A0));
	lv_style_set_border_color(&ls->style_btn_disabled, lv_color_hex(0xC0C0C0));
}

void Create_Log_Control(void)
{
	if (ls != NULL) return;

	ls = (log_state_t *)malloc_ccm(sizeof(log_state_t));
	if (!ls) return;
	memset(ls, 0, sizeof(log_state_t));

	init_custom_styles();

	if (debug_mode != Debug_Mode_None) {
		ls->selected_active_mode = debug_mode;
	} else {
		ls->selected_active_mode = Debug_Mode_None;
	}

	ls->ctrl_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(ls->ctrl_cont, 240, 180);
	lv_obj_center(ls->ctrl_cont);
	lv_obj_clear_flag(ls->ctrl_cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(ls->ctrl_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ls->ctrl_cont, 0, LV_PART_MAIN);

	lv_obj_t * label_sw = lv_label_create(ls->ctrl_cont);
	lv_label_set_text(label_sw, "开启调试功能:");
	lv_obj_set_style_text_font(label_sw, &lv_font_16, LV_PART_MAIN);
	lv_obj_align(label_sw, LV_ALIGN_TOP_LEFT, 20, 15);

	ls->sw_debug = lv_switch_create(ls->ctrl_cont);
	lv_obj_set_size(ls->sw_debug, 60, 25);
	lv_obj_align(ls->sw_debug, LV_ALIGN_TOP_LEFT, 160, 10);

	lv_obj_set_style_shadow_width(ls->sw_debug, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(ls->sw_debug, 4, LV_PART_MAIN);
	lv_obj_set_style_shadow_width(ls->sw_debug, 0, LV_PART_INDICATOR);
	lv_obj_set_style_radius(ls->sw_debug, 4, LV_PART_INDICATOR);
	lv_obj_set_style_shadow_width(ls->sw_debug, 0, LV_PART_KNOB);
	lv_obj_set_style_radius(ls->sw_debug, 4, LV_PART_KNOB);

	if (debug_mode != Debug_Mode_None) {
		lv_obj_add_state(ls->sw_debug, LV_STATE_CHECKED);
	}
	lv_obj_add_event_cb(ls->sw_debug, sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

	lv_obj_t * label_rt_mode = lv_label_create(ls->ctrl_cont);
	lv_label_set_text(label_rt_mode, "实时日志输出方式");
	lv_obj_align(label_rt_mode, LV_ALIGN_TOP_LEFT, 20, 60);

	ls->btn_mode_tsdb = lv_btn_create(ls->ctrl_cont);
	lv_obj_set_size(ls->btn_mode_tsdb, 60, 25);
	lv_obj_add_flag(ls->btn_mode_tsdb, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(ls->btn_mode_tsdb, &ls->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(ls->btn_mode_tsdb, &ls->style_btn_checked, LV_STATE_CHECKED);
	lv_obj_add_style(ls->btn_mode_tsdb, &ls->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_align(ls->btn_mode_tsdb, LV_ALIGN_TOP_LEFT, 20, 80);
	lv_obj_t * label_tsdb = lv_label_create(ls->btn_mode_tsdb);
	lv_label_set_text(label_tsdb, "TSDB");
	lv_obj_center(label_tsdb);
	lv_obj_add_event_cb(ls->btn_mode_tsdb, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_TSDB);

	ls->btn_mode_usb = lv_btn_create(ls->ctrl_cont);
	lv_obj_set_size(ls->btn_mode_usb, 60, 25);
	lv_obj_add_flag(ls->btn_mode_usb, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(ls->btn_mode_usb, &ls->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(ls->btn_mode_usb, &ls->style_btn_checked, LV_STATE_CHECKED);
	lv_obj_add_style(ls->btn_mode_usb, &ls->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_align(ls->btn_mode_usb, LV_ALIGN_TOP_LEFT, 90, 80);
	lv_obj_t * label_usb_mode = lv_label_create(ls->btn_mode_usb);
	lv_label_set_text(label_usb_mode, "USB");
	lv_obj_center(label_usb_mode);
	lv_obj_add_event_cb(ls->btn_mode_usb, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_USBD);

	ls->btn_mode_lvgl = lv_btn_create(ls->ctrl_cont);
	lv_obj_set_size(ls->btn_mode_lvgl, 60, 25);
	lv_obj_add_flag(ls->btn_mode_lvgl, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(ls->btn_mode_lvgl, &ls->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(ls->btn_mode_lvgl, &ls->style_btn_checked, LV_STATE_CHECKED);
	lv_obj_add_style(ls->btn_mode_lvgl, &ls->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_align(ls->btn_mode_lvgl, LV_ALIGN_TOP_LEFT, 160, 80);
	lv_obj_t * label_lvgl_mode = lv_label_create(ls->btn_mode_lvgl);
	lv_label_set_text(label_lvgl_mode, "LVGL");
	lv_obj_center(label_lvgl_mode);
	lv_obj_add_event_cb(ls->btn_mode_lvgl, mode_btn_event_cb, LV_EVENT_CLICKED, (void*)Debug_Mode_LVGL);

	update_mode_buttons_ui();

	lv_obj_t * label_dump_mode = lv_label_create(ls->ctrl_cont);
	lv_label_set_text(label_dump_mode, "储存日志输出方式");
	lv_obj_align(label_dump_mode, LV_ALIGN_TOP_LEFT, 20, 120);

	ls->btn_dump_usb = lv_btn_create(ls->ctrl_cont);
	lv_obj_set_size(ls->btn_dump_usb, 95, 25);
	lv_obj_add_style(ls->btn_dump_usb, &ls->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(ls->btn_dump_usb, &ls->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_align(ls->btn_dump_usb, LV_ALIGN_TOP_LEFT, 20, 140);
	lv_obj_t * label_dump_usb = lv_label_create(ls->btn_dump_usb);
	lv_label_set_text(label_dump_usb, "Dump USB");
	lv_obj_center(label_dump_usb);
	lv_obj_add_event_cb(ls->btn_dump_usb, dump_usb_event_cb, LV_EVENT_CLICKED, NULL);

	ls->btn_dump_lvgl = lv_btn_create(ls->ctrl_cont);
	lv_obj_set_size(ls->btn_dump_lvgl, 95, 25);
	lv_obj_add_style(ls->btn_dump_lvgl, &ls->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(ls->btn_dump_lvgl, &ls->style_btn_disabled, LV_STATE_DISABLED);
	lv_obj_align(ls->btn_dump_lvgl, LV_ALIGN_TOP_LEFT, 125, 140);
	lv_obj_t * label_dump_lvgl = lv_label_create(ls->btn_dump_lvgl);
	lv_label_set_text(label_dump_lvgl, "Dump LVGL");
	lv_obj_center(label_dump_lvgl);
	lv_obj_add_event_cb(ls->btn_dump_lvgl, dump_lvgl_event_cb, LV_EVENT_CLICKED, NULL);

	update_all_buttons_state();
}

void Update_Log_Control(void)
{
}

void Remove_Log_Control(void)
{
	if (ls == NULL) return;

	if (ls->ctrl_cont != NULL) {
		lv_obj_del(ls->ctrl_cont);
	}

	free_ccm(ls);
	ls = NULL;
}

// ---------------- 内部回调与状态更新实现 ----------------

static void update_all_buttons_state(void)
{
	if (!lv_obj_has_state(ls->sw_debug, LV_STATE_CHECKED)) {
		lv_obj_clear_state(ls->btn_dump_usb, LV_STATE_DISABLED);
		lv_obj_clear_state(ls->btn_dump_lvgl, LV_STATE_DISABLED);

		lv_obj_add_state(ls->btn_mode_tsdb, LV_STATE_DISABLED);
		lv_obj_add_state(ls->btn_mode_usb, LV_STATE_DISABLED);
		lv_obj_add_state(ls->btn_mode_lvgl, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(ls->btn_dump_usb, LV_STATE_DISABLED);
		lv_obj_add_state(ls->btn_dump_lvgl, LV_STATE_DISABLED);

		lv_obj_clear_state(ls->btn_mode_tsdb, LV_STATE_DISABLED);
		lv_obj_clear_state(ls->btn_mode_usb, LV_STATE_DISABLED);
		lv_obj_clear_state(ls->btn_mode_lvgl, LV_STATE_DISABLED);
	}
}

static void update_mode_buttons_ui(void)
{
	lv_obj_clear_state(ls->btn_mode_tsdb, LV_STATE_CHECKED);
	lv_obj_clear_state(ls->btn_mode_usb, LV_STATE_CHECKED);
	lv_obj_clear_state(ls->btn_mode_lvgl, LV_STATE_CHECKED);

	if (ls->selected_active_mode == Debug_Mode_TSDB) {
		lv_obj_add_state(ls->btn_mode_tsdb, LV_STATE_CHECKED);
	} else if (ls->selected_active_mode == Debug_Mode_USBD) {
		lv_obj_add_state(ls->btn_mode_usb, LV_STATE_CHECKED);
	} else if (ls->selected_active_mode == Debug_Mode_LVGL) {
		lv_obj_add_state(ls->btn_mode_lvgl, LV_STATE_CHECKED);
	}
}

static void sw_event_cb(lv_event_t * e)
{
	lv_obj_t * sw = lv_event_get_target(e);
	if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
		ls->selected_active_mode = Debug_Mode_None;
		debug_mode = Debug_Mode_None;
	} else {
		ls->selected_active_mode = Debug_Mode_None;
		debug_mode = Debug_Mode_None;
	}

	update_all_buttons_state();
	update_mode_buttons_ui();
}

static void mode_btn_event_cb(lv_event_t * e)
{
	uint8_t mode_val = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
	ls->selected_active_mode = mode_val;

	update_mode_buttons_ui();

	if (lv_obj_has_state(ls->sw_debug, LV_STATE_CHECKED)) {
		debug_mode = ls->selected_active_mode;

		if (debug_mode == Debug_Mode_LVGL) {
			Page_Request_Switch(PAGE_DEBUG);
		}
	}
}

static void dump_usb_event_cb(lv_event_t * e)
{
	Debug_Dump_TSDB_To_USB(50);
}

extern volatile int tsdb_lvgl_need_dump;

static void dump_lvgl_event_cb(lv_event_t * e)
{
	tsdb_lvgl_need_dump = 1;
	Page_Request_Switch(PAGE_DEBUG);
}
