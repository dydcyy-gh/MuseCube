#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include "variables.h"
#include "defines.h"
#include "task_manager.h"

static lv_obj_t *desktop_unit = NULL;
static lv_obj_t *wallpaper_bg = NULL;  // 新增：用于存放桌面壁纸的对象

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

// 图标点击回调函数
static void app_icon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * icon = lv_event_get_target(e);
    
    if(code == LV_EVENT_CLICKED) {
        int icon_index = (int)(intptr_t)lv_obj_get_user_data(icon);
        switch(icon_index) 
		{
            case 0: Page_Request_Switch(PAGE_MEM); break; //app_task_manager
			case 1: Page_Request_Switch(PAGE_MUSIC);break; //app_music
			case 2: Page_Request_Switch(PAGE_USB_CTRL);break; //app_usb
			case 3: Page_Request_Switch(PAGE_NOTE);break; //app_note
			case 4: Taskmanager_Ctrl(Task_N_Video, Task_T_Creat, 0);break; //app_video
			case 5: Taskmanager_Ctrl(Task_N_Game, Task_T_Creat, 0); break; //app_game
			case 6: Page_Request_Switch(PAGE_SETTINGS);break; //app_setting 
			case 7: Page_Request_Switch(PAGE_CANVAS);break;
			case 8: Page_Request_Switch(PAGE_FILE);break; //app_file_manager
			default: break; 
        }
    }
}

void Create_Desktop_Unit(void)
{
    if (desktop_unit != NULL) return;

    // 0. 创建桌面壁纸 (必须最先创建，保证在最底层)
    wallpaper_bg = lv_img_create(lv_scr_act());
    lv_img_set_src(wallpaper_bg,&pic_back);
    lv_obj_align(wallpaper_bg, LV_ALIGN_CENTER, 0, 0);
	
    // 1. 创建桌面容器 (覆盖在壁纸上方)
    desktop_unit = lv_obj_create(lv_scr_act());
    lv_obj_set_size(desktop_unit, 240, 210);
    lv_obj_set_pos(desktop_unit, 0, 0); 
    
    // 设置容器透明，从而露出底层的壁纸
    lv_obj_set_style_bg_opa(desktop_unit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(desktop_unit, 0, 0);
    lv_obj_set_style_border_width(desktop_unit, 0, 0);
    lv_obj_set_style_pad_all(desktop_unit, 0, 0);
    lv_obj_clear_flag(desktop_unit, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 循环创建图标
    const int icon_w = 40;
    const int icon_h = 40;
    const int gap = 16;
    const int start_x = 16;
    const int start_y = 16;
    const int cols = 4;
    const int rows = 3;

    // 建立索引与图标的映射，对应你的 switch case 顺序
    const lv_img_dsc_t * app_icons[] = {
        &app_task_manager, // 0: PAGE_MEM
        &app_music,        // 1: PAGE_MUSIC
        &app_usb,          // 2: PAGE_USB_CTRL
        &app_note,         // 3: PAGE_NOTE
        &app_video,        // 4: Task_N_Video
        &app_game,         // 5: Task_N_Game
        &app_setting,      // 6: PAGE_SETTINGS
		&app_drawing,      // 7: app_drawing
		&app_file_manager  // 8: app_file_manager
    };
    int max_app_count = sizeof(app_icons) / sizeof(app_icons[0]);

    for (int i = 0; i < rows * cols; i++) {
        int row = i / cols;
        int col = i % cols;

        lv_coord_t x = start_x + col * (icon_w + gap);
        lv_coord_t y = start_y + row * (icon_h + gap);

        // 创建 APP 背景容器
        lv_obj_t * icon = lv_obj_create(desktop_unit);
        lv_obj_set_size(icon, icon_w, icon_h);
        lv_obj_set_pos(icon, x, y);

        // 【修改】背景颜色设置为 #baccd9
        lv_obj_set_style_bg_color(icon, lv_color_hex(0xc6e6e8), 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
        
        // 【修改】去除边框
        lv_obj_set_style_border_width(icon, 0, 0);
        lv_obj_set_style_border_color(icon, lv_palette_main(LV_PALETTE_GREY), 0); // 边框宽度为0时颜色其实不生效了
        lv_obj_set_style_radius(icon, 10, 0);
        
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(icon, 0, 0);

        // 根据数组添加内部的图标图像
        if (i < max_app_count) {
            lv_obj_t * img = lv_img_create(icon);
            lv_img_set_src(img, app_icons[i]);
            
            // 居中显示图标
            lv_obj_center(img);
            
            // 【修改】配置重着色功能将其染成 #ffb3b3 (粉色系)
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
    // 销毁桌面图标容器
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
