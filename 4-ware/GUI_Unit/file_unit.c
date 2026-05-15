#include "stm32f4xx.h"
#include "lv_port_disp.h"
#include "variables.h"
#include "defines.h"
#include "file_unit.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include "fatfs.h"
#include "page_manager.h"
#include "task_manager.h"
#include "malloc.h"

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
    if (strcmp(current_path, "") == 0 || strcmp(current_path, "/") == 0) {
        strcpy(current_path, "");
        return;
    }

    char *last_slash = strrchr(current_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
    } else {
        strcpy(current_path, "");
    }
}

/**
 * @brief 点击文件夹进入下一级目录的回调
 */
static void folder_click_event_cb(lv_event_t * e)
{
    const char *folder_name = (const char *)lv_event_get_user_data(e);

    if (strcmp(current_path, "") == 0) {
        strcpy(current_path, folder_name);
        if (strchr(current_path, ':') == NULL)
            strcat(current_path, ":");
    } else {
        if (current_path[strlen(current_path) - 1] != '/')
            strcat(current_path, "/");
        strcat(current_path, folder_name);
    }

    Update_File();
}

/**
 * @brief 点击文件的回调：将拼接后的完整路径赋给 chosen_file_path，并将标志置1
 */
static void file_click_event_cb(lv_event_t * e)
{
    const char * file_name = (const char *)lv_event_get_user_data(e);
    if (file_name == NULL) return;

    // 确保缓冲区已分配
    if (!chosen_file_path) chosen_file_path = malloc_bsc(256);
    if (!chosen_file_path) return;
    chosen_file_path[0] = '\0';

    // 复制当前目录路径
    strcpy(chosen_file_path, current_path);

    size_t len = strlen(chosen_file_path);
    // 确保有合法的分隔符
    if (len > 0 && chosen_file_path[len-1] != '/' && chosen_file_path[len-1] != ':') {
        strcat(chosen_file_path, "/");
    } else if (len > 0 && chosen_file_path[len-1] == ':') {
        strcat(chosen_file_path, "/"); // 处理 "0:" 拼接为 "0:/file.txt"
    }

    // 拼接文件名（防溢出检查）
    if (strlen(chosen_file_path) + strlen(file_name) + 1 < 256) {
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
    
    int y_ofs = 0;

    // ===== 最顶层状态：展示所有盘符的详细信息（类似 Windows 此电脑） =====
    if (strcmp(current_path, "") == 0) 
	{
        // 遍历0: (SD卡), 1: (U盘1), 2: (U盘2)
        for (int i = 0; i < 3; i++) {
            char drive_path[8];
            sprintf(drive_path, "%d:", i);
            
            DWORD fre_clust, fre_sect, tot_sect;
            FATFS *fs;
            
            // 查询磁盘空闲信息，返回 FR_OK 说明挂载正常
            if (f_getfree(drive_path, &fre_clust, &fs) == FR_OK) {
                // 计算扇区数量 (默认扇区通常为512字节)
                tot_sect = (fs->n_fatent - 2) * fs->csize;
                fre_sect = fre_clust * fs->csize;
                
                // 为了避开某些嵌入式 sprintf 不支持浮点运算，采用整数计算
                // tot_sect / 2 = KB; / 2048 = MB;
                uint32_t tot_MB = tot_sect / 2048;
                uint32_t fre_MB = fre_sect / 2048;
                uint32_t used_percent = tot_sect ? ((tot_sect - fre_sect) * 100 / tot_sect) : 0;

                // 创建包含行 (高度48，容纳标题、进度条、文字)
                lv_obj_t * item_cont = lv_obj_create(file_list_cont);
                lv_obj_set_size(item_cont, 240, 48);
                lv_obj_set_pos(item_cont, 0, y_ofs);
                remove_default_style(item_cont);
                
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
                
                // 1. 左侧图标 (居中稍微靠左)
                lv_obj_t * icon = lv_img_create(item_cont);
                lv_img_set_src(icon, &file_folder_icon); // 暂时代替磁盘图标
                lv_obj_set_size(icon, 16, 16);
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
                
                // 2. 盘符名称 (右上部)
                const char * disk_name = (i == 0) ? "SD卡 (0:)" : ((i == 1) ? "U盘 1 (1:)" : "U盘 2 (2:)");
                lv_obj_t * name_label = lv_label_create(item_cont);
                lv_obj_set_style_text_font(name_label, &lv_font_12, 0);
                lv_label_set_text(name_label, disk_name);
                lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 36, 4);
                
                // 3. 存储容量进度条
                lv_obj_t * bar = lv_bar_create(item_cont);
                lv_obj_set_size(bar, 190, 6);
                lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 36, 22);
                lv_bar_set_range(bar, 0, 100);
                lv_bar_set_value(bar, used_percent, LV_ANIM_OFF);
                
                // 4. 具体容量数值说明 (右下部，使用重着色置灰)
                char size_str[64];
                if (tot_MB >= 1024) { // 转换为 GB 显示
                    sprintf(size_str, "#808080 %lu.%lu GB 可用，共 %lu.%lu GB#", 
                            fre_MB / 1024, (fre_MB % 1024) * 10 / 1024,
                            tot_MB / 1024, (tot_MB % 1024) * 10 / 1024);
                } else { // 保持 MB 显示
                    sprintf(size_str, "#808080 %lu MB 可用，共 %lu MB#", fre_MB, tot_MB);
                }
                
                lv_obj_t * size_label = lv_label_create(item_cont);
                lv_obj_set_style_text_font(size_label, &lv_font_12, 0);
                lv_label_set_recolor(size_label, true);
                lv_label_set_text(size_label, size_str);
                lv_obj_align(size_label, LV_ALIGN_TOP_LEFT, 36, 32);
                
                // 绑定点击事件
                lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
                char * folder_name = lv_mem_alloc(strlen(drive_path) + 1);
                strcpy(folder_name, drive_path);
                lv_obj_add_event_cb(item_cont, folder_click_event_cb, LV_EVENT_CLICKED, folder_name);
                lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, folder_name);
                
                y_ofs += 48; // 行距增加到 48
            }
        }
        return; // 盘符遍历结束，直接返回
    }

    // ===== 正常读取目录下的文件 (原逻辑) =====
    DIR dir;
    FILINFO fno;
    FRESULT res;

    // 打开当前目录
    res = f_opendir(&dir, current_path);
    if (res != FR_OK) return; 

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;
        
        if (fno.fattrib & (AM_HID | AM_SYS)) continue;

        lv_obj_t * item_cont = lv_obj_create(file_list_cont);
        lv_obj_set_size(item_cont, 240, 20); // 正常文件列表维持 20 高度
        lv_obj_set_pos(item_cont, 0, y_ofs);
        remove_default_style(item_cont);

        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
        lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
        
        bool is_dir = (fno.fattrib & AM_DIR) ? true : false;
        
        lv_obj_t * icon = lv_img_create(item_cont);
        lv_img_set_src(icon, is_dir ? &file_folder_icon : &file_file_icon);
        lv_obj_set_size(icon, 16, 16);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 2, 0);
        
        lv_obj_t * name_label = lv_label_create(item_cont);
        lv_obj_set_style_text_font(name_label, &lv_font_12, 0);
        lv_label_set_text(name_label, fno.fname);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(name_label, 220);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 20, 0);
        
        lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
        
        if (is_dir) {
            char * folder_name = lv_mem_alloc(strlen(fno.fname) + 1);
            strcpy(folder_name, fno.fname);
            lv_obj_add_event_cb(item_cont, folder_click_event_cb, LV_EVENT_CLICKED, folder_name);
            lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, folder_name);
        } else {
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

    if (strcmp(current_path, "") == 0) {
        lv_label_set_text(path_label, "此电脑");
    } else {
        lv_label_set_text(path_label, current_path);
    }

    load_file_list();
}

