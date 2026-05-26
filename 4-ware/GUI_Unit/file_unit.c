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

// 盘符监控状态
static uint8_t last_drive_mask = 0xFF;

// =========== 文件操作状态机 ============
#define COLOR_A lv_color_hex(0xE0E0E0) // 浅灰：不可用 / 已记录
#define COLOR_B lv_color_hex(0x90CAF9) // 浅蓝：可用 / 待执行
#define COLOR_C lv_color_hex(0xFFCC80) // 浅橙：等待点击目标

enum {
    OP_IDLE = 0,         // 默认状态
    OP_COPY_WAITING = 1, // 点击了复制，等待选中文件
    OP_COPY_READY = 2,   // 已选中文件，等待点击粘贴
    OP_DEL_WAITING = 3,  // 点击了删除，等待选中文件
    OP_DEL_READY = 4     // 已选中文件，等待再次点击删除以执行
};

static uint8_t op_mode = OP_IDLE;
// 动态分配的目标路径，节省静态内存
static char * op_path = NULL; 

// 浮动容器对象
static lv_obj_t * btn_copy = NULL;
static lv_obj_t * btn_paste = NULL;
static lv_obj_t * btn_delete = NULL;

// 函数声明
static void load_file_list(void);
static void folder_click_event_cb(lv_event_t * e);
static void file_click_event_cb(lv_event_t * e);
static void back_click_event_cb(lv_event_t * e);
static void Update_File(void);
static void update_op_buttons(void);

// ================= 底层 FatFs 递归操作引擎 =================
// 递归删除
static int do_delete(const char* path) {
    FILINFO fno;
    if (f_stat(path, &fno) != FR_OK) return -1;
    if (fno.fattrib & AM_DIR) {
        DIR dir;
        if (f_opendir(&dir, path) == FR_OK) {
            while(1) {
                if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
                char *sub = malloc_bsc(strlen(path) + strlen(fno.fname) + 2);
                sprintf(sub, "%s/%s", path, fno.fname);
                do_delete(sub);
                free_bsc(sub);
            }
            f_closedir(&dir);
        }
    }
    return (f_unlink(path) == FR_OK) ? 0 : -1;
}

// 递归复制
static int do_copy(const char* src, const char* dst) {
    FILINFO fno;
    if (f_stat(src, &fno) != FR_OK) return -1;
    if (fno.fattrib & AM_DIR) {
        f_mkdir(dst); // 创建文件夹，若已存在忽略错误
        DIR dir;
        if (f_opendir(&dir, src) == FR_OK) {
            while(1) {
                if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
                char *sub_src = malloc_bsc(strlen(src) + strlen(fno.fname) + 2);
                char *sub_dst = malloc_bsc(strlen(dst) + strlen(fno.fname) + 2);
                sprintf(sub_src, "%s/%s", src, fno.fname);
                sprintf(sub_dst, "%s/%s", dst, fno.fname);
                do_copy(sub_src, sub_dst);
                free_bsc(sub_src);
                free_bsc(sub_dst);
            }
            f_closedir(&dir);
        }
    } else {
        FIL fsrc, fdst;
        if (f_open(&fsrc, src, FA_READ) != FR_OK) return -1;
        if (f_open(&fdst, dst, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            f_close(&fsrc); return -1;
        }
        uint8_t *buf = malloc_bsc(4096); // 分配 4KB 缓冲
        if(buf) {
            UINT br, bw;
            while(1) {
                f_read(&fsrc, buf, 4096, &br);
                if (br == 0) break;
                f_write(&fdst, buf, br, &bw);
                if (bw < br) break;
            }
            free_bsc(buf);
        }
        f_close(&fsrc);
        f_close(&fdst);
    }
    return 0;
}

// ================= UI 操作与逻辑控制 =================
static void reset_op_state(void) {
    op_mode = OP_IDLE;
    if (op_path) {
        free_bsc(op_path);
        op_path = NULL;
    }
}

static void op_btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    
    if (btn == btn_copy) {
        if (op_mode == OP_IDLE) {
            op_mode = OP_COPY_WAITING; 
        } else if (op_mode == OP_COPY_WAITING || op_mode == OP_COPY_READY) {
            reset_op_state(); // 取消记录
        }
    } 
    else if (btn == btn_paste) {
        if (op_mode == OP_COPY_READY && op_path) {
            const char *basename = strrchr(op_path, '/');
            if (!basename) {
                basename = strchr(op_path, ':');
                if(basename) basename++; else basename = op_path;
            } else basename++;
            
            char target[256] = {0};
            strcpy(target, current_path);
            int len = strlen(target);
            if (len > 0 && target[len-1] != '/' && target[len-1] != ':') strcat(target, "/");
            else if (len > 0 && target[len-1] == ':') strcat(target, "/");
            strcat(target, basename);
            
            if (strncmp(target, op_path, strlen(op_path)) != 0) {
                do_copy(op_path, target);
            }
            
            reset_op_state();
            Update_File(); 
        }
    }
    else if (btn == btn_delete) {
        if (op_mode == OP_IDLE) {
            op_mode = OP_DEL_WAITING; 
        } else if (op_mode == OP_DEL_WAITING) {
            reset_op_state(); 
        } else if (op_mode == OP_DEL_READY && op_path) {
            do_delete(op_path); 
            reset_op_state();
            Update_File(); 
        }
    }
    update_op_buttons();
}

