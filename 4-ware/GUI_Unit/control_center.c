#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "pin_ctrl.h"
#include "malloc.h"

extern const lv_img_dsc_t control_light;
extern const lv_img_dsc_t control_headphone;
extern const lv_img_dsc_t control_speaker;
extern const lv_img_dsc_t control_exit;

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * ctrl_center_cont;
	lv_obj_t * btn_close;
	lv_obj_t * bg_cover;

	lv_obj_t * switch_bg;
	lv_obj_t * switch_knob;

	lv_obj_t * btn_bright_pwr;
	lv_obj_t * btn_hdp_pwr;
	lv_obj_t * btn_spk_pwr;

	lv_obj_t * slider_bright;
	lv_obj_t * slider_hdp;
	lv_obj_t * slider_spk;

	lv_obj_t * label_bright;
	lv_obj_t * label_hdp;
	lv_obj_t * label_spk;

	lv_style_t style_cont;
	lv_style_t style_btn_base;
	lv_style_t style_btn_close;
	lv_style_t style_btn_checked;
	lv_style_t style_slider_bg;
	lv_style_t style_slider_knob;
	lv_style_t style_slider_indic;
	lv_style_t style_slider_indic_disabled;
} cc_state_t;

static cc_state_t *cc = NULL;

// 内部函数声明
static void bg_cover_event_cb(lv_event_t * e);
static void btn_close_event_cb(lv_event_t * e);
static void btn_pwr_event_cb(lv_event_t * e);
static void custom_switch_event_cb(lv_event_t * e);
static void slider_value_event_cb(lv_event_t * e);
static void update_controls_state(void);

// 初始化通用 UI 样式
static void init_custom_styles(void)
{
	lv_style_init(&cc->style_cont);
	lv_style_set_radius(&cc->style_cont, 10);
	lv_style_set_border_width(&cc->style_cont, 1);
	lv_style_set_border_color(&cc->style_cont, lv_color_black());
	lv_style_set_bg_color(&cc->style_cont, lv_color_white());
	lv_style_set_bg_opa(&cc->style_cont, LV_OPA_COVER);
	lv_style_set_pad_all(&cc->style_cont, 0);

	lv_style_init(&cc->style_btn_base);
	lv_style_set_radius(&cc->style_btn_base, 4);
	lv_style_set_border_width(&cc->style_btn_base, 0);
	lv_style_set_border_color(&cc->style_btn_base, lv_color_black());
	lv_style_set_bg_color(&cc->style_btn_base, lv_color_hex(0xCCCCCC));
	lv_style_set_shadow_width(&cc->style_btn_base, 0);
	lv_style_set_pad_all(&cc->style_btn_base, 0);

	lv_style_init(&cc->style_btn_checked);
	lv_style_set_bg_color(&cc->style_btn_checked, lv_palette_main(LV_PALETTE_BLUE));

	lv_style_init(&cc->style_btn_close);
	lv_style_set_radius(&cc->style_btn_close, 4);
	lv_style_set_border_width(&cc->style_btn_close, 0);
	lv_style_set_border_color(&cc->style_btn_close, lv_color_black());
	lv_style_set_bg_color(&cc->style_btn_close, lv_palette_main(LV_PALETTE_RED));
	lv_style_set_shadow_width(&cc->style_btn_close, 0);
	lv_style_set_pad_all(&cc->style_btn_close, 0);

	lv_style_init(&cc->style_slider_bg);
	lv_style_set_radius(&cc->style_slider_bg, 4);
	lv_style_set_border_width(&cc->style_slider_bg, 1);
	lv_style_set_border_color(&cc->style_slider_bg, lv_color_black());
	lv_style_set_bg_color(&cc->style_slider_bg, lv_color_hex(0xE0E0E0));
	lv_style_set_pad_all(&cc->style_slider_bg, 1);

	lv_style_init(&cc->style_slider_knob);
	lv_style_set_radius(&cc->style_slider_knob, 4);
	lv_style_set_bg_color(&cc->style_slider_knob, lv_color_hex(0x666666));
	lv_style_set_border_width(&cc->style_slider_knob, 0);
	lv_style_set_pad_all(&cc->style_slider_knob, 0);

	lv_style_init(&cc->style_slider_indic);
	lv_style_set_radius(&cc->style_slider_indic, 3);
	lv_style_set_bg_color(&cc->style_slider_indic, lv_palette_main(LV_PALETTE_BLUE));

	lv_style_init(&cc->style_slider_indic_disabled);
	lv_style_set_radius(&cc->style_slider_indic_disabled, 3);
	lv_style_set_bg_color(&cc->style_slider_indic_disabled, lv_color_hex(0xAAAAAA));
}

