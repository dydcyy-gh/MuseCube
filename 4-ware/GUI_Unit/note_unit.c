#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "keyboard.h"
#include "malloc.h"

// --- 状态结构体 ---
typedef struct {
	lv_obj_t * note_cont;
	lv_obj_t * ta_note;
	lv_style_t style_note_bg;
} note_state_t;

static note_state_t *ns = NULL;

// 内部函数声明
static void note_ta_event_cb(lv_event_t * e);

// 初始化样式
static void init_custom_styles(void)
{
	lv_style_init(&ns->style_note_bg);
	lv_style_set_radius(&ns->style_note_bg, 4);
	lv_style_set_border_width(&ns->style_note_bg, 1);
	lv_style_set_border_color(&ns->style_note_bg, lv_color_black());
	lv_style_set_bg_color(&ns->style_note_bg, lv_color_white());
	lv_style_set_bg_opa(&ns->style_note_bg, LV_OPA_100);
	lv_style_set_pad_all(&ns->style_note_bg, 8);
	lv_style_set_text_color(&ns->style_note_bg, lv_color_black());
}

void Create_Note_Unit(void)
{
	if (ns != NULL) return;

	ns = (note_state_t *)malloc_ccm(sizeof(note_state_t));
	if (!ns) return;
	memset(ns, 0, sizeof(note_state_t));

	init_custom_styles();

	ns->note_cont = lv_obj_create(lv_scr_act());
	lv_obj_set_size(ns->note_cont, 240, 180);
	lv_obj_center(ns->note_cont);
	lv_obj_clear_flag(ns->note_cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(ns->note_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ns->note_cont, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ns->note_cont, lv_color_hex(0xE8E8E8), LV_PART_MAIN);

	ns->ta_note = lv_textarea_create(ns->note_cont);
	lv_obj_set_size(ns->ta_note, 220, 160);
	lv_obj_align(ns->ta_note, LV_ALIGN_CENTER, 0, 0);

	lv_textarea_set_placeholder_text(ns->ta_note, "在这里输入笔记（支持英文+数字）...");
	lv_textarea_set_one_line(ns->ta_note, false);
	lv_textarea_set_max_length(ns->ta_note, 1024);
	lv_textarea_set_cursor_click_pos(ns->ta_note, true);

	lv_obj_add_style(ns->ta_note, &ns->style_note_bg, LV_PART_MAIN);
	lv_obj_add_event_cb(ns->ta_note, note_ta_event_cb, LV_EVENT_ALL, NULL);

	lv_obj_add_state(ns->ta_note, LV_STATE_FOCUSED);
}

void Update_Note_Unit(void)
{
	if (ns == NULL) return;
}

void Remove_Note_Unit(void)
{
	if (ns == NULL) return;

	if (ns->note_cont != NULL) {
		lv_obj_del(ns->note_cont);
	}

	free_ccm(ns);
	ns = NULL;
}

// ---------------- 内部回调 ----------------
static void note_ta_event_cb(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * ta = lv_event_get_target(e);

	if (code == LV_EVENT_FOCUSED) {
		Create_Keyboard(ta);
	}
}