static void update_op_buttons(void) {
    if (!btn_copy || !btn_paste || !btn_delete) return;
    
    // 如果在盘符根目录，则隐藏这三个悬浮按钮
    if (strcmp(current_path, "") == 0) {
        lv_obj_add_flag(btn_copy, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_paste, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_delete, LV_OBJ_FLAG_HIDDEN);
        return;
    } else {
        lv_obj_clear_flag(btn_copy, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_paste, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_delete, LV_OBJ_FLAG_HIDDEN);
    }

    lv_color_t c_copy, c_paste, c_del;
    switch(op_mode) {
        case OP_IDLE:         c_copy = COLOR_B; c_paste = COLOR_A; c_del = COLOR_A; break;
        case OP_COPY_WAITING: c_copy = COLOR_C; c_paste = COLOR_A; c_del = COLOR_A; break;
        case OP_COPY_READY:   c_copy = COLOR_A; c_paste = COLOR_B; c_del = COLOR_A; break;
        case OP_DEL_WAITING:  c_copy = COLOR_A; c_paste = COLOR_A; c_del = COLOR_C; break;
        case OP_DEL_READY:    c_copy = COLOR_A; c_paste = COLOR_A; c_del = COLOR_B; break;
        default:              c_copy = COLOR_A; c_paste = COLOR_A; c_del = COLOR_A; break;
    }
    lv_obj_set_style_bg_color(btn_copy, c_copy, 0);
    lv_obj_set_style_bg_color(btn_paste, c_paste, 0);
    lv_obj_set_style_bg_color(btn_delete, c_del, 0);
}

static uint8_t get_available_drives(void)
{
    uint8_t mask = 0;
    DIR dir;
    if (f_opendir(&dir, "0:") == FR_OK) { mask |= (1 << 0); f_closedir(&dir); }
    if (f_opendir(&dir, "1:") == FR_OK) { mask |= (1 << 1); f_closedir(&dir); }
    if (f_opendir(&dir, "2:") == FR_OK) { mask |= (1 << 2); f_closedir(&dir); }
    return mask;
}

static void path_go_back(void)
{
    if (strcmp(current_path, "") == 0 || strcmp(current_path, "/") == 0) {
        strcpy(current_path, "");
        return;
    }
    char *last_slash = strrchr(current_path, '/');
    if (last_slash != NULL) *last_slash = '\0';
    else strcpy(current_path, "");
}

// ============== 拦截引擎 ==============
static void build_full_path(char *dst, const char *name) {
    strcpy(dst, current_path);
    int len = strlen(dst);
    if (len > 0 && dst[len-1] != '/' && dst[len-1] != ':') strcat(dst, "/");
    else if (len > 0 && dst[len-1] == ':') strcat(dst, "/");
    strcat(dst, name);
}

static bool intercept_operation(const char *name) {
    if (op_mode == OP_COPY_WAITING || op_mode == OP_DEL_WAITING) {
        if (!op_path) op_path = malloc_bsc(256);
        if (op_path) build_full_path(op_path, name);
        
        if (op_mode == OP_COPY_WAITING) op_mode = OP_COPY_READY;
        if (op_mode == OP_DEL_WAITING)  op_mode = OP_DEL_READY;
        
        update_op_buttons();
        return true; 
    }
    return false;
}

