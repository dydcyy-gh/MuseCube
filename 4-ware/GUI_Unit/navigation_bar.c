#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "variables.h"
#include "icon_unit.h"
#include "page_manager.h"

static lv_obj_t *navigation_bar = NULL;

extern const lv_img_dsc_t exit_icon;
extern const lv_img_dsc_t minimize_icon;
extern const lv_img_dsc_t close_icon;

// Empty callback functions for the three icons
static void exit_icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        Page_Back();
    }
}

static void minimize_icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        Page_Request_Switch(PAGE_DESKTOP); 
    }
}

static void close_icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        Page_Request_Switch(PAGE_DESKTOP); 
    }
}

void Create_Navigation_Bar(const char* title)
{
    if (navigation_bar != NULL) return;

    // Create Navigation Bar Container
    // Position: (0,0), Size: 240x30
    navigation_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(navigation_bar, 240, 30);
    lv_obj_set_pos(navigation_bar, 0, 0);
    lv_obj_set_style_radius(navigation_bar, 0, 0);
    lv_obj_set_style_border_width(navigation_bar, 0, 0);
    lv_obj_set_style_pad_all(navigation_bar, 0, 0);
    lv_obj_clear_flag(navigation_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Left Icon (Exit_Icon)
    // Click area: (0,0) size 30x30
    lv_obj_t * btn_left = lv_obj_create(navigation_bar);
    lv_obj_set_size(btn_left, 30, 30);
    lv_obj_set_pos(btn_left, 0, 0);
    lv_obj_set_style_bg_opa(btn_left, LV_OPA_TRANSP, 0); // Transparent
    lv_obj_set_style_border_width(btn_left, 0, 0);
    lv_obj_clear_flag(btn_left, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(btn_left, 0, 0); 
    lv_obj_add_event_cb(btn_left, exit_icon_event_cb, LV_EVENT_CLICKED, NULL);

    // Image Placement: (5,5) relative to bar (so 5,5 inside the 0,0 button)
    lv_obj_t * img_exit = lv_img_create(btn_left);
    lv_img_set_src(img_exit, &exit_icon);
    lv_obj_set_pos(img_exit, 5, 5);
	lv_obj_set_style_img_recolor(img_exit, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(img_exit, LV_OPA_COVER, 0);

    // 2. Text Container
    // Position: (35,5), Size: 140x20
    lv_obj_t * text_cont = lv_obj_create(navigation_bar);
    lv_obj_set_size(text_cont, 140, 20);
    lv_obj_set_pos(text_cont, 35, 5);
    lv_obj_set_style_bg_opa(text_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_cont, 0, 0);
    lv_obj_set_style_pad_all(text_cont, 0, 0);
    lv_obj_clear_flag(text_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Text Label: Centered vertically, aligned left
    lv_obj_t * label = lv_label_create(text_cont);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_16, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    // 3. Minimize Icon (Minimize_Icon)
    // Click area: (180,0) size 30x30
    lv_obj_t * btn_minimize = lv_obj_create(navigation_bar);
    lv_obj_set_size(btn_minimize, 30, 30);
    lv_obj_set_pos(btn_minimize, 180, 0);
    lv_obj_set_style_bg_opa(btn_minimize, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_minimize, 0, 0);
    lv_obj_clear_flag(btn_minimize, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(btn_minimize, 0, 0);
    lv_obj_add_event_cb(btn_minimize, minimize_icon_event_cb, LV_EVENT_CLICKED, NULL);

    // Image Placement: (185,5) relative to bar (so 5,5 inside the 180,0 button)
    lv_obj_t * img_minimize = lv_img_create(btn_minimize);
    lv_img_set_src(img_minimize, &minimize_icon);
    lv_obj_set_pos(img_minimize, 5, 5);
	lv_obj_set_style_img_recolor(img_minimize, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(img_minimize, LV_OPA_COVER, 0);
	
    // 4. Right Icon (Exit_Icon)
    // Click area: (210,0) size 30x30
    lv_obj_t * btn_right = lv_obj_create(navigation_bar);
    lv_obj_set_size(btn_right, 30, 30);
    lv_obj_set_pos(btn_right, 210, 0);
    lv_obj_set_style_bg_opa(btn_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_right, 0, 0);
    lv_obj_clear_flag(btn_right, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(btn_right, 0, 0);
    lv_obj_add_event_cb(btn_right, close_icon_event_cb, LV_EVENT_CLICKED, NULL);

    // Image Placement: (215,5) relative to bar (so 5,5 inside the 210,0 button)
    lv_obj_t * img_close = lv_img_create(btn_right);
    lv_img_set_src(img_close, &close_icon);
    lv_obj_set_pos(img_close, 5, 5);
	lv_obj_set_style_img_recolor(img_close, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(img_close, LV_OPA_COVER, 0);
	
	// 在 y=30-1 的位置绘制一条水平直线
    static lv_obj_t *line = NULL;
	static lv_point_t line_points[] = {{0, 29},{240, 29}};
	
	line = lv_line_create(navigation_bar);
	lv_line_set_points(line, line_points, 2);
	
	// 设置直线样式
	lv_obj_set_style_line_width(line, 1, 0);  // 线宽为1像素
	lv_obj_set_style_line_color(line, lv_color_hex(0x000000), 0);  // 黑色
	lv_obj_set_style_line_opa(line, LV_OPA_50, 0);  // 完全不透明
}

void Update_Navigation_Bar(void)
{
    return;
}

void Remove_Navigation_Bar(void)
{
    if (navigation_bar) {
        lv_obj_del(navigation_bar);
        navigation_bar = NULL;
    }
}
