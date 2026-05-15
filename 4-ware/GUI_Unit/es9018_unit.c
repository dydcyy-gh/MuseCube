#include "es9018_unit.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "es9018k2m.h"
#include "page_manager.h"
#include "variables.h"
#include "malloc.h"

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * es9018_cont;
	ES9018_Config_t cur_cfg;

	lv_style_t style_btn_base;
	lv_style_t style_slider_bg;
	lv_style_t style_slider_indic;
	lv_style_t style_slider_knob;
} es9018_state_t;

static es9018_state_t *es = NULL;

// 控件ID定义 (共18个选项)
enum {
	ID_FILTER_SHAPE = 1,
	ID_IIR_BW,
	ID_STOP_DIV,
	ID_SEL_CH1,
	ID_SEL_CH2,
	ID_DPLL_BW,
	ID_VOL_RATE,
	ID_AUTO_MUTE,
	ID_AUTOMUTE_TIME,
	ID_AUTOMUTE_LEVEL,
	ID_MUTE_CH1,
	ID_MUTE_CH2,
	ID_MUTE_LOCK,
	ID_OUTL_LOCK,
	ID_BYPASS_IIR,
	ID_BYPASS_THD,
	ID_C2,
	ID_C3
};

// 选项字符串常量
static const char * filter_opts[] = {"Fast Rolloff", "Slow Rolloff", "Minimum Phase"};
static const char * iir_opts[] = {"1.0757 fs", "1.1338 fs", "1.3605 fs", "1.5873 fs"};
static const char * ch_opts[] = {"Left", "Right"};
static const char * stop_div_opts[] = {"18364", "8192", "5461", "4096", "3276", "2730", "2340", "2048"};

static void init_custom_styles(void)
{
	lv_style_init(&es->style_btn_base);
	lv_style_set_radius(&es->style_btn_base, 4);
	lv_style_set_border_width(&es->style_btn_base, 0);
	lv_style_set_bg_color(&es->style_btn_base, lv_color_hex(0xEEEEEE));
	lv_style_set_text_color(&es->style_btn_base, lv_color_black());
	lv_style_set_shadow_width(&es->style_btn_base, 0);

	lv_style_init(&es->style_slider_bg);
	lv_style_set_radius(&es->style_slider_bg, 4);
	lv_style_set_border_width(&es->style_slider_bg, 1);
	lv_style_set_border_color(&es->style_slider_bg, lv_color_black());
	lv_style_set_bg_color(&es->style_slider_bg, lv_color_hex(0xE0E0E0));
	lv_style_set_pad_all(&es->style_slider_bg, 1);

	lv_style_init(&es->style_slider_indic);
	lv_style_set_radius(&es->style_slider_indic, 3);
	lv_style_set_bg_color(&es->style_slider_indic, lv_palette_main(LV_PALETTE_BLUE));

	lv_style_init(&es->style_slider_knob);
	lv_style_set_bg_opa(&es->style_slider_knob, LV_OPA_TRANSP);
}

static void setting_event_cb(lv_event_t * e)
{
	lv_obj_t * obj = lv_event_get_target(e);
	int id = (int)(intptr_t)lv_event_get_user_data(e);

	ES9018_Get_Config(&es->cur_cfg);

	switch(id) {
		case ID_FILTER_SHAPE:
			es->cur_cfg.Filter_Shape = (es->cur_cfg.Filter_Shape + 1) % 3;
			lv_label_set_text(lv_obj_get_child(obj, 0), filter_opts[es->cur_cfg.Filter_Shape]);
			break;
		case ID_IIR_BW:
			es->cur_cfg.IIR_BW = (es->cur_cfg.IIR_BW + 1) % 4;
			lv_label_set_text(lv_obj_get_child(obj, 0), iir_opts[es->cur_cfg.IIR_BW]);
			break;
		case ID_STOP_DIV:
			es->cur_cfg.Stop_Div = (es->cur_cfg.Stop_Div + 1) % 8;
			lv_label_set_text(lv_obj_get_child(obj, 0), stop_div_opts[es->cur_cfg.Stop_Div]);
			break;
		case ID_SEL_CH1:
			es->cur_cfg.Sel_Ch1 = (es->cur_cfg.Sel_Ch1 + 1) % 2;
			lv_label_set_text(lv_obj_get_child(obj, 0), ch_opts[es->cur_cfg.Sel_Ch1]);
			break;
		case ID_SEL_CH2:
			es->cur_cfg.Sel_Ch2 = (es->cur_cfg.Sel_Ch2 + 1) % 2;
			lv_label_set_text(lv_obj_get_child(obj, 0), ch_opts[es->cur_cfg.Sel_Ch2]);
			break;

		case ID_DPLL_BW:
			es->cur_cfg.DPLL_BW = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.DPLL_BW);
			break;
		case ID_VOL_RATE:
			es->cur_cfg.Vol_Rate = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.Vol_Rate);
			break;
		case ID_AUTOMUTE_TIME:
			es->cur_cfg.Automute_Time = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.Automute_Time);
			break;
		case ID_AUTOMUTE_LEVEL:
			es->cur_cfg.Automute_Level = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.Automute_Level);
			break;
		case ID_C2:
			es->cur_cfg.C2 = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.C2);
			break;
		case ID_C3:
			es->cur_cfg.C3 = lv_slider_get_value(obj);
			lv_label_set_text_fmt(lv_obj_get_child(obj, 0), "%d", es->cur_cfg.C3);
			break;

		case ID_AUTO_MUTE:
			es->cur_cfg.Enable_Loopback = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_MUTE_CH1:
			es->cur_cfg.Mute_Ch1 = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_MUTE_CH2:
			es->cur_cfg.Mute_Ch2 = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_MUTE_LOCK:
			es->cur_cfg.Mute_Lock = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_OUTL_LOCK:
			es->cur_cfg.OutL_Lock = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_BYPASS_IIR:
			es->cur_cfg.Bypass_IIR = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;
		case ID_BYPASS_THD:
			es->cur_cfg.Bypass_THD = lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0;
			break;

		default:
			break;
	}

	ES9018_Set_Config(&es->cur_cfg);
}

