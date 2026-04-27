#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "variables.h"
#include "file_unit.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include "fatfs.h"
#include "page_manager.h"

extern const lv_img_dsc_t file_exit_icon;
extern const lv_img_dsc_t file_file_icon;
extern const lv_img_dsc_t file_folder_icon;

// 页面主要对象
static lv_obj_t * file_unit_cont = NULL;
static lv_obj_t * path_label = NULL;
static lv_obj_t * file_list_cont = NULL;

// 函数声明
static void load_file_list(void);
static void folder_click_event_cb(lv_event_t * e);
static void file_click_event_cb(lv_event_t * e);
static void back_click_event_cb(lv_event_t * e);
static void Update_File(void);

/**
 * @brief 移除路径的最后一级（返回上一级目录）
 */
static void path_go_back(void)
{
    // 如果已经在最顶层驱动器目录，直接返回
    if (strcmp(current_path, "") == 0 || strcmp(current_path, "/") == 0) {
        strcpy(current_path, "");
        return;
    }

    char * last_slash = strrchr(current_path, '/');
    if (last_slash != NULL) {
        if (last_slash == current_path + 2 && current_path[1] == ':') { 
            // 形如 "0:/xxx"，退回 "0:"
            *last_slash = '\0';
        } else {
            *last_slash = '\0'; // 截断最后一个 '/'
        }
    } else {
        // 如果连斜杠都没有了（比如 "0:"），说明要退到多盘符选择界面
        strcpy(current_path, "");
    }
}

/**
 * @brief 点击文件夹进入下一级目录的回调
 */
static void folder_click_event_cb(lv_event_t * e)
{
    const char * folder_name = (const char *)lv_event_get_user_data(e);
    
    // 如果当前是在最顶层（无路径），直接拼接盘符名加冒号（视FatFs读出的盘符格式而定，通常读出"0"需补":"）
    if (strcmp(current_path, "") == 0) {
        // 如果FatFs读出的是"0"，我们需要拼成"0:"；如果是带冒号的直接拼
        strcpy(current_path, folder_name);
        if (strchr(current_path, ':') == NULL) {
            strcat(current_path, ":");
        }
    } else {
        // 正常目录拼接
        if (current_path[strlen(current_path) - 1] != '/') {
            strcat(current_path, "/");
        }
        strcat(current_path, folder_name);
    }
    
    // 更新显示
    Update_File();
}

/**
 * @brief 点击文件的回调：将拼接后的完整路径赋给 chosen_file_path，并将标志置1
 */
static void file_click_event_cb(lv_event_t * e)
{
    const char * file_name = (const char *)lv_event_get_user_data(e);
    if (file_name == NULL) return;

    // 清空上次的选择
    memset(chosen_file_path, 0, sizeof(chosen_file_path));

    // 复制当前目录路
    strcpy(chosen_file_path, current_path);

    size_t len = strlen(chosen_file_path);
    // 确保有合法的分隔符
    if (len > 0 && chosen_file_path[len-1] != '/' && chosen_file_path[len-1] != ':') {
        strcat(chosen_file_path, "/");
    } else if (len > 0 && chosen_file_path[len-1] == ':') {
        strcat(chosen_file_path, "/"); // 处理 "0:" 拼接为 "0:/file.txt"
    }

    // 拼接文件名（防溢出检查）
    if (strlen(chosen_file_path) + strlen(file_name) + 1 < sizeof(chosen_file_path)) {
        strcat(chosen_file_path, file_name);
    }

    g_file_chosen = 1; // 通知外部应用：文件已选择
}

/**
 * @brief 删除回调
 */
static void item_delete_event_cb(lv_event_t * e)
{
    void * user_data = lv_event_get_user_data(e);
    if (user_data) {
        lv_mem_free(user_data);
    }
}

/**
 * @brief 点击返回图标的回调
 */
static void back_click_event_cb(lv_event_t * e)
{
    path_go_back();
    Update_File();
}

/**
 * @brief 移除所有LVGL默认的样式（内边距、边框、背景、滚动条）
 */