void Create_Control_Center(void)
{
	if (cc != NULL) return;

	cc = (cc_state_t *)malloc_ccm(sizeof(cc_state_t));
	if (!cc) return;
	memset(cc, 0, sizeof(cc_state_t));

	init_custom_styles();

	lv_coord_t screen_w = lv_disp_get_hor_res(NULL);
	lv_coord_t screen_h = lv_disp_get_ver_res(NULL);

	// 1. 创建全屏透明背景
	cc->bg_cover = lv_obj_create(lv_layer_top());
	lv_obj_set_size(cc->bg_cover, screen_w, screen_h);
	lv_obj_set_pos(cc->bg_cover, 0, 0);
	lv_obj_set_style_bg_opa(cc->bg_cover, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(cc->bg_cover, 0, 0);
	lv_obj_clear_flag(cc->bg_cover, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(cc->bg_cover, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(cc->bg_cover, bg_cover_event_cb, LV_EVENT_CLICKED, NULL);

	// 2. 创建主容器
	cc->ctrl_center_cont = lv_obj_create(lv_layer_top());
	lv_obj_set_size(cc->ctrl_center_cont, 200, 100);
	lv_obj_set_pos(cc->ctrl_center_cont, 20, 100);
	lv_obj_clear_flag(cc->ctrl_center_cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(cc->ctrl_center_cont, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_style(cc->ctrl_center_cont, &cc->style_cont, LV_PART_MAIN);

	// 3. 左上角关闭按钮
	cc->btn_close = lv_btn_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->btn_close, 20, 20);
	lv_obj_set_pos(cc->btn_close, 10, 10);
	lv_obj_add_style(cc->btn_close, &cc->style_btn_close, LV_PART_MAIN);
	lv_obj_add_event_cb(cc->btn_close, btn_close_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t * img_close = lv_img_create(cc->btn_close);
	lv_img_set_src(img_close, &control_exit);
	lv_obj_align(img_close, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_img_recolor(img_close, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(img_close, LV_OPA_COVER, 0);

	// 4. 左侧自定义垂直滑块
	cc->switch_bg = lv_obj_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->switch_bg, 20, 50);
	lv_obj_set_pos(cc->switch_bg, 10, 40);
	lv_obj_add_style(cc->switch_bg, &cc->style_slider_bg, LV_PART_MAIN);
	lv_obj_set_style_border_width(cc->switch_bg, 0, LV_PART_MAIN);
	lv_obj_clear_flag(cc->switch_bg, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(cc->switch_bg, custom_switch_event_cb, LV_EVENT_CLICKED, NULL);

	cc->switch_knob = lv_obj_create(cc->switch_bg);
	lv_obj_set_size(cc->switch_knob, 16, 23);
	lv_obj_add_style(cc->switch_knob, &cc->style_slider_knob, LV_PART_MAIN);
	lv_obj_clear_flag(cc->switch_knob, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(cc->switch_knob, LV_OBJ_FLAG_EVENT_BUBBLE);

	if (g_hdp0_or_spk1 == 0) {
		lv_obj_align(cc->switch_knob, LV_ALIGN_TOP_MID, 0, 0);
	} else {
		lv_obj_align(cc->switch_knob, LV_ALIGN_BOTTOM_MID, 0, 0);
	}

	// 5. 中部三个电源按钮
	int btn_mid_x = 40;

	cc->btn_bright_pwr = lv_btn_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->btn_bright_pwr, 20, 20);
	lv_obj_set_pos(cc->btn_bright_pwr, btn_mid_x, 10);
	lv_obj_add_flag(cc->btn_bright_pwr, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(cc->btn_bright_pwr, &cc->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(cc->btn_bright_pwr, &cc->style_btn_checked, LV_STATE_CHECKED);
	if (g_screen_status == 1) lv_obj_add_state(cc->btn_bright_pwr, LV_STATE_CHECKED);
	lv_obj_add_event_cb(cc->btn_bright_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0);

	lv_obj_t * img_bright = lv_img_create(cc->btn_bright_pwr);
	lv_img_set_src(img_bright, &control_light);
	lv_obj_align(img_bright, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_img_recolor(img_bright, lv_color_hex(0x333333), 0);
	lv_obj_set_style_img_recolor_opa(img_bright, LV_OPA_COVER, 0);

	cc->btn_hdp_pwr = lv_btn_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->btn_hdp_pwr, 20, 20);
	lv_obj_set_pos(cc->btn_hdp_pwr, btn_mid_x, 40);
	lv_obj_add_flag(cc->btn_hdp_pwr, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(cc->btn_hdp_pwr, &cc->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(cc->btn_hdp_pwr, &cc->style_btn_checked, LV_STATE_CHECKED);
	if (g_es9018_status == 1) lv_obj_add_state(cc->btn_hdp_pwr, LV_STATE_CHECKED);
	lv_obj_add_event_cb(cc->btn_hdp_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1);

	lv_obj_t * img_hdp = lv_img_create(cc->btn_hdp_pwr);
	lv_img_set_src(img_hdp, &control_headphone);
	lv_obj_align(img_hdp, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_img_recolor(img_hdp, lv_color_hex(0x333333), 0);
	lv_obj_set_style_img_recolor_opa(img_hdp, LV_OPA_COVER, 0);

	cc->btn_spk_pwr = lv_btn_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->btn_spk_pwr, 20, 20);
	lv_obj_set_pos(cc->btn_spk_pwr, btn_mid_x, 70);
	lv_obj_add_flag(cc->btn_spk_pwr, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_style(cc->btn_spk_pwr, &cc->style_btn_base, LV_PART_MAIN);
	lv_obj_add_style(cc->btn_spk_pwr, &cc->style_btn_checked, LV_STATE_CHECKED);
	if (g_max98357_ststus == 1) {lv_obj_add_state(cc->btn_spk_pwr, LV_STATE_CHECKED);}
	lv_obj_add_event_cb(cc->btn_spk_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2);

	lv_obj_t * img_spk = lv_img_create(cc->btn_spk_pwr);
	lv_img_set_src(img_spk, &control_speaker);
	lv_obj_align(img_spk, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_img_recolor(img_spk, lv_color_hex(0x333333), 0);
	lv_obj_set_style_img_recolor_opa(img_spk, LV_OPA_COVER, 0);

	// 6. 右侧三个进度条
	int slider_right_x = 70;
	int slider_w = 120;

	cc->slider_bright = lv_slider_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->slider_bright, slider_w, 20);
	lv_obj_set_pos(cc->slider_bright, slider_right_x, 10);
	lv_obj_set_ext_click_area(cc->slider_bright, 0);
	lv_slider_set_range(cc->slider_bright, 0, 255);
	lv_slider_set_value(cc->slider_bright, g_brightness, LV_ANIM_OFF);
	lv_obj_add_style(cc->slider_bright, &cc->style_slider_bg, LV_PART_MAIN);
	lv_obj_add_style(cc->slider_bright, &cc->style_slider_indic, LV_PART_INDICATOR);
	lv_obj_add_style(cc->slider_bright, &cc->style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
	lv_obj_set_style_bg_opa(cc->slider_bright, LV_OPA_TRANSP, LV_PART_KNOB);
	lv_obj_add_event_cb(cc->slider_bright, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0);

	cc->label_bright = lv_label_create(cc->slider_bright);
	lv_label_set_text_fmt(cc->label_bright, "%03d", g_brightness);
	lv_obj_set_style_text_color(cc->label_bright, lv_color_black(), LV_PART_MAIN);
	lv_obj_align(cc->label_bright, LV_ALIGN_RIGHT_MID, -5, 0);

	cc->slider_hdp = lv_slider_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->slider_hdp, slider_w, 20);
	lv_obj_set_pos(cc->slider_hdp, slider_right_x, 40);
	lv_obj_set_ext_click_area(cc->slider_hdp, 0);
	lv_slider_set_range(cc->slider_hdp, 0, 255);
	lv_slider_set_value(cc->slider_hdp, g_hdp_value, LV_ANIM_OFF);
	lv_obj_add_style(cc->slider_hdp, &cc->style_slider_bg, LV_PART_MAIN);
	lv_obj_add_style(cc->slider_hdp, &cc->style_slider_indic, LV_PART_INDICATOR);
	lv_obj_add_style(cc->slider_hdp, &cc->style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
	lv_obj_set_style_bg_opa(cc->slider_hdp, LV_OPA_TRANSP, LV_PART_KNOB);
	lv_obj_add_event_cb(cc->slider_hdp, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1);

	cc->label_hdp = lv_label_create(cc->slider_hdp);
	lv_label_set_text_fmt(cc->label_hdp, "%03d", g_hdp_value);
	lv_obj_set_style_text_color(cc->label_hdp, lv_color_black(), LV_PART_MAIN);
	lv_obj_align(cc->label_hdp, LV_ALIGN_RIGHT_MID, -5, 0);

	cc->slider_spk = lv_slider_create(cc->ctrl_center_cont);
	lv_obj_set_size(cc->slider_spk, slider_w, 20);
	lv_obj_set_pos(cc->slider_spk, slider_right_x, 70);
	lv_obj_set_ext_click_area(cc->slider_spk, 0);
	lv_slider_set_range(cc->slider_spk, 0, 255);
	lv_slider_set_value(cc->slider_spk, g_spk_value, LV_ANIM_OFF);
	lv_obj_add_style(cc->slider_spk, &cc->style_slider_bg, LV_PART_MAIN);
	lv_obj_add_style(cc->slider_spk, &cc->style_slider_indic, LV_PART_INDICATOR);
	lv_obj_add_style(cc->slider_spk, &cc->style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
	lv_obj_set_style_bg_opa(cc->slider_spk, LV_OPA_TRANSP, LV_PART_KNOB);
	lv_obj_add_event_cb(cc->slider_spk, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2);

	cc->label_spk = lv_label_create(cc->slider_spk);
	lv_label_set_text_fmt(cc->label_spk, "%03d", g_spk_value);
	lv_obj_set_style_text_color(cc->label_spk, lv_color_black(), LV_PART_MAIN);
	lv_obj_align(cc->label_spk, LV_ALIGN_RIGHT_MID, -5, 0);

	update_controls_state();
}

void Remove_Control_Center(void)
{
	if (cc == NULL) return;

	if (cc->ctrl_center_cont != NULL) {
		lv_obj_del(cc->ctrl_center_cont);
	}
	if (cc->bg_cover != NULL) {
		lv_obj_del(cc->bg_cover);
	}

	free_ccm(cc);
	cc = NULL;
}

void Update_Control_Center(void)
{
}

// ---------------- 内部回调与状态更新实现 ----------------

static void bg_cover_event_cb(lv_event_t * e)
{
	Remove_Control_Center();
}

static void update_controls_state(void)
{
	if (lv_obj_has_state(cc->btn_bright_pwr, LV_STATE_CHECKED)) {
		lv_obj_clear_state(cc->slider_bright, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(cc->slider_bright, LV_STATE_DISABLED);
	}

	if (lv_obj_has_state(cc->btn_hdp_pwr, LV_STATE_CHECKED)) {
		lv_obj_clear_state(cc->slider_hdp, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(cc->slider_hdp, LV_STATE_DISABLED);
	}

	if (lv_obj_has_state(cc->btn_spk_pwr, LV_STATE_CHECKED)) {
		lv_obj_clear_state(cc->slider_spk, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(cc->slider_spk, LV_STATE_DISABLED);
	}
}

static void btn_close_event_cb(lv_event_t * e)
{
	Remove_Control_Center();
}

static void custom_switch_event_cb(lv_event_t * e)
{
	if (g_hdp0_or_spk1 == 0) {
		I2S_Exchange_Ctrl(1);
		lv_obj_align(cc->switch_knob, LV_ALIGN_BOTTOM_MID, 0, 0);
	} else {
		I2S_Exchange_Ctrl(0);
		lv_obj_align(cc->switch_knob, LV_ALIGN_TOP_MID, 0, 0);
	}
}

static void btn_pwr_event_cb(lv_event_t * e)
{
	int type = (int)(intptr_t)lv_event_get_user_data(e);
	lv_obj_t * btn = lv_event_get_target(e);
	bool is_on = lv_obj_has_state(btn, LV_STATE_CHECKED);

	switch(type) {
		case 0:
			if(is_on) g_screen_status = 1; else g_screen_status = 0;
			break;
		case 1:
			if(is_on) g_es9018_status = 1; else g_es9018_status = 0;
			break;
		case 2:
			if(is_on) g_max98357_ststus = 1; else g_max98357_ststus = 0;
			break;
	}

	update_controls_state();
}

static void slider_value_event_cb(lv_event_t * e)
{
	int type = (int)(intptr_t)lv_event_get_user_data(e);
	lv_obj_t * slider = lv_event_get_target(e);
	uint8_t val = (uint8_t)lv_slider_get_value(slider);

	switch(type) {
		case 0:
			g_brightness = val;
			lv_label_set_text_fmt(cc->label_bright, "%03d", g_brightness);
			break;
		case 1:
			g_hdp_value = val;
			lv_label_set_text_fmt(cc->label_hdp, "%03d", g_hdp_value);
			break;
		case 2:
			g_spk_value = val;
			lv_label_set_text_fmt(cc->label_spk, "%03d", g_spk_value);
			break;
	}
}