static void folder_click_event_cb(lv_event_t * e)
{
    const char *folder_name = (const char *)lv_event_get_user_data(e);
    
    if(intercept_operation(folder_name)) return;

    if (strcmp(current_path, "") == 0) {
        strcpy(current_path, folder_name);
        if (strchr(current_path, ':') == NULL) strcat(current_path, ":");
    } else {
        if (current_path[strlen(current_path) - 1] != '/') strcat(current_path, "/");
        strcat(current_path, folder_name);
    }
    Update_File();
}

static void file_click_event_cb(lv_event_t * e)
{
    const char * file_name = (const char *)lv_event_get_user_data(e);
    if (file_name == NULL) return;

    if(intercept_operation(file_name)) return;

    if (!chosen_file_path) chosen_file_path = malloc_bsc(256);
    if (!chosen_file_path) return;
    
    build_full_path(chosen_file_path, file_name);
    g_file_chosen = 1;
}

static void item_delete_event_cb(lv_event_t * e)
{
    void * user_data = lv_event_get_user_data(e);
    if (user_data) lv_mem_free(user_data);
}

static void back_click_event_cb(lv_event_t * e)
{
    path_go_back();
    Update_File();
}

static void remove_default_style(lv_obj_t * obj)
{
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static void load_file_list(void)
{
    lv_obj_clean(file_list_cont);
    int y_ofs = 0;

    if (strcmp(current_path, "") == 0) 
    {
        for (int i = 0; i < 3; i++) {
            char drive_path[8];
            sprintf(drive_path, "%d:", i);
            DWORD fre_clust, fre_sect, tot_sect;
            FATFS *fs;
            if (f_getfree(drive_path, &fre_clust, &fs) == FR_OK) {
                tot_sect = (fs->n_fatent - 2) * fs->csize;
                fre_sect = fre_clust * fs->csize;
                uint32_t tot_MB = tot_sect / 2048;
                uint32_t fre_MB = fre_sect / 2048;
                uint32_t used_percent = tot_sect ? ((tot_sect - fre_sect) * 100 / tot_sect) : 0;

                lv_obj_t * item_cont = lv_obj_create(file_list_cont);
                lv_obj_set_size(item_cont, 240, 48);
                lv_obj_set_pos(item_cont, 0, y_ofs);
                remove_default_style(item_cont);
                
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xE0E0E0), LV_STATE_HOVERED | LV_PART_MAIN);
                lv_obj_set_style_bg_opa(item_cont, LV_OPA_COVER, LV_STATE_PRESSED | LV_PART_MAIN);
                lv_obj_set_style_bg_color(item_cont, lv_color_hex(0xC0C0C0), LV_STATE_PRESSED | LV_PART_MAIN);
                
                lv_obj_t * icon = lv_img_create(item_cont);
                lv_img_set_src(icon, &file_folder_icon); 
                lv_obj_set_size(icon, 16, 16);
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
                
                const char * disk_name = (i == 0) ? "SD卡 (0:)" : ((i == 1) ? "U盘 1 (1:)" : "U盘 2 (2:)");
                lv_obj_t * name_label = lv_label_create(item_cont);
                lv_obj_set_style_text_font(name_label, &lv_font_12, 0);
                lv_label_set_text(name_label, disk_name);
                lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 36, 4);
                
                lv_obj_t * bar = lv_bar_create(item_cont);
                lv_obj_set_size(bar, 190, 6);
                lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 36, 22);
                lv_bar_set_range(bar, 0, 100);
                lv_bar_set_value(bar, used_percent, LV_ANIM_OFF);
                
                char size_str[64];
                if (tot_MB >= 1024) { 
                    sprintf(size_str, "#808080 %lu.%lu GB 可用，共 %lu.%lu GB#", 
                            fre_MB / 1024, (fre_MB % 1024) * 10 / 1024,
                            tot_MB / 1024, (tot_MB % 1024) * 10 / 1024);
                } else { 
                    sprintf(size_str, "#808080 %lu MB 可用，共 %lu MB#", fre_MB, tot_MB);
                }
                
                lv_obj_t * size_label = lv_label_create(item_cont);
                lv_obj_set_style_text_font(size_label, &lv_font_12, 0);
                lv_label_set_recolor(size_label, true);
                lv_label_set_text(size_label, size_str);
                lv_obj_align(size_label, LV_ALIGN_TOP_LEFT, 36, 32);
                
                lv_obj_add_flag(item_cont, LV_OBJ_FLAG_CLICKABLE);
                char * folder_name = lv_mem_alloc(strlen(drive_path) + 1);
                strcpy(folder_name, drive_path);
                lv_obj_add_event_cb(item_cont, folder_click_event_cb, LV_EVENT_CLICKED, folder_name);
                lv_obj_add_event_cb(item_cont, item_delete_event_cb, LV_EVENT_DELETE, folder_name);
                y_ofs += 48; 
            }
        }
        return; 
    }

    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, current_path) != FR_OK) return; 

    while (1) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & (AM_HID | AM_SYS)) continue;

        lv_obj_t * item_cont = lv_obj_create(file_list_cont);
        lv_obj_set_size(item_cont, 240, 20); 
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
        // 缩小名字显示宽度，防止文字遮挡右侧悬浮按钮
        lv_obj_set_width(name_label, 180);
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
        last_drive_mask = get_available_drives();
    } else {
        lv_label_set_text(path_label, current_path);
    }
    
    update_op_buttons(); // 在此触发一次按钮的显示/隐藏与颜色刷新
    
    load_file_list();
}

