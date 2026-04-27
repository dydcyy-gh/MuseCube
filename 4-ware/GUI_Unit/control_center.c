#include "lvgl.h"
#include <stdio.h>
#include "variables.h"
#include "defines.h"
#include "es9018k2m.h"
#include "max98357A.h"
#include "pin_ctrl.h"
#include "lcd_pwm.h"

extern const lv_img_dsc_t control_light;
extern const lv_img_dsc_t control_headphone;
extern const lv_img_dsc_t control_speaker;
extern const lv_img_dsc_t control_exit;

// 全局 UI 变量
static lv_obj_t * ctrl_center_cont = NULL;
static lv_obj_t * btn_close = NULL;
static lv_obj_t * bg_cover = NULL;

// 左侧自定义垂直滑块
static lv_obj_t * switch_bg = NULL;
static lv_obj_t * switch_knob = NULL;

static lv_obj_t * btn_bright_pwr = NULL;
static lv_obj_t * btn_hdp_pwr = NULL;
static lv_obj_t * btn_spk_pwr = NULL;

static lv_obj_t * slider_bright = NULL;
static lv_obj_t * slider_hdp = NULL;
static lv_obj_t * slider_spk = NULL;

static lv_obj_t * label_bright = NULL;
static lv_obj_t * label_hdp = NULL;
static lv_obj_t * label_spk = NULL;

