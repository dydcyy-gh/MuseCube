#include "stm32f4xx.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include "variables.h"
#include "defines.h"
#include "task_manager.h"

static lv_obj_t *desktop_unit = NULL;
static lv_obj_t *wallpaper_bg = NULL;  // 存放桌面壁纸的对象

LV_IMG_DECLARE(pic_back);

extern const lv_img_dsc_t app_clock;
extern const lv_img_dsc_t app_drawing;
extern const lv_img_dsc_t app_file_manager;
extern const lv_img_dsc_t app_game;
extern const lv_img_dsc_t app_music;
extern const lv_img_dsc_t app_setting;
extern const lv_img_dsc_t app_task_manager;
extern const lv_img_dsc_t app_usb;
extern const lv_img_dsc_t app_note;
extern const lv_img_dsc_t app_video;
extern const lv_img_dsc_t app_calendar;
extern const lv_img_dsc_t app_words;
extern const lv_img_dsc_t app_cmd;
extern const lv_img_dsc_t app_calculator;
extern const lv_img_dsc_t app_lots;
extern const lv_img_dsc_t app_album;

// 图标点击回调函数
static void app_icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * icon = lv_event_get_target(e);
    
    if(code == LV_EVENT_CLICKED) {
        int icon_index = (int)(intptr_t)lv_obj_get_user_data(icon);
        switch(icon_index) 
		{
            case 0:  Page_Request_Switch(PAGE_MEM); break; //app_task_manager
			case 1:  Page_Request_Switch(PAGE_MUSIC);break; //app_music
			case 2:  Page_Request_Switch(PAGE_USB_CTRL);break; //app_usb
			case 3:  Page_Request_Switch(PAGE_NOTE);break; //app_note
			case 4:  Page_Request_Switch(PAGE_FILE, "0:/VIDEO"); break; //app_video
			case 5:  Page_Request_Switch(PAGE_FILE, "0:/GAME"); break;  //app_game
			case 6:  Page_Request_Switch(PAGE_SETTINGS);break; //app_setting 
			case 7:  Page_Request_Switch(PAGE_CANVAS);break;
			case 8:  Page_Request_Switch(PAGE_FILE);break; //app_file_manager
			case 9:  Page_Request_Switch(PAGE_CLOCK);break; //app_clock
			case 10: Page_Request_Switch(PAGE_CALENDAR);break;
			case 11: Page_Request_Switch(PAGE_WORDS);break;
			case 12: Page_Request_Switch(PAGE_CALCULATOR);break;
			case 13: Page_Request_Switch(PAGE_CMD);break;
			case 14: Page_Request_Switch(PAGE_LOTS);break;
			case 15: Page_Request_Switch(PAGE_ALBUM);break;
			default: break; 
        }
    }
}

