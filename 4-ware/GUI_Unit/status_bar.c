#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "rtc_clock.h"
#include "v2p_bat.h"
#include "adc.h"
#include "page_manager.h"
#include "icon_unit.h"
#include "variables.h"
#include "control_center.h"
#include "defines.h"
#include "keyboard.h"
#include "status_bar.h"
#include "cpu_usage.h"
#include "malloc.h"

volatile uint8_t g_VorP  = 0;
// 引入外部充电状态变量
extern volatile uint8_t g_charge_status; //0-不在充电 1-正在充电 2-充电完成

static lv_obj_t *status_bar = NULL;
//@name:rtc_label_1  @size:12  @pos:(5,5)  @hh:mm:ss(am)
static lv_obj_t *rtc_label_1 = NULL;

// 电池显示相关对象
static lv_obj_t *bat_cont = NULL;   // 电池容器
static lv_obj_t *bat_bar = NULL;    // 电量条
static lv_obj_t *bat_label = NULL;  // 电池文字标签
static lv_obj_t *bat_terminal = NULL; // 电池电极（正极小方块）

// --- 新增: 图标自动排列容器 ---
lv_obj_t *icon_flex_cont = NULL;

// 点击热区对象
static lv_obj_t *click_area_left = NULL;
static lv_obj_t *click_area_mid = NULL;
static lv_obj_t *click_area_right = NULL;

// 右侧显示模式 0-24h, 1-12h, 2-CPU/RAM
static uint8_t right_display_mode = 0; 

// 封装一个更新右侧区域的函数，方便复用
static void Update_Right_Area(void)
{
    if (right_display_mode == 0) {
        // 24小时制：日期在上，时间在下
        lv_label_set_text_fmt(rtc_label_1, "%02d.%02d.%02d\n%02d:%02d:%02d",
            now_date.RTC_Year, now_date.RTC_Month, now_date.RTC_Date,
            now_time.RTC_Hours, now_time.RTC_Minutes, now_time.RTC_Seconds);
    } else if (right_display_mode == 1) {
        // 12小时制：日期在上，时间在下
        const char *ampm = (now_time.RTC_H12 == 0x00) ? "AM" : "PM";
        lv_label_set_text_fmt(rtc_label_1, "%02d.%02d.%02d\n%02d:%02d:%02d%s",
            now_date.RTC_Year, now_date.RTC_Month, now_date.RTC_Date,
            now_time.RTC_Hours, now_time.RTC_Minutes, now_time.RTC_Seconds, ampm);
    } else if (right_display_mode == 2) {
        // CPU 和 RAM 显示
        mem_monitor_t mon;
        tlsf_monitor_ccm(&mon); // 获取内存信息
        uint8_t cpu = Get_CPU_Usage(); // 获取 CPU 信息
        lv_label_set_text_fmt(rtc_label_1, "CPU:%d%%\nRAM:%d%%", cpu, mon.used_pct);
    }
}

// ... (左侧、中间点击回调函数保持不变) ...
static void left_area_click_cb(lv_event_t * e)
{
    g_VorP = !g_VorP;
	// 立即刷新
    if(g_VorP) lv_label_set_text_fmt(bat_label, "%02d%%", g_battery_percent);
    else lv_label_set_text_fmt(bat_label, "%.02fV", g_battery_voltage);
}

static void mid_area_click_cb(lv_event_t * e)
{
	// 获取当前正在显示的页面ID
    uint32_t current_id = Page_Get_Current();

    if (current_id == PAGE_START)
        Page_Request_Switch(PAGE_DESKTOP); 
	else if (g_usb_function == USBD_KBD)
		Create_Keyboard(NULL); 
	else Create_Control_Center();
}

// 右侧点击回调：切换显示模式 (24h -> 12h -> CPU/RAM)
static void right_area_click_cb(lv_event_t * e)
{
    // 循环切换模式 0, 1, 2
    right_display_mode = (right_display_mode + 1) % 3;
    
    // 如果切换到了时间模式，同步更新一下外部的 RTC_HFmt（如果其他地方依赖的话）
    if (right_display_mode == 0) RTC_HFmt = 0;
    else if (right_display_mode == 1) RTC_HFmt = 1;

    RTC_Clock_Update();
    Update_Right_Area(); // 立即刷新
}

