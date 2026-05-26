#include "lv_port_indev.h"
#include "lvgl.h"
#include "adc.h"
#include "key.h"
#include "variables.h"
#include <math.h>
#include <stdbool.h>
#include "page_manager.h"
#include "debug.h"
#include "variables.h"

#define HOR_RES      240  /* 屏幕宽度 */
#define VER_RES      240  /* 屏幕高度 */

static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

lv_indev_t * indev_mouse;
static lv_obj_t * mouse_cursor; 

float cursor_x = HOR_RES / 2.0f;
float cursor_y = VER_RES / 2.0f; 

static lv_indev_drv_t indev_drv;

/* ========================================================================= */
/* ======================== 智能上下文探测引擎 ============================== */
/* ========================================================================= */

static lv_obj_t * get_clickable_obj_under_point(lv_obj_t * obj, lv_point_t * p) 
{
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return NULL; 
    if(p->x < obj->coords.x1 || p->x > obj->coords.x2 || 
       p->y < obj->coords.y1 || p->y > obj->coords.y2) {
        return NULL;
    }
    uint32_t child_cnt = lv_obj_get_child_cnt(obj);
    for(int32_t i = child_cnt - 1; i >= 0; i--) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        lv_obj_t * res = get_clickable_obj_under_point(child, p);
        if(res != NULL) return res; 
    }
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
        return obj;
    }
    return NULL;
}

static bool check_is_scrollable(lv_coord_t x, lv_coord_t y) 
{
    lv_point_t p = {x, y};
    lv_obj_t * target = NULL;
    
    target = get_clickable_obj_under_point(lv_layer_sys(), &p);
    if(!target) target = get_clickable_obj_under_point(lv_layer_top(), &p);
    if(!target) target = get_clickable_obj_under_point(lv_scr_act(), &p);
    
    while(target != NULL) {
        if(lv_obj_has_flag(target, LV_OBJ_FLAG_SCROLLABLE)) {
            return true;
        }
        target = lv_obj_get_parent(target);
    }
    return false;
}

/* ========================================================================= */

void lv_port_indev_init(void)
{
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = mouse_read;
	indev_mouse = lv_indev_drv_register(&indev_drv);

	mouse_cursor = lv_obj_create(lv_layer_sys());
	lv_obj_set_size(mouse_cursor, 9, 9);
	lv_obj_set_style_radius(mouse_cursor, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_opa(mouse_cursor, LV_OPA_0, 0); 
	lv_obj_set_style_border_width(mouse_cursor, 1, 0);  
	lv_obj_set_style_border_color(mouse_cursor, lv_color_black(), 0); 
	
	lv_obj_clear_flag(mouse_cursor, LV_OBJ_FLAG_CLICKABLE); 
}

static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    if (g_lvgl_input_disabled) 
	{
		static uint8_t last_R = 0;
        if (last_R && !g_key_R_M_RT) 
		{
            g_lvgl_input_disabled = 0;
			last_R = 0;
        } 
		else 
		{
			last_R = g_key_R_M_RT;
            data->state = LV_INDEV_STATE_REL;
            return;
        }
    }
	
    int16_t usb_dx = g_usb_mouse_dx; g_usb_mouse_dx = 0;
    int16_t usb_dy = g_usb_mouse_dy; g_usb_mouse_dy = 0;
    uint8_t usb_btn = g_usb_mouse_btn;

    /* --------- 2. 右摇杆：融合板载ADC与USB手柄 --------- */
    // 注意 move_y 是 -(Y轴)，因为屏幕坐标系的 y 向下是递增的
    float move_x = (g_key_R_X * 0.06f) + (g_usb_joy_R_X * 0.06f) + usb_dx;
    float move_y = -(g_key_R_Y * 0.06f) - (g_usb_joy_R_Y * 0.06f) + usb_dy; 

    cursor_x += move_x;
    cursor_y += move_y;

    if(cursor_x < 0) cursor_x = 0;
    if(cursor_y < 0) cursor_y = 0;
    if(cursor_x > HOR_RES - 1) cursor_x = HOR_RES - 1;
    if(cursor_y > VER_RES - 1) cursor_y = VER_RES - 1;

    /* --------- 3. 左摇杆：融合板载ADC与USB手柄 --------- */
    int16_t combined_L_X = g_key_L_X + g_usb_joy_L_X;
    int16_t combined_L_Y = g_key_L_Y + g_usb_joy_L_Y;
    
    // 防止两个摇杆同时推到最大导致溢出 (-128 ~ 127)
    if(combined_L_X > 127) combined_L_X = 127;
    if(combined_L_X < -128) combined_L_X = -128;
    if(combined_L_Y > 127) combined_L_Y = 127;
    if(combined_L_Y < -128) combined_L_Y = -128;

    static float virtual_drag_x = 0;
    static float virtual_drag_y = 0;
    static bool last_is_pressed = false; 
    static bool is_scroll_mode = false; 
    
    bool left_joy_active = (combined_L_X > 80 || combined_L_X < -80 || combined_L_Y > 80 || combined_L_Y < -80);
    bool is_pressed = left_joy_active || (usb_btn & 0x01);

    if (is_pressed && !last_is_pressed) {
        is_scroll_mode = check_is_scrollable((lv_coord_t)cursor_x, (lv_coord_t)cursor_y);
    }

    if (left_joy_active) {
        if (is_scroll_mode) {
            virtual_drag_x += (combined_L_X * 0.08f);
            virtual_drag_y += (combined_L_Y * 0.08f); 
        }
    } else {
        if (!last_is_pressed) {
            virtual_drag_x = 0;
            virtual_drag_y = 0;
        }
    }
	
	/* --------- 4. 左右摇杆按键 --------- */
	static uint8_t last_L = 0;
	if(!g_key_L_M_RT && last_L)
	{
		Page_Back();
	}
	last_L = g_key_L_M_RT;
	
	static uint8_t last_M = 0;
	if(!g_key_WKP_RT && last_M)
	{
		debug_printf("hello world\n");
	}
	last_M = g_key_WKP_RT;
	
    /* --------- 5. 提交给 LVGL 底层 --------- */
    data->point.x = (lv_coord_t)(cursor_x + virtual_drag_x);
    data->point.y = (lv_coord_t)(cursor_y + virtual_drag_y);
    data->state = is_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    last_is_pressed = is_pressed;

    /* --------- 6. 实时更新屏幕上的鼠标小黑点 --------- */
    lv_obj_set_pos(mouse_cursor, (lv_coord_t)cursor_x - 4, (lv_coord_t)cursor_y - 4);
}
