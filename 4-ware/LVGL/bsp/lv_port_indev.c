#include "lv_port_indev.h"
#include "lvgl.h"
#include "adc.h"
#include "key.h"
#include "variables.h"
#include <math.h>
#include <stdbool.h>
#include "page_manager.h"

#define HOR_RES      240  /* 屏幕宽度 */
#define VER_RES      240  /* 屏幕高度 */

extern volatile int16_t g_usb_mouse_dx;
extern volatile int16_t g_usb_mouse_dy;
extern volatile uint8_t g_usb_mouse_btn;

static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

lv_indev_t * indev_mouse;
static lv_obj_t * mouse_cursor; 

float cursor_x = HOR_RES / 2.0f;
float cursor_y = VER_RES / 2.0f; 

static lv_indev_drv_t indev_drv;

/* ========================================================================= */
/* ======================== 智能上下文探测引擎 ============================== */
/* ========================================================================= */

// 递归寻找当前坐标下，最深层的可点击对象
static lv_obj_t * get_clickable_obj_under_point(lv_obj_t * obj, lv_point_t * p) 
{
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return NULL; // 忽略隐藏对象
    
    // 边界检查：如果坐标不在本对象的物理包围���内，直接淘汰
    if(p->x < obj->coords.x1 || p->x > obj->coords.x2 || 
       p->y < obj->coords.y1 || p->y > obj->coords.y2) {
        return NULL;
    }
    
    // 递归检查子对象 (倒序遍历，因为后添加的 UI 在最上层)
    uint32_t child_cnt = lv_obj_get_child_cnt(obj);
    for(int32_t i = child_cnt - 1; i >= 0; i--) {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        lv_obj_t * res = get_clickable_obj_under_point(child, p);
        if(res != NULL) return res; // 如果命中子对象，直接返回
    }
    
    // 如果子对象没命中，且本对象是可点击的，则返回本对象
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
        return obj;
    }
    
    return NULL;
}

// 核心判断：当前坐标点击下去，最终会不会触发滚动？
static bool check_is_scrollable(lv_coord_t x, lv_coord_t y) 
{
    lv_point_t p = {x, y};
    lv_obj_t * target = NULL;
    
    // 按系统层级从上到下透视检索（跳过鼠标小黑点，因为它没有 CLICKABLE 标志）
    target = get_clickable_obj_under_point(lv_layer_sys(), &p);
    if(!target) target = get_clickable_obj_under_point(lv_layer_top(), &p);
    if(!target) target = get_clickable_obj_under_point(lv_scr_act(), &p);
    
    // 如果找到了目标对象，沿父节点向上追溯（LVGL 的事件冒泡机制）
    // 只要有任何一个父容器是可滚动的，那么当前手势就会变成滚动！
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
	
	/* 极度重要：小黑点绝对不能拦截点击事件 */
	lv_obj_clear_flag(mouse_cursor, LV_OBJ_FLAG_CLICKABLE); 
}

static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /* --------- 1. 读取并清空 USB 鼠标数据 --------- */
    int16_t usb_dx = g_usb_mouse_dx; g_usb_mouse_dx = 0;
    int16_t usb_dy = g_usb_mouse_dy; g_usb_mouse_dy = 0;
    uint8_t usb_btn = g_usb_mouse_btn;

    /* --------- 2. 右摇杆：永远控制绝对坐标 --------- */
    float move_x = (g_key_R_X * 0.06f) + usb_dx;
    float move_y = -(g_key_R_Y * 0.06f) + usb_dy; 

    cursor_x += move_x;
    cursor_y += move_y;

    if(cursor_x < 0) cursor_x = 0;
    if(cursor_y < 0) cursor_y = 0;
    if(cursor_x > HOR_RES - 1) cursor_x = HOR_RES - 1;
    if(cursor_y > VER_RES - 1) cursor_y = VER_RES - 1;

    /* --------- 3. 左摇杆：智能感知上下文 (核心魔法) --------- */
    static float virtual_drag_x = 0;
    static float virtual_drag_y = 0;
    static bool last_is_pressed = false; 
    static bool is_scroll_mode = false; // 锁定当前拨动的“模式”
    
    bool left_joy_active = (g_key_L_X > 80 || g_key_L_X < -80 || g_key_L_Y > 80 || g_key_L_Y < -80);
    bool is_pressed = left_joy_active || (usb_btn & 0x01);

    // 【核心逻辑】：在按下的第一帧，感知脚下的土地
    if (is_pressed && !last_is_pressed) {
        is_scroll_mode = check_is_scrollable((lv_coord_t)cursor_x, (lv_coord_t)cursor_y);
    }

    if (left_joy_active) {
        if (is_scroll_mode) {
            // 是滚动区域：允许坐标累加，触发拖拽/滚动
            virtual_drag_x += (g_key_L_X * 0.05f);
            virtual_drag_y += (g_key_L_Y * 0.05f); 
        }
        // 如果不是滚动区域，什么都不做！virtual_drag 死死钉在 0 上！
        // 此时拨动左摇杆，只会产生完美的“原位按下”！
    } else {
        if (!last_is_pressed) {
            virtual_drag_x = 0;
            virtual_drag_y = 0;
        }
    }
	
	/* --------- 4. 左右摇杆按键     --------- */
	static uint8_t last_L = 0;
	
	if(!g_key_L_M_RT && last_L)
	{
		//Page_Back();
	}
	last_L = g_key_L_M_RT;
	
    /* --------- 5. 提交给 LVGL 底层 --------- */
    data->point.x = (lv_coord_t)(cursor_x + virtual_drag_x);
    data->point.y = (lv_coord_t)(cursor_y + virtual_drag_y);
    data->state = is_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    last_is_pressed = is_pressed;

    /* --------- 6. 实时更新屏幕上的鼠标小黑点 --------- */
    lv_obj_set_pos(mouse_cursor, (lv_coord_t)cursor_x - 4, (lv_coord_t)cursor_y - 4);
}