static void remove_default_style(lv_obj_t * obj)
{
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

/**
 * @brief 读取当前路径的文件并生成列表
 */
static void load_file_list(void)
{
    // 清空现有的文件列表
    lv_obj_clean(file_list_cont);
    
    int y_ofs = 0; // 行偏移，每行20pix

    // ===== 核心修复：如果是最顶层（无路径），则手动探测并列出盘符 =====
    if (strcmp(current_path, "") == 0) {
        for (int i = 0; i < 4; i++) { // 一般嵌入式系统挂载的盘符不会超过 4 个 (0~3)
            char drive_path[8];
            sprintf(drive_path, "%d:", i);
            
            DIR dir;
            // 尝试打开该盘符，如果返回 FR_OK 说明该盘存在且文件系统正常
            if (f_opendir(&dir, drive_path) == FR_OK) {
                f_closedir(&dir);
                
                // 创建每一行的容器
                lv_obj_t * item_cont = lv_obj_create(file_list_cont);
                lv_obj_set_size(item_cont, 240, 20);
                lv_obj_set_pos(item_cont, 0, y_ofs);
                remove_default_style(item_cont);
                
                // 增加鼠标悬浮态的淡灰色背景
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
                
                // 左侧 20*20 图标 (盘符统一当成文件夹图标)
                lv_obj_t * icon = lv_img_create(item_cont);
                lv_img_set_src(icon, &file_folder_icon);
                lv_obj_set_size(icon, 16, 16);
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 2, 0);
                
                // 右侧 盘符名 Label (例如 "0:")
                lv_obj_t * name_label = lv_label_create(item_cont);
                lv_label_set_text(name_label, drive_path);
                lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP);
                lv_obj_set_width(name_label, 220);
                lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 20, 0);
                
                // 使整行可点击
                lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
                
                // 动态分配内存保存盘符名，作为文件夹点击事件处理
                char * folder_name = lv_mem_alloc(strlen(drive_path) + 1);
                strcpy(folder_name, drive_path);
                lv_obj_add_event_cb(item_cont, folder_click_event_cb, LV_EVENT_CLICKED, folder_name);
                lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, folder_name);
                
                y_ofs += 20;
            }
        }
        return; // 盘符遍历结束，直接返回
    }

    // ===== 正常读取目录下的文件 =====
    DIR dir;
    FILINFO fno;
    FRESULT res;

    // 打开当前目录
    res = f_opendir(&dir, current_path);
    if (res != FR_OK) return; // 打开失败则直接返回

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break; // 读取完毕或出错
        
        // 忽略隐藏文件或系统文件
        if (fno.fattrib & (AM_HID | AM_SYS)) continue;

        // 创建每一行的容器
        lv_obj_t * item_cont = lv_obj_create(file_list_cont);
        lv_obj_set_size(item_cont, 240, 20);
        lv_obj_set_pos(item_cont, 0, y_ofs);
        remove_default_style(item_cont);

        // 增加鼠标悬浮态的淡灰色背景
        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
        
        // 判断是文件还是文件夹
        bool is_dir = (fno.fattrib & AM_DIR) ? true : false;
        
        // 左侧 20*20 图标
        lv_obj_t * icon = lv_img_create(item_cont);
        lv_img_set_src(icon, is_dir ? &file_folder_icon : &file_file_icon);
        lv_obj_set_size(icon, 16, 16);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 2, 0);
        
        // 右侧 文件名 Label
        lv_obj_t * name_label = lv_label_create(item_cont);
        lv_label_set_text(name_label, fno.fname);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP); // 超出截断
        lv_obj_set_width(name_label, 220);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 20, 0);
        
        // 使整行可点击
        lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
        
        if (is_dir) {
            // 文件夹：进入目录，动态分配内存保存文件夹名
            char * folder_name = lv_mem_alloc(strlen(fno.fname) + 1);
            strcpy(folder_name, fno.fname);
            lv_obj_add_event_cb(item_cont, folder_click_event_cb, LV_EVENT_CLICKED, folder_name);
            lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, folder_name);
        } else {
            // 文件：分配内存保存文件名
            char * file_name = lv_mem_alloc(strlen(fno.fname) + 1);
            strcpy(file_name, fno.fname);
            lv_obj_add_event_cb(item_cont, file_click_event_cb, LV_EVENT_CLICKED, file_name);
            lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, file_name);
        }
        
        y_ofs += 20;
    }
    
    f_closedir(&dir);
}