// 统一样式
static lv_style_t style_cont;
static lv_style_t style_btn_base;
static lv_style_t style_btn_close;
static lv_style_t style_btn_checked;
static lv_style_t style_slider_bg;
static lv_style_t style_slider_knob;
static lv_style_t style_slider_indic;
static lv_style_t style_slider_indic_disabled;
static bool style_inited = false;

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
    if (style_inited) return;

    /* 容器样式 */
    lv_style_init(&style_cont);
    lv_style_set_radius(&style_cont, 10);
    lv_style_set_border_width(&style_cont, 1);
    lv_style_set_border_color(&style_cont, lv_color_black());
    lv_style_set_bg_color(&style_cont, lv_color_white());
    lv_style_set_bg_opa(&style_cont, LV_OPA_COVER); 
    lv_style_set_pad_all(&style_cont, 0);

    /* 基础按钮样式 (中间的三个电源按钮) */
    lv_style_init(&style_btn_base);
    lv_style_set_radius(&style_btn_base, 4);
    lv_style_set_border_width(&style_btn_base, 0); // 【修改】去除线框
    lv_style_set_border_color(&style_btn_base, lv_color_black());
    lv_style_set_bg_color(&style_btn_base, lv_color_hex(0xCCCCCC));
    lv_style_set_shadow_width(&style_btn_base, 0);
    lv_style_set_pad_all(&style_btn_base, 0); 

    /* 选中后样式 */
    lv_style_init(&style_btn_checked);
    lv_style_set_bg_color(&style_btn_checked, lv_palette_main(LV_PALETTE_BLUE));

    /* 关闭按钮样式 */
    lv_style_init(&style_btn_close);
    lv_style_set_radius(&style_btn_close, 4);
    lv_style_set_border_width(&style_btn_close, 0); // 【修改】去除线框
    lv_style_set_border_color(&style_btn_close, lv_color_black());
    lv_style_set_bg_color(&style_btn_close, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_shadow_width(&style_btn_close, 0);
    lv_style_set_pad_all(&style_btn_close, 0); // 【新增】清除内边距，确保图标完美放入

    /* 滑块/进度条背景样式 */
    lv_style_init(&style_slider_bg);
    lv_style_set_radius(&style_slider_bg, 4);
    lv_style_set_border_width(&style_slider_bg, 1);
    lv_style_set_border_color(&style_slider_bg, lv_color_black());
    lv_style_set_bg_color(&style_slider_bg, lv_color_hex(0xE0E0E0));
    lv_style_set_pad_all(&style_slider_bg, 1); 

    /* 左侧滑块内部旋钮样式 */
    lv_style_init(&style_slider_knob);
    lv_style_set_radius(&style_slider_knob, 4);
    lv_style_set_bg_color(&style_slider_knob, lv_color_hex(0x666666)); 
    lv_style_set_border_width(&style_slider_knob, 0);
    lv_style_set_pad_all(&style_slider_knob, 0);

    /* 进度条指示器样式 (可调节，蓝色) */
    lv_style_init(&style_slider_indic);
    lv_style_set_radius(&style_slider_indic, 3); 
    lv_style_set_bg_color(&style_slider_indic, lv_palette_main(LV_PALETTE_BLUE));

    /* 进度条指示器样式 (不可调节，灰色) */
    lv_style_init(&style_slider_indic_disabled);
    lv_style_set_radius(&style_slider_indic_disabled, 3);
    lv_style_set_bg_color(&style_slider_indic_disabled, lv_color_hex(0xAAAAAA));

    style_inited = true;
}
// 完整的 Create_Control_Center 函数
void Create_Control_Center(void)
{
    if (ctrl_center_cont != NULL) return;

    init_custom_styles();

    // 获取屏幕尺寸
    lv_coord_t screen_w = lv_disp_get_hor_res(NULL);
    lv_coord_t screen_h = lv_disp_get_ver_res(NULL);

    /* ==========================================
       1. 创建全屏透明背景（点击外部关闭）
       ========================================== */
    bg_cover = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bg_cover, screen_w, screen_h);
    lv_obj_set_pos(bg_cover, 0, 0);
    lv_obj_set_style_bg_opa(bg_cover, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bg_cover, 0, 0);
    lv_obj_clear_flag(bg_cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(bg_cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bg_cover, bg_cover_event_cb, LV_EVENT_CLICKED, NULL);

    /* ==========================================
       2. 创建主容器 (适配 200x100)
       ========================================== */
    ctrl_center_cont = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ctrl_center_cont, 200, 100);
    lv_obj_set_pos(ctrl_center_cont, 20, 100); 
    lv_obj_clear_flag(ctrl_center_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctrl_center_cont, LV_OBJ_FLAG_CLICKABLE); // 防止点击穿透
    lv_obj_add_style(ctrl_center_cont, &style_cont, LV_PART_MAIN);

    /* ==========================================
       3. 左上角关闭按钮
       ========================================== */
    btn_close = lv_btn_create(ctrl_center_cont);
    lv_obj_set_size(btn_close, 20, 20);
    lv_obj_set_pos(btn_close, 10, 10);
    lv_obj_add_style(btn_close, &style_btn_close, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_close, btn_close_event_cb, LV_EVENT_CLICKED, NULL);

    // 【新增】退出图标
    lv_obj_t * img_close = lv_img_create(btn_close);
    lv_img_set_src(img_close, &control_exit);
    lv_obj_align(img_close, LV_ALIGN_CENTER, 0, 0);
    // 因为关闭按钮背景是红色，这里将图标重新着色为白色对比更明显
    lv_obj_set_style_img_recolor(img_close, lv_color_black(), 0); 
    lv_obj_set_style_img_recolor_opa(img_close, LV_OPA_COVER, 0);

    /* ==========================================
       4. 左侧自定义垂直滑块
       ========================================== */
    switch_bg = lv_obj_create(ctrl_center_cont);
    lv_obj_set_size(switch_bg, 20, 50);
    lv_obj_set_pos(switch_bg, 10, 40); 
    lv_obj_add_style(switch_bg, &style_slider_bg, LV_PART_MAIN);
    lv_obj_set_style_border_width(switch_bg, 0, LV_PART_MAIN); 
    lv_obj_clear_flag(switch_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(switch_bg, custom_switch_event_cb, LV_EVENT_CLICKED, NULL);

    switch_knob = lv_obj_create(switch_bg);
    lv_obj_set_size(switch_knob, 16, 23);
    lv_obj_add_style(switch_knob, &style_slider_knob, LV_PART_MAIN);
    lv_obj_clear_flag(switch_knob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(switch_knob, LV_OBJ_FLAG_EVENT_BUBBLE); 

    if (g_hdp0_or_spk1 == 0) {
        lv_obj_align(switch_knob, LV_ALIGN_TOP_MID, 0, 0); 
    } else {
        lv_obj_align(switch_knob, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    /* ==========================================
       5. 中部三个电源按钮 (高 20，间距 10 -> Y: 10, 40, 70)
       ========================================== */
    int btn_mid_x = 40; 
    
    // 1. 亮度电源
    btn_bright_pwr = lv_btn_create(ctrl_center_cont);
    lv_obj_set_size(btn_bright_pwr, 20, 20);
    lv_obj_set_pos(btn_bright_pwr, btn_mid_x, 10);
    lv_obj_add_flag(btn_bright_pwr, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_bright_pwr, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_bright_pwr, &style_btn_checked, LV_STATE_CHECKED);
    if (g_screen_pwm == 1) lv_obj_add_state(btn_bright_pwr, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn_bright_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0);
    
    // 亮度电源图标
    lv_obj_t * img_bright = lv_img_create(btn_bright_pwr);
    lv_img_set_src(img_bright, &control_light);
    lv_obj_align(img_bright, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_recolor(img_bright, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(img_bright, LV_OPA_COVER, 0);

    // 2. 耳机电源
    btn_hdp_pwr = lv_btn_create(ctrl_center_cont);
    lv_obj_set_size(btn_hdp_pwr, 20, 20);
    lv_obj_set_pos(btn_hdp_pwr, btn_mid_x, 40);
    lv_obj_add_flag(btn_hdp_pwr, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_hdp_pwr, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_hdp_pwr, &style_btn_checked, LV_STATE_CHECKED);
    if (g_es9018_status == 1) lv_obj_add_state(btn_hdp_pwr, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn_hdp_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1);

    // 耳机电源图标
    lv_obj_t * img_hdp = lv_img_create(btn_hdp_pwr);
    lv_img_set_src(img_hdp, &control_headphone);
    lv_obj_align(img_hdp, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_recolor(img_hdp, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(img_hdp, LV_OPA_COVER, 0);

    // 3. 喇叭电源
    btn_spk_pwr = lv_btn_create(ctrl_center_cont);
    lv_obj_set_size(btn_spk_pwr, 20, 20);
    lv_obj_set_pos(btn_spk_pwr, btn_mid_x, 70);
    lv_obj_add_flag(btn_spk_pwr, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(btn_spk_pwr, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn_spk_pwr, &style_btn_checked, LV_STATE_CHECKED);
    if (g_max98357_ststus == 1) {lv_obj_add_state(btn_spk_pwr, LV_STATE_CHECKED);}
    lv_obj_add_event_cb(btn_spk_pwr, btn_pwr_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2);

    // 喇叭电源图标
    lv_obj_t * img_spk = lv_img_create(btn_spk_pwr);
    lv_img_set_src(img_spk, &control_speaker);
    lv_obj_align(img_spk, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_recolor(img_spk, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(img_spk, LV_OPA_COVER, 0);

    /* ==========================================
       6. 右侧三个进度条(滑块)及数值显示
       ========================================== */
    int slider_right_x = 70; 
    int slider_w = 120; 

    // --- 亮度调节 ---
    slider_bright = lv_slider_create(ctrl_center_cont);
    lv_obj_set_size(slider_bright, slider_w, 20);
    lv_obj_set_pos(slider_bright, slider_right_x, 10);
	lv_obj_set_ext_click_area(slider_bright, 0); 
    lv_slider_set_range(slider_bright, 0, 255);
    lv_slider_set_value(slider_bright, g_brightness, LV_ANIM_OFF);
    lv_obj_add_style(slider_bright, &style_slider_bg, LV_PART_MAIN);
    lv_obj_add_style(slider_bright, &style_slider_indic, LV_PART_INDICATOR);
    lv_obj_add_style(slider_bright, &style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(slider_bright, LV_OPA_TRANSP, LV_PART_KNOB); 
    lv_obj_add_event_cb(slider_bright, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0);

    label_bright = lv_label_create(slider_bright);
    lv_label_set_text_fmt(label_bright, "%03d", g_brightness);
    lv_obj_set_style_text_color(label_bright, lv_color_black(), LV_PART_MAIN); 
    lv_obj_align(label_bright, LV_ALIGN_RIGHT_MID, -5, 0);

    // --- 耳机音量调节 ---
    slider_hdp = lv_slider_create(ctrl_center_cont);
    lv_obj_set_size(slider_hdp, slider_w, 20);
    lv_obj_set_pos(slider_hdp, slider_right_x, 40);
	lv_obj_set_ext_click_area(slider_hdp, 0); 
    lv_slider_set_range(slider_hdp, 0, 255);
    lv_slider_set_value(slider_hdp, g_hdp_value, LV_ANIM_OFF);
    lv_obj_add_style(slider_hdp, &style_slider_bg, LV_PART_MAIN);
    lv_obj_add_style(slider_hdp, &style_slider_indic, LV_PART_INDICATOR);
    lv_obj_add_style(slider_hdp, &style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(slider_hdp, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_hdp, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1);

    label_hdp = lv_label_create(slider_hdp);
    lv_label_set_text_fmt(label_hdp, "%03d", g_hdp_value);
    lv_obj_set_style_text_color(label_hdp, lv_color_black(), LV_PART_MAIN); 
    lv_obj_align(label_hdp, LV_ALIGN_RIGHT_MID, -5, 0);

    // --- 喇叭音量调节 ---
    slider_spk = lv_slider_create(ctrl_center_cont);
    lv_obj_set_size(slider_spk, slider_w, 20);
    lv_obj_set_pos(slider_spk, slider_right_x, 70);
	lv_obj_set_ext_click_area(slider_spk, 0); 
    lv_slider_set_range(slider_spk, 0, 255);
    lv_slider_set_value(slider_spk, g_spk_value, LV_ANIM_OFF);
    lv_obj_add_style(slider_spk, &style_slider_bg, LV_PART_MAIN);
    lv_obj_add_style(slider_spk, &style_slider_indic, LV_PART_INDICATOR);
    lv_obj_add_style(slider_spk, &style_slider_indic_disabled, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(slider_spk, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_add_event_cb(slider_spk, slider_value_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2);

    label_spk = lv_label_create(slider_spk);
    lv_label_set_text_fmt(label_spk, "%03d", g_spk_value);
    lv_obj_set_style_text_color(label_spk, lv_color_black(), LV_PART_MAIN); 
    lv_obj_align(label_spk, LV_ALIGN_RIGHT_MID, -5, 0);

    // 初始状态更新
    update_controls_state();
}

// 完整的 Remove_Control_Center 函数
void Remove_Control_Center(void)
{
    if (ctrl_center_cont != NULL) {
        lv_obj_del(ctrl_center_cont);
        ctrl_center_cont = NULL;
    }
    if (bg_cover != NULL) {
        lv_obj_del(bg_cover);
        bg_cover = NULL;
    }
}

void Update_Control_Center(void)
{
	
}


// ---------------- 内部回调与状态更新实现 ----------------

// 添加背景事件回调
static void bg_cover_event_cb(lv_event_t * e)
{
    // 点击背景任意位置即关闭控制中心
    Remove_Control_Center();
}

// 刷新右侧进度条是否可点击的状态 (配合禁用样式)
static void update_controls_state(void)
{
    if (lv_obj_has_state(btn_bright_pwr, LV_STATE_CHECKED)) {
        lv_obj_clear_state(slider_bright, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(slider_bright, LV_STATE_DISABLED);
    }

    if (lv_obj_has_state(btn_hdp_pwr, LV_STATE_CHECKED)) {
        lv_obj_clear_state(slider_hdp, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(slider_hdp, LV_STATE_DISABLED);
    }

    if (lv_obj_has_state(btn_spk_pwr, LV_STATE_CHECKED)) {
        lv_obj_clear_state(slider_spk, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(slider_spk, LV_STATE_DISABLED);
    }
}

// 关闭按钮回调
static void btn_close_event_cb(lv_event_t * e)
{
    Remove_Control_Center();
}

// 左侧自定义垂直开关回调
static void custom_switch_event_cb(lv_event_t * e)
{
    // 切换状态
    if (g_hdp0_or_spk1 == 0) {
		I2S_Exchange_Ctrl(1);
        lv_obj_align(switch_knob, LV_ALIGN_BOTTOM_MID, 0, 0);
    } else {
		I2S_Exchange_Ctrl(0);
        lv_obj_align(switch_knob, LV_ALIGN_TOP_MID, 0, 0);
    }
}

// 中间电源按钮回调
static void btn_pwr_event_cb(lv_event_t * e)
{
    int type = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t * btn = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(btn, LV_STATE_CHECKED);

    switch(type) {
        case 0:
            if(is_on) {
                LCD_TIM4_PWM_Init(); 
                LCD_PWM_SetFreq(10000);
                LCD_PWM_SetDuty(g_brightness); 
            } else {
                LCD_PWM_DeInit();
            }
            break;
        case 1:
            if(is_on) {if (ES9018_Init()) lv_obj_clear_state(btn, LV_STATE_CHECKED);}
            else ES9018_Deinit();
            break;
        case 2:
            if(is_on) MAX98357_Init(); else MAX98357_Deinit();
            break;
    }
    
    update_controls_state();
}

// 右侧数值调节回调
static void slider_value_event_cb(lv_event_t * e)
{
    int type = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t * slider = lv_event_get_target(e);
    uint8_t val = (uint8_t)lv_slider_get_value(slider);

    switch(type) {
        case 0:
            g_brightness = val;
            lv_label_set_text_fmt(label_bright, "%03d", g_brightness);
            break;
        case 1:
            g_hdp_value = val;
            lv_label_set_text_fmt(label_hdp, "%03d", g_hdp_value);
            break;
        case 2:
            g_spk_value = val;
            lv_label_set_text_fmt(label_spk, "%03d", g_spk_value);
            break;
    }
}