void Create_Desktop_Unit(void)
{
    if (desktop_unit != NULL) return;

    // 0. 创建桌面壁纸 (必须最先创建，保证在最底层)
    wallpaper_bg = lv_img_create(lv_scr_act());
    lv_img_set_src(wallpaper_bg, &pic_back);
    lv_obj_align(wallpaper_bg, LV_ALIGN_CENTER, 0, 0);
	
    // 1. 创建桌面容器 - 使用 Tileview 支持左右翻页滑动
    desktop_unit = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(desktop_unit, 240, 210);
    lv_obj_set_pos(desktop_unit, 0, 0); 
    
    // 设置容器透明，从而露出底层的壁纸，隐藏滚动条
    lv_obj_set_style_bg_opa(desktop_unit, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(desktop_unit, LV_SCROLLBAR_MODE_OFF);

    // 创建两个平铺页面 (page1: 坐标0,0; page2: 坐标1,0)，允许水平滑动 (LV_DIR_HOR)
    lv_obj_t * page1 = lv_tileview_add_tile(desktop_unit, 0, 0, LV_DIR_HOR);
    lv_obj_t * page2 = lv_tileview_add_tile(desktop_unit, 1, 0, LV_DIR_HOR);
    
    // 清除自带背景色以防遮挡壁纸
    lv_obj_set_style_bg_opa(page1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(page2, LV_OPA_TRANSP, 0);

    // 2. 循环创建图标
    const int icon_w = 40;
    const int icon_h = 40;
    const int gap = 16;
    const int start_x = 16;
    const int start_y = 16;
    const int cols = 4;
    const int rows = 3;
    const int items_per_page = rows * cols; // 每页 12 个
    const int total_pages = 2;              // 总共 2 页

    // 建立索引与图标的映射
    const lv_img_dsc_t * app_icons[] = {
        &app_task_manager, // 0: PAGE_MEM
        &app_music,        // 1: PAGE_MUSIC
        &app_usb,          // 2: PAGE_USB_CTRL
        &app_note,         // 3: PAGE_NOTE
        &app_video,        // 4: Task_N_Video
        &app_game,         // 5: Task_N_Game
        &app_setting,      // 6: PAGE_SETTINGS
		&app_drawing,      // 7: app_drawing
		&app_file_manager, // 8: app_file_manager
		&app_clock,        // 9: app_clock
		&app_calendar,     // 10: app_calendar
        &app_words,        // 11: app_words
		&app_calculator,   // 12: app_calculator
		&app_cmd,          // 13: app_cmd
		&app_lots,         // 14: app_lots
		&app_album,         // 15: app_album
    };
    int max_app_count = sizeof(app_icons) / sizeof(app_icons[0]);

    // 遍历两页所有的槽位（2页 x 12个 = 24次）
    for (int i = 0; i < total_pages * items_per_page; i++) {
        // 计算当前处于哪一页，以及在该页上的行列
        int page_idx = i / items_per_page;
        int idx_on_page = i % items_per_page;
        int row = idx_on_page / cols;
        int col = idx_on_page % cols;

        lv_coord_t x = start_x + col * (icon_w + gap);
        lv_coord_t y = start_y + row * (icon_h + gap);

        // 确定父对象：第一页放在 page1，第二页放在 page2
        lv_obj_t * parent_page = (page_idx == 0) ? page1 : page2;

        // 创建 APP 背景容器
        lv_obj_t * icon = lv_obj_create(parent_page);
        lv_obj_set_size(icon, icon_w, icon_h);
        lv_obj_set_pos(icon, x, y);

        // 背景颜色设置为 #baccd9 (浅蓝绿色)
        lv_obj_set_style_bg_color(icon, lv_color_hex(0xc6e6e8), 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
        
        // 去除边框
        lv_obj_set_style_border_width(icon, 0, 0);
        lv_obj_set_style_radius(icon, 10, 0);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(icon, 0, 0);

        // 添加内部的图标图像 (如果数组中有并且不为NULL)
        if (i < max_app_count && app_icons[i] != NULL) {
            lv_obj_t * img = lv_img_create(icon);
            lv_img_set_src(img, app_icons[i]);
            
            // 居中显示图标
            lv_obj_center(img);
            
            // 配置重着色功能将其染成 #ffb3b3 (粉色系)
            lv_obj_set_style_img_recolor(img, lv_color_hex(0xFFB3B3), 0);
            lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
        }

        // 保存索引以供点击事件判断
        lv_obj_set_user_data(icon, (void*)(intptr_t)i);
        lv_obj_add_event_cb(icon, app_icon_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

void Update_Desktop_Unit(void)
{
    
}

void Remove_Desktop_Unit(void)
{
    // 销毁桌面图标容器 (这会连带清除 page1/page2 以及它们下面的子对象)
    if (desktop_unit) {
        lv_obj_del(desktop_unit);
        desktop_unit = NULL;
    }
    
    // 销毁桌面壁纸
    if (wallpaper_bg) {
        lv_obj_del(wallpaper_bg);
        wallpaper_bg = NULL;
    }
}