static void create_cycle_btn_row(lv_obj_t * parent, int y_ofs, const char * title, const char * init_text, int id)
{
	lv_obj_t * label = lv_label_create(parent);
	lv_obj_set_style_text_font(label, &lv_font_12, 0);
	lv_label_set_text(label, title);
	lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, y_ofs + 4);

	lv_obj_t * btn = lv_btn_create(parent);
	lv_obj_set_size(btn, 110, 20);
	lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -10, y_ofs);
	lv_obj_add_style(btn, &es->style_btn_base, LV_PART_MAIN);
	lv_obj_add_event_cb(btn, setting_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)id);

	lv_obj_t * btn_label = lv_label_create(btn);
	lv_obj_set_style_text_font(btn_label, &lv_font_12, 0);
	lv_label_set_text(btn_label, init_text);
	lv_obj_center(btn_label);
}

static void create_slider_row(lv_obj_t * parent, int y_ofs, const char * title, int min, int max, int id, int current_val)
{
	lv_obj_t * label = lv_label_create(parent);
	lv_obj_set_style_text_font(label, &lv_font_12, 0);
	lv_label_set_text(label, title);
	lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, y_ofs + 2);

	lv_obj_t * slider = lv_slider_create(parent);
	lv_slider_set_range(slider, min, max);
	lv_slider_set_value(slider, current_val, LV_ANIM_OFF);
	lv_obj_set_size(slider, 110, 16);
	lv_obj_align(slider, LV_ALIGN_TOP_RIGHT, -10, y_ofs);

	lv_obj_add_style(slider, &es->style_slider_bg, LV_PART_MAIN);
	lv_obj_add_style(slider, &es->style_slider_indic, LV_PART_INDICATOR);
	lv_obj_add_style(slider, &es->style_slider_knob, LV_PART_KNOB);
	lv_obj_set_ext_click_area(slider, 0);

	lv_obj_t * val_label = lv_label_create(slider);
	lv_obj_set_style_text_font(val_label, &lv_font_12, 0);
	lv_obj_set_style_text_color(val_label, lv_color_black(), 0);
	lv_label_set_text_fmt(val_label, "%d", current_val);
	lv_obj_center(val_label);

	lv_obj_add_event_cb(slider, setting_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
}

static void create_switch_row(lv_obj_t * parent, int y_ofs, const char * title, int id, uint8_t is_on)
{
	lv_obj_t * label = lv_label_create(parent);
	lv_obj_set_style_text_font(label, &lv_font_12, 0);
	lv_label_set_text(label, title);
	lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, y_ofs + 2);

	lv_obj_t * sw = lv_switch_create(parent);
	lv_obj_set_size(sw, 40, 16);
	lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -10, y_ofs);
	if(is_on) {
		lv_obj_add_state(sw, LV_STATE_CHECKED);
	}
	lv_obj_add_event_cb(sw, setting_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)id);
}