static void Update_File(void)
{
    if (file_unit_cont == NULL) return;
    
    // 更新路径 Label
    // 如果是最顶层（空），为了 UI 友好，显示为 "Root" 或 "/"
    if (strcmp(current_path, "") == 0) {
        lv_label_set_text(path_label, "/:");
    } else {
        lv_label_set_text(path_label, current_path);
    }
    
    // 重新加载文件列表
    load_file_list();
}

/**
 * @brief 创建文件浏览器页面
 */
void Create_File_Unit(void)
{
    if (file_unit_cont != NULL) return;

    // 重置文件选中标志
    g_file_chosen = 0;
    memset(chosen_file_path, 0, sizeof(chosen_file_path));

    // 1. 创建主容器 240*180
    file_unit_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(file_unit_cont, 240, 180);
    lv_obj_center(file_unit_cont);
    
    // 设置主背景为白色，去除边框等
    remove_default_style(file_unit_cont);
    lv_obj_set_style_bg_opa(file_unit_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(file_unit_cont, lv_color_white(), LV_PART_MAIN);
    
    // 2. 创建第一行（头部） 240*20
    lv_obj_t * header_cont = lv_obj_create(file_unit_cont);
    lv_obj_set_size(header_cont, 240, 20);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 0);
    remove_default_style(header_cont);
    
    // 头部左侧返回按钮 20*20
    lv_obj_t * back_btn = lv_img_create(header_cont);
    lv_img_set_src(back_btn, &file_exit_icon);
    lv_obj_set_size(back_btn, 16, 16);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_btn, back_click_event_cb, LV_EVENT_CLICKED, NULL);
    
    // 头部路径显示 Label
    path_label = lv_label_create(header_cont);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(path_label, 220); 
    lv_obj_align(path_label, LV_ALIGN_LEFT_MID, 20, 0);
    
    // 3. 创建下方文件列表容器 240*160
    file_list_cont = lv_obj_create(file_unit_cont);
    lv_obj_set_size(file_list_cont, 240, 160);
    lv_obj_align(file_list_cont, LV_ALIGN_TOP_MID, 0, 20);
    remove_default_style(file_list_cont);
    lv_obj_set_scroll_dir(file_list_cont, LV_DIR_VER);
    
    Update_File();
}

/**
 * @brief 更新文件浏览器页面数据
 */
void Update_File_Unit(void)
{
    if(g_file_chosen)
    {
        uint8_t *ext = fatfs_get_extension(chosen_file_path);

        if (ext != NULL) 
        {
            const char * supported_exts[] = {
                "txt", "c", "h", "cpp", "hpp", "cc", "ino",
                "py", "js", "ts", "html", "css",
                "java", "cs", "go", "rs", "php", "rb",
                "sh", "bat", "ps1",
                "json", "xml", "yaml", "yml", "toml",
                "ini", "conf", "cfg", "md", "sql", "log"
            };
            
            bool is_supported = false;
            int num_exts = sizeof(supported_exts) / sizeof(supported_exts[0]);
            
            for (int i = 0; i < num_exts; i++) {
                if (strcmp((char*)ext, supported_exts[i]) == 0) {
                    is_supported = true;
                    break;
                }
            }

            if (is_supported) 
            {
                Page_Request_Switch(PAGE_TEXT);
                g_file_chosen = 0;
            }
        }
    }
}

/**
 * @brief 移除文件浏览页面并释放资源，保留 current_path 记忆功能
 */
void Remove_File_Unit(void)
{
    if (file_unit_cont != NULL) {
        lv_obj_del(file_unit_cont);
        file_unit_cont = NULL;
        path_label = NULL;
        file_list_cont = NULL;
    }
}