/**
 * @brief 辅助创建无阴影、无圆角的小工具按钮
 */
static lv_obj_t * create_tool_btn(lv_obj_t * parent, const char * text, int y_ofs)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 30, 20);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -5, y_ofs);
    
    // 清除边距和阴影，圆角设为0
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN); 
    
    lv_obj_add_event_cb(btn, op_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &lv_font_12, 0);
    // 强制字体为黑色
    lv_obj_set_style_text_color(label, lv_color_black(), 0); 
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

/**
 * @brief 创建文件浏览器页面
 */
void Create_File_Unit(void)
{
    if (file_unit_cont != NULL) return;

    if (current_path == NULL) current_path = malloc_bsc(256);

    g_file_chosen = 0;
    last_drive_mask = 0xFF; 
    reset_op_state(); // 重置状态机与内存

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
    lv_obj_set_size(file_list_cont, 240, 160); // 恢复全屏列表高度
    lv_obj_align(file_list_cont, LV_ALIGN_TOP_MID, 0, 20);
    remove_default_style(file_list_cont);
    lv_obj_set_scroll_dir(file_list_cont, LV_DIR_VER);

    // ============= 右侧悬浮操作按钮 =============
    // 悬浮在主容器上，y轴偏移避开 header_cont (高度20)
    btn_copy   = create_tool_btn(file_unit_cont, "复制", 25);
    btn_paste  = create_tool_btn(file_unit_cont, "粘贴", 50);
    btn_delete = create_tool_btn(file_unit_cont, "删除", 75);

    Update_File();
}

/**
 * @brief 更新文件浏览器页面数据 (支持自动挂载刷新和拔出防死机)
 */
void Update_File_Unit(void)
{
    if (file_unit_cont == NULL) return;

    static uint32_t drive_poll_cnt = 0;
    if (++drive_poll_cnt >= 20) { 
        drive_poll_cnt = 0;
        uint8_t current_mask = get_available_drives();
        
        if (current_mask != last_drive_mask) {
            last_drive_mask = current_mask;
            if (strcmp(current_path, "") != 0) {
                int current_drive = current_path[0] - '0';
                if (current_drive >= 0 && current_drive <= 2) {
                    if ((current_mask & (1 << current_drive)) == 0) {
                        strcpy(current_path, "");
                        Update_File();
                    }
                }
            } else {
                Update_File();
            }
        }
    }

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
        btn_copy = btn_paste = btn_delete = NULL;
    }
    if (current_path) {
        free_bsc(current_path);
        current_path = NULL;
    }
    reset_op_state(); // 清理残余状态和路径内存
}

void chosen_file_path_free(void)
{
    if (chosen_file_path) {
        free_bsc(chosen_file_path);
        chosen_file_path = NULL;
    }
}