void Create_Status_Bar(void)
{
    if (status_bar != NULL) return;

    // 创建状态栏背景容器
    status_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(status_bar, 240, 30);
    lv_obj_set_pos(status_bar, 0, 210);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 绘制顶部分割线
    static lv_obj_t *line = NULL;
    static lv_point_t line_points[] = {{0, 0}, {240, 0}};
    line = lv_line_create(status_bar);
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(0x000000), 0);
    lv_obj_set_style_line_opa(line, LV_OPA_50, 0);

    // -------------------- 电池组件 --------------------
    bat_cont = lv_obj_create(status_bar);
    lv_obj_set_size(bat_cont, 45, 20);
    lv_obj_set_pos(bat_cont, 5, 5);
    lv_obj_set_style_pad_all(bat_cont, 1, 0);
    lv_obj_set_style_border_width(bat_cont, 2, 0);
    lv_obj_set_style_border_color(bat_cont, lv_color_black(), 0);
    lv_obj_set_style_radius(bat_cont, 4, 0);
    lv_obj_set_style_bg_opa(bat_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bat_cont, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_scrollbar_mode(bat_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(bat_cont, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(bat_bar, LV_OBJ_FLAG_SCROLLABLE);

    bat_bar = lv_obj_create(bat_cont);
    lv_obj_set_style_border_width(bat_bar, 0, 0);
    lv_obj_set_style_radius(bat_bar, 3, 0);
    lv_obj_set_height(bat_bar, lv_pct(100));
    lv_obj_clear_flag(bat_bar, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(bat_bar, LV_OBJ_FLAG_SCROLLABLE);

    bat_label = lv_label_create(bat_cont);
    lv_obj_set_width(bat_label, 38);
    lv_obj_align_to(bat_label, bat_cont, LV_ALIGN_CENTER, 2, 0);
    lv_obj_set_style_text_font(bat_label, &lv_font_12, 0);
    lv_obj_set_style_text_align(bat_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(bat_label, lv_color_black(), 0);
    lv_obj_clear_flag(bat_label, LV_OBJ_FLAG_CLICKABLE);

    bat_terminal = lv_obj_create(status_bar);
    lv_obj_set_size(bat_terminal, 3, 10);
    lv_obj_set_pos(bat_terminal, 49, 10);
    lv_obj_set_style_bg_color(bat_terminal, lv_color_black(), 0);
    lv_obj_set_style_border_width(bat_terminal, 0, 0);
    lv_obj_set_style_radius(bat_terminal, 1, 0);
    lv_obj_clear_flag(bat_terminal, LV_OBJ_FLAG_CLICKABLE);

    // 立即设置电池文本、进度条宽度及充电颜色
    if (g_VorP) {
        lv_label_set_text_fmt(bat_label, "%02d%%", g_battery_percent);
    } else {
        lv_label_set_text_fmt(bat_label, "%.02fV", g_battery_voltage);
    }
    int pct = g_battery_percent;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    lv_obj_set_width(bat_bar, lv_pct(pct));

    if (g_charge_status == 1) {
        lv_obj_set_style_bg_color(bat_bar, lv_palette_main(LV_PALETTE_YELLOW), 0);
    } else if (g_charge_status == 2) {
        lv_obj_set_style_bg_color(bat_bar, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_obj_set_style_bg_color(bat_bar, lv_color_white(), 0);
    }

    // -------------------- 图标 Flex 容器 --------------------
    icon_flex_cont = lv_obj_create(status_bar);
    lv_obj_set_pos(icon_flex_cont, 55, 0);
    lv_obj_set_size(icon_flex_cont, 105, 30);
    lv_obj_set_layout(icon_flex_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(icon_flex_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon_flex_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(icon_flex_cont, 0, 0);
    lv_obj_set_style_pad_column(icon_flex_cont, 2, 0);
    lv_obj_set_style_bg_opa(icon_flex_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_flex_cont, 0, 0);
    lv_obj_set_scrollbar_mode(icon_flex_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(icon_flex_cont, LV_OBJ_FLAG_CLICKABLE);

    // 立即创建所有状态图标
    Update_Saving_Icon();
    Update_Headphone_Icon();
    Update_Card_Icon();
    Update_USB_Icon();
    Update_Volume_Icon();

    // -------------------- 时间标签 --------------------
    rtc_label_1 = lv_label_create(status_bar);
    lv_obj_set_style_text_font(rtc_label_1, &lv_font_12, 0);
    lv_obj_set_style_text_align(rtc_label_1, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_size(rtc_label_1, 115, 24);
    
    // 【修改点】：Y坐标从 2 改为 3 整体下移1pix
    lv_obj_set_pos(rtc_label_1, 120, 3); 
    lv_obj_clear_flag(rtc_label_1, LV_OBJ_FLAG_CLICKABLE);

    // 立即设置时间标签文本（采用新的显示逻辑）
    Update_Right_Area();

    // -------------------- 点击热区（交互层）--------------------
    // -------------------- 点击热区（交互层）--------------------
    click_area_left = lv_obj_create(status_bar);
    lv_obj_set_size(click_area_left, 60, 30);
    lv_obj_set_pos(click_area_left, 0, 0);
    lv_obj_set_style_bg_opa(click_area_left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(click_area_left, 0, 0);
    lv_obj_set_scrollbar_mode(click_area_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(click_area_left, LV_OBJ_FLAG_SCROLLABLE); // 【新增这句】
    lv_obj_add_event_cb(click_area_left, left_area_click_cb, LV_EVENT_CLICKED, NULL);

    click_area_mid = lv_obj_create(status_bar);
    lv_obj_set_size(click_area_mid, 120, 30);
    lv_obj_set_pos(click_area_mid, 60, 0);
    lv_obj_set_style_bg_opa(click_area_mid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(click_area_mid, 0, 0);
    lv_obj_set_scrollbar_mode(click_area_mid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(click_area_mid, LV_OBJ_FLAG_SCROLLABLE); // 【新增这句】
    lv_obj_add_event_cb(click_area_mid, mid_area_click_cb, LV_EVENT_CLICKED, NULL);

    click_area_right = lv_obj_create(status_bar);
    lv_obj_set_size(click_area_right, 60, 30);
    lv_obj_align(click_area_right, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(click_area_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(click_area_right, 0, 0);
    lv_obj_set_scrollbar_mode(click_area_right, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(click_area_right, LV_OBJ_FLAG_SCROLLABLE); // 【新增这句】
    lv_obj_add_event_cb(click_area_right, right_area_click_cb, LV_EVENT_CLICKED, NULL);
}

void Update_Status_Bar(void)
{
    if (status_bar == NULL) return;

	static uint8_t last_sec = 0;

    if(now_time.RTC_Seconds == last_sec) return;
    last_sec = now_time.RTC_Seconds;
    
    // 更新右侧时间或资源监控区域
    Update_Right_Area();
	
    // Update Battery Text
	if(g_VorP) lv_label_set_text_fmt(bat_label, "%02d%%", g_battery_percent);
	else lv_label_set_text_fmt(bat_label, "%.02fV", g_battery_voltage);

    // Update Battery Bar Width
    if(bat_bar != NULL) {
        int pct = g_battery_percent;
        if(pct > 100) pct = 100;
        if(pct < 0) pct = 0;
        lv_obj_set_width(bat_bar, lv_pct(pct));

        // 根据充电状态设置颜色
        if (g_charge_status == 1) { 
             lv_obj_set_style_bg_color(bat_bar, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else if (g_charge_status == 2) { 
             lv_obj_set_style_bg_color(bat_bar, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
             lv_obj_set_style_bg_color(bat_bar, lv_color_white(), 0);
        }
    }
	
	// update icons
	Update_Saving_Icon();
	Update_Headphone_Icon();
	Update_Card_Icon();
	Update_USB_Icon();
	Update_Volume_Icon();
}

void Remove_Status_Bar(void)
{
    Remove_Saving_Icon();
    Remove_Headphone_Icon();
    Remove_Card_Icon();
    Remove_USB_Icon();
    Remove_Volume_Icon();

    if(status_bar != NULL) 
	{
        lv_obj_del_async(status_bar); 
        
        status_bar = NULL;
        rtc_label_1 = NULL;
        bat_label = NULL;
        bat_cont = NULL;
        bat_bar = NULL;
        icon_flex_cont = NULL;
        
        click_area_left = NULL;
        click_area_mid = NULL;
        click_area_right = NULL;

        bat_terminal = NULL;
    }
}