/**
 * @brief 创建文件浏览器页面
 */
void Create_File_Unit(void)
{
    if (file_unit_cont != NULL) return;

    if (current_path == NULL) current_path = malloc_bsc(256);

    g_file_chosen = 0;

    file_unit_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(file_unit_cont, 240, 180);
    lv_obj_center(file_unit_cont);
    
    remove_default_style(file_unit_cont);
    lv_obj_set_style_bg_opa(file_unit_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(file_unit_cont, lv_color_white(), LV_PART_MAIN);
    
    lv_obj_t * header_cont = lv_obj_create(file_unit_cont);
    lv_obj_set_size(header_cont, 240, 20);
    lv_obj_align(header_cont, LV_ALIGN_TOP_MID, 0, 0);
    remove_default_style(header_cont);
    
    lv_obj_t * back_btn = lv_img_create(header_cont);
    lv_img_set_src(back_btn, &file_exit_icon);
    lv_obj_set_size(back_btn, 16, 16);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_btn, back_click_event_cb, LV_EVENT_CLICKED, NULL);
    
    path_label = lv_label_create(header_cont);
    lv_obj_set_style_text_font(path_label, &lv_font_12, 0);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(path_label, 220); 
    lv_obj_align(path_label, LV_ALIGN_LEFT_MID, 20, 0);
    
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
            if (strcmp((char*)ext, "mjpeg") == 0 ||
                strcmp((char*)ext, "raw")   == 0 ||
                strcmp((char*)ext, "bmp")   == 0 ||
                strcmp((char*)ext, "jpeg")  == 0 ||
                strcmp((char*)ext, "jpg")   == 0 ||
                strcmp((char*)ext, "png")   == 0 ||
                strcmp((char*)ext, "gif")   == 0) {
                g_file_chosen = 0;
                Taskmanager_Ctrl(Task_N_Video, Task_T_Creat, 0);
                return;
            }

            if (strcmp((char*)ext, "nes") == 0) {
                g_file_chosen = 0;
                Taskmanager_Ctrl(Task_N_Game, Task_T_Creat, 0);
                return;
            }

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
 * @brief 移除文件浏览页面并释放资源
 */
void Remove_File_Unit(void)
{
    if (file_unit_cont != NULL) {
        lv_obj_del(file_unit_cont);
        file_unit_cont = NULL;
        path_label = NULL;
        file_list_cont = NULL;
    }
    if (current_path) {
        free_bsc(current_path);
        current_path = NULL;
    }
}

void chosen_file_path_free(void)
{
    if (chosen_file_path) {
        free_bsc(chosen_file_path);
        chosen_file_path = NULL;
    }
}
