#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "adc.h"
#include "key.h"
#include "variables.h"

// --- 左摇杆对象 ---
static lv_obj_t *stick_L_panel;  // 左摇杆总面板
static lv_obj_t *stick_L_cont;   // 左摇杆圆环区域
static lv_obj_t *stick_L_cursor; // 左摇杆位置光标/圆点
static lv_obj_t *stick_L_label;  // 坐标文本标签
static lv_obj_t *stick_L_line_h; // 水平线
static lv_obj_t *stick_L_line_v; // 垂直线

void Create_Stick_Label_L(void)
{
    // 1. 创建总面板容器 (100x130, 起点15,30)
    stick_L_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(stick_L_panel, 100, 130);
    lv_obj_set_pos(stick_L_panel, 15, 30);
    lv_obj_set_style_pad_all(stick_L_panel, 0, 0); 
    lv_obj_set_style_radius(stick_L_panel, 0, 0);
    lv_obj_set_style_border_width(stick_L_panel, 0, 0); // 无边框
    lv_obj_set_style_bg_opa(stick_L_panel, LV_OPA_0, 0); // 透明背景
    lv_obj_clear_flag(stick_L_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 顶部标题区域 (100x15, 淡灰色背景, "左侧摇杆")
    lv_obj_t * header = lv_obj_create(stick_L_panel);
    lv_obj_set_size(header, 100, 15);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(header, LV_OPA_0, 0);
    
    lv_obj_t * header_lbl = lv_label_create(header);
    lv_label_set_text(header_lbl, "[---左摇杆---]");
    lv_obj_center(header_lbl);

    // 3. 摇杆圆环区域 (90x90, 绝对坐标20,50 -> 相对面板(5,20))
    stick_L_cont = lv_obj_create(stick_L_panel);
    lv_obj_set_size(stick_L_cont, 90, 90);
    lv_obj_set_pos(stick_L_cont, 5, 20);
    lv_obj_set_style_radius(stick_L_cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(stick_L_cont, LV_OPA_0, 0); 
    lv_obj_set_style_border_width(stick_L_cont, 2, 0); 
    lv_obj_set_style_border_color(stick_L_cont, lv_palette_main(LV_PALETTE_GREY), 0); 
    lv_obj_clear_flag(stick_L_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(stick_L_cont, 0, 0);
	
    // 创建水平线 - 相对圆环坐标 (0,45) -> (90,45)
    stick_L_line_h = lv_line_create(stick_L_cont);
    static lv_point_t line_h_points[] = {{0, 43}, {90, 43}}; 
    lv_line_set_points(stick_L_line_h, line_h_points, 2);
    lv_obj_set_style_line_width(stick_L_line_h, 1, 0);
    lv_obj_set_style_line_color(stick_L_line_h, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_line_opa(stick_L_line_h, LV_OPA_50, 0); 
    
    // 创建垂直线 - 相对圆环坐标 (45,0) -> (45,90)
    stick_L_line_v = lv_line_create(stick_L_cont);
    static lv_point_t line_v_points[] = {{43, 0}, {43, 90}}; 
    lv_line_set_points(stick_L_line_v, line_v_points, 2);
    lv_obj_set_style_line_width(stick_L_line_v, 1, 0);
    lv_obj_set_style_line_color(stick_L_line_v, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_line_opa(stick_L_line_v, LV_OPA_50, 0); 
    
    // 创建光标（默认灰色）
    stick_L_cursor = lv_obj_create(stick_L_cont);
    lv_obj_set_size(stick_L_cursor, 12, 12);
    lv_obj_set_style_radius(stick_L_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(stick_L_cursor, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_center(stick_L_cursor); 
    lv_obj_set_style_border_width(stick_L_cursor, 0, 0);
    lv_obj_set_style_shadow_width(stick_L_cursor, 0, 0);

    // 4. 底部数据显示区域 (100x15, 淡灰色背景, "X:+000 Y:+000")
    lv_obj_t * footer = lv_obj_create(stick_L_panel);
    lv_obj_set_size(footer, 100, 15);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(footer, LV_OPA_0, 0);
	
    stick_L_label = lv_label_create(footer);
    lv_label_set_text(stick_L_label, "X:+000 Y:+000");
    lv_obj_center(stick_L_label);
}

void Update_Stick_Label_L(void)
{
    if(stick_L_cont == NULL || stick_L_cursor == NULL) return;

    // 调整比例因子以适应90像素圆环
    int8_t x_offset = g_key_L_X / 4; 
    int8_t y_offset = -g_key_L_Y / 4;
	
    lv_obj_align(stick_L_cursor, LV_ALIGN_CENTER, x_offset, y_offset);

    // 如果按下（L_M），则改变颜色（按下淡蓝，未按下淡灰）
    if(g_key_L_M_RT) 
        lv_obj_set_style_bg_color(stick_L_cursor, lv_palette_main(LV_PALETTE_BLUE), 0);
    else 
        lv_obj_set_style_bg_color(stick_L_cursor, lv_palette_main(LV_PALETTE_GREY), 0);

    // 更新文本格式 (去除Key部分)
    lv_label_set_text_fmt(stick_L_label, "X:%+04d Y:%+04d", g_key_L_X, g_key_L_Y);
}

void Remove_Stick_Label_L(void)
{
    if(stick_L_panel) {
        lv_obj_del(stick_L_panel);
        stick_L_panel = NULL;
        stick_L_cont = NULL;
        stick_L_cursor = NULL;
        stick_L_line_h = NULL;
        stick_L_line_v = NULL;
        stick_L_label = NULL;
    }
}

// --- 右摇杆对象 ---
static lv_obj_t *stick_R_panel;
static lv_obj_t *stick_R_cont;
static lv_obj_t *stick_R_cursor;
static lv_obj_t *stick_R_label;
static lv_obj_t *stick_R_line_h; 
static lv_obj_t *stick_R_line_v; 

void Create_Stick_Label_R(void)
{
    // 1. 创建总面板容器 (100x130, 起点125,30)
    stick_R_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(stick_R_panel, 100, 130);
    lv_obj_set_pos(stick_R_panel, 125, 30);
    lv_obj_set_style_pad_all(stick_R_panel, 0, 0);
    lv_obj_set_style_radius(stick_R_panel, 0, 0);
    lv_obj_set_style_border_width(stick_R_panel, 0, 0);
    lv_obj_set_style_bg_opa(stick_R_panel, LV_OPA_0, 0); 
    lv_obj_clear_flag(stick_R_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 顶部标题区域 (100x15, 淡灰色背景, "右侧摇杆")
    lv_obj_t * header = lv_obj_create(stick_R_panel);
    lv_obj_set_size(header, 100, 15);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(header, LV_OPA_0, 0);
	
    lv_obj_t * header_lbl = lv_label_create(header);
    lv_label_set_text(header_lbl, "[---右摇杆---]");
    lv_obj_center(header_lbl);
	
    // 3. 摇杆圆环区域 (90x90, 绝对坐标130,50 -> 相对面板(5,20))
    stick_R_cont = lv_obj_create(stick_R_panel);
    lv_obj_set_size(stick_R_cont, 90, 90);
    lv_obj_set_pos(stick_R_cont, 5, 20); 
    lv_obj_set_style_radius(stick_R_cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(stick_R_cont, LV_OPA_0, 0); 
    lv_obj_set_style_border_width(stick_R_cont, 2, 0); 
    lv_obj_set_style_border_color(stick_R_cont, lv_palette_main(LV_PALETTE_GREY), 0); 
    lv_obj_clear_flag(stick_R_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(stick_R_cont, 0, 0);
	
    // 创建水平线 - 相对圆环坐标 (0,45) -> (90,45)
    stick_R_line_h = lv_line_create(stick_R_cont);
    static lv_point_t line_h_points_r[] = {{0, 43}, {90, 43}}; 
    lv_line_set_points(stick_R_line_h, line_h_points_r, 2);
    lv_obj_set_style_line_width(stick_R_line_h, 1, 0);
    lv_obj_set_style_line_color(stick_R_line_h, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_line_opa(stick_R_line_h, LV_OPA_50, 0); 
    
    // 创建垂直线 - 相对圆环坐标 (45,0) -> (45,90)
    stick_R_line_v = lv_line_create(stick_R_cont);
    static lv_point_t line_v_points_r[] = {{43, 0}, {43, 90}}; 
    lv_line_set_points(stick_R_line_v, line_v_points_r, 2);
    lv_obj_set_style_line_width(stick_R_line_v, 1, 0);
    lv_obj_set_style_line_color(stick_R_line_v, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_line_opa(stick_R_line_v, LV_OPA_50, 0); 
    
    // 创建光标（默认灰色）
    stick_R_cursor = lv_obj_create(stick_R_cont);
    lv_obj_set_size(stick_R_cursor, 12, 12);
    lv_obj_set_style_radius(stick_R_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(stick_R_cursor, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_center(stick_R_cursor);
    lv_obj_set_style_border_width(stick_R_cursor, 0, 0);
    lv_obj_set_style_shadow_width(stick_R_cursor, 0, 0);

    // 4. 底部数据显示区域 (100x15, 淡灰色背景, "X:+000 Y:+000")
    lv_obj_t * footer = lv_obj_create(stick_R_panel);
    lv_obj_set_size(footer, 100, 15);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(footer, LV_OPA_0, 0);
	
    stick_R_label = lv_label_create(footer);
    lv_label_set_text(stick_R_label, "X:+000 Y:+000");
    lv_obj_center(stick_R_label);
}

void Update_Stick_Label_R(void)
{
    if(stick_R_cont == NULL || stick_R_cursor == NULL) return;

    // 调整比例因子以适应90像素圆环
    int8_t x_offset = g_key_R_X / 3; 
    int8_t y_offset = g_key_R_Y / 3;
	
    lv_obj_align(stick_R_cursor, LV_ALIGN_CENTER, x_offset, y_offset);

    // 如果按下（R_M），则改变颜色（按下淡蓝，未按下淡灰）
    if(g_key_R_M_RT) 
        lv_obj_set_style_bg_color(stick_R_cursor, lv_palette_main(LV_PALETTE_BLUE), 0);
    else 
        lv_obj_set_style_bg_color(stick_R_cursor, lv_palette_main(LV_PALETTE_GREY), 0);

    // 更新文本格式 (去除Key部分)
    lv_label_set_text_fmt(stick_R_label, "X:%+04d Y:%+04d", g_key_R_X, g_key_R_Y);
}

void Remove_Stick_Label_R(void)
{
    if(stick_R_panel) {
        lv_obj_del(stick_R_panel);
        stick_R_panel = NULL;
        stick_R_cont = NULL;
        stick_R_cursor = NULL;
        stick_R_line_h = NULL;
        stick_R_line_v = NULL;
        stick_R_label = NULL;
    }
}

// --- 按钮对象 ---
// 定义三个按键的组件指针
static lv_obj_t *btn_L_cont;
static lv_obj_t *btn_L_ind;
static lv_obj_t *btn_L_lbl_status;

static lv_obj_t *btn_M_cont;
static lv_obj_t *btn_M_ind;
static lv_obj_t *btn_M_lbl_status;

static lv_obj_t *btn_R_cont;
static lv_obj_t *btn_R_ind;
static lv_obj_t *btn_R_lbl_status;

// 按键上一次的状态（用于优化更新）
static uint8_t btn_L_last_state = 0;
static uint8_t btn_M_last_state = 0;
static uint8_t btn_R_last_state = 0;

// 辅助函数：创建单个按键UI
// x,y: 坐标
// title: 标题 ("左键:", "中键:", "右键:")
// cont_out: 输出容器指针
// ind_out: 输出指示器指针
// lbl_status_out: 输出状态标签指针
static void create_single_key_ui(lv_coord_t x, lv_coord_t y, const char* title, 
                                 lv_obj_t **cont_out, lv_obj_t **ind_out, lv_obj_t **lbl_status_out)
{
    // 容器 60x40
    lv_obj_t * cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 60, 40);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0); // 透明
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    *cont_out = cont;

    // 标题 Label (左上角)
    lv_obj_t * lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_pos(lbl_title, 0, 5);
    lv_obj_set_style_text_font(lbl_title, LV_FONT_DEFAULT, 0); // 默认字体

    // 指示器方块 50x20, 圆角4（默认灰色）
    lv_obj_t * ind = lv_obj_create(cont);
    lv_obj_set_size(ind, 50, 20);
    lv_obj_align(ind, LV_ALIGN_BOTTOM_MID, 0, 0); // 底部居中
    lv_obj_set_style_radius(ind, 4, 0);
    lv_obj_set_style_border_width(ind, 0, 0);
    lv_obj_set_style_bg_color(ind, lv_palette_lighten(LV_PALETTE_GREY, 1), 0); // 默认灰色
    lv_obj_clear_flag(ind, LV_OBJ_FLAG_SCROLLABLE);
    *ind_out = ind;

    // 状态文字 (居中显示"已/未按下")
    lv_obj_t * lbl_st = lv_label_create(ind);
    lv_label_set_text(lbl_st, "未按下");
    lv_obj_center(lbl_st);
    *lbl_status_out = lbl_st;
}

void Create_Button_Label(void)
{
    create_single_key_ui(15, 165, "左键:", &btn_L_cont, &btn_L_ind, &btn_L_lbl_status);
    create_single_key_ui(90, 165, "中键:", &btn_M_cont, &btn_M_ind, &btn_M_lbl_status);
    create_single_key_ui(165, 165, "右键:", &btn_R_cont, &btn_R_ind, &btn_R_lbl_status);
}

void Update_Button_Label(void)
{
    if(!btn_L_cont || !btn_M_cont || !btn_R_cont) return;

    if(g_key_L_M_RT != btn_L_last_state) {
        btn_L_last_state = g_key_L_M_RT;
        
        if(g_key_L_M_RT) {
            lv_obj_set_style_bg_color(btn_L_ind, lv_palette_lighten(LV_PALETTE_BLUE, 1), 0);
            lv_label_set_text(btn_L_lbl_status, "已按下");
        } else {
            lv_obj_set_style_bg_color(btn_L_ind, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
            lv_label_set_text(btn_L_lbl_status, "未按下");
        }
    }

    if(g_key_WKP_RT != btn_M_last_state) {
        btn_M_last_state = g_key_WKP_RT;
        
        if(g_key_WKP_RT) {
            lv_obj_set_style_bg_color(btn_M_ind, lv_palette_lighten(LV_PALETTE_BLUE, 1), 0);
            lv_label_set_text(btn_M_lbl_status, "已按下");
        } else {
            lv_obj_set_style_bg_color(btn_M_ind, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
            lv_label_set_text(btn_M_lbl_status, "未按下");
        }
    }

    if(g_key_R_M_RT != btn_R_last_state) {
        btn_R_last_state = g_key_R_M_RT;
        
        if(g_key_R_M_RT) {
            lv_obj_set_style_bg_color(btn_R_ind, lv_palette_lighten(LV_PALETTE_BLUE, 1), 0);
            lv_label_set_text(btn_R_lbl_status, "已按下");
        } else {
            lv_obj_set_style_bg_color(btn_R_ind, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
            lv_label_set_text(btn_R_lbl_status, "未按下");
        }
    }
}

void Remove_Button_Label(void)
{
    if(btn_L_cont) {
        lv_obj_del(btn_L_cont);
        btn_L_cont = NULL;
    }
    if(btn_M_cont) {
        lv_obj_del(btn_M_cont);
        btn_M_cont = NULL;
    }
    if(btn_R_cont) {
        lv_obj_del(btn_R_cont);
        btn_R_cont = NULL;
    }
}