void Create_ES9018_Unit(void)
{
	if (es != NULL) return;

	es = (es9018_state_t *)malloc_ccm(sizeof(es9018_state_t));
	if (!es) return;
	memset(es, 0, sizeof(es9018_state_t));

	init_custom_styles();

	ES9018_Get_Config(&es->cur_cfg);

	es->es9018_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(es->es9018_cont, 240, 180);
	lv_obj_center(es->es9018_cont);

	lv_obj_set_style_bg_color(es->es9018_cont, lv_color_white(), 0);
	lv_obj_set_style_border_width(es->es9018_cont, 0, 0);
	lv_obj_set_style_radius(es->es9018_cont, 0, 0);
	lv_obj_set_style_pad_all(es->es9018_cont, 0, 0);
	lv_obj_set_scroll_dir(es->es9018_cont, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(es->es9018_cont, LV_SCROLLBAR_MODE_OFF);

	int y_ofs = 10;

	lv_obj_t * title = lv_label_create(es->es9018_cont);
	lv_obj_set_style_text_font(title, &lv_font_12, 0);
	lv_label_set_recolor(title, true);
	lv_label_set_text(title, "#000000 ES9018K2M 芯片高级配置#");
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_ofs);
	y_ofs += 25;

	lv_obj_t * line = lv_obj_create(es->es9018_cont);
	lv_obj_set_size(line, 220, 1);
	lv_obj_set_style_bg_color(line, lv_color_hex(0xCCCCCC), 0);
	lv_obj_set_style_border_width(line, 0, 0);
	lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y_ofs);
	y_ofs += 15;

	create_cycle_btn_row(es->es9018_cont, y_ofs, "数字滤波器", filter_opts[es->cur_cfg.Filter_Shape], ID_FILTER_SHAPE);
	y_ofs += 30;

	create_cycle_btn_row(es->es9018_cont, y_ofs, "IIR 滤波器带宽", iir_opts[es->cur_cfg.IIR_BW], ID_IIR_BW);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "旁路 IIR 滤波", ID_BYPASS_IIR, es->cur_cfg.Bypass_IIR);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "DPLL 带宽", 0, 15, ID_DPLL_BW, es->cur_cfg.DPLL_BW);
	y_ofs += 30;

	create_cycle_btn_row(es->es9018_cont, y_ofs, "锁定信号时钟数", stop_div_opts[es->cur_cfg.Stop_Div], ID_STOP_DIV);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "音量渐变速率", 0, 7, ID_VOL_RATE, es->cur_cfg.Vol_Rate);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "平滑自动静音", ID_AUTO_MUTE, es->cur_cfg.Enable_Loopback);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "自动静音时间", 0, 255, ID_AUTOMUTE_TIME, es->cur_cfg.Automute_Time);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "自动静音等级", 0, 127, ID_AUTOMUTE_LEVEL, es->cur_cfg.Automute_Level);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "静音通道 1", ID_MUTE_CH1, es->cur_cfg.Mute_Ch1);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "静音通道 2", ID_MUTE_CH2, es->cur_cfg.Mute_Ch2);
	y_ofs += 30;

	create_cycle_btn_row(es->es9018_cont, y_ofs, "通道 1 映射", ch_opts[es->cur_cfg.Sel_Ch1], ID_SEL_CH1);
	y_ofs += 30;

	create_cycle_btn_row(es->es9018_cont, y_ofs, "通道 2 映射", ch_opts[es->cur_cfg.Sel_Ch2], ID_SEL_CH2);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "旁路 THD 补偿", ID_BYPASS_THD, es->cur_cfg.Bypass_THD);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "C2 谐波补偿", -1000, 1000, ID_C2, es->cur_cfg.C2);
	y_ofs += 30;

	create_slider_row(es->es9018_cont, y_ofs, "C3 谐波补偿", -1000, 1000, ID_C3, es->cur_cfg.C3);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "失锁时强制静音", ID_MUTE_LOCK, es->cur_cfg.Mute_Lock);
	y_ofs += 30;

	create_switch_row(es->es9018_cont, y_ofs, "失锁时输出变低", ID_OUTL_LOCK, es->cur_cfg.OutL_Lock);
	y_ofs += 30;

	lv_obj_t * bottom_pad = lv_obj_create(es->es9018_cont);
	lv_obj_set_size(bottom_pad, 10, 20);
	lv_obj_set_style_opa(bottom_pad, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(bottom_pad, 0, 0);
	lv_obj_align(bottom_pad, LV_ALIGN_TOP_MID, 0, y_ofs);
}

void Update_ES9018_Unit(void)
{
}

void Remove_ES9018_Unit(void)
{
	if (es == NULL) return;

	if (es->es9018_cont != NULL) {
		lv_obj_del(es->es9018_cont);
	}

	free_ccm(es);
	es = NULL;
}
