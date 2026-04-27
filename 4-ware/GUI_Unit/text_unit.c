#include "text_unit.h"
#include "lv_port_disp.h"
#include "variables.h"
#include "ff.h"
#include "txt.h"
#include "program.h"
#include "page_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "fatfs.h"

// --- 宏定义 ---
#define MAX_PAGE_HISTORY      256   
#define TXT_CHUNK_SIZE        1024  
#define APPROX_BYTES_PER_PAGE 512   
#define PROG_MAX_READ_SIZE    8192  // 代码文件最大读取限制

// 【性能优化】代码文件切块渲染，每块包含的行数。数值越小，内存占用稍微多一点点但滚动越丝滑
#define LINES_PER_CHUNK       10    

// --- 静态全局变量 ---
static lv_obj_t * txt_cont = NULL;
static lv_obj_t * text_label = NULL;
static lv_obj_t * page_label = NULL;

static txt_decoder_t decoder;
static prog_decoder_t prog_decoder;

static bool is_code_mode = false;

// 滑动窗口历史记录 (TXT 模式使用)
static uint32_t * page_offsets = NULL;
static int valid_pages = 0;         
static int current_idx = 0;         
static uint32_t current_offset = 0;        
static uint32_t current_valid_bytes = 0;   

// --- 内部函数声明 ---
static void load_page(uint32_t target_offset);
static void btn_prev_cb(lv_event_t * e);
static void btn_next_cb(lv_event_t * e);

// --- 接口实现 ---
void Create_Text_Unit(void)
{
    if (txt_cont != NULL) return; 

    // --- 极简判断逻辑 ---
    is_code_mode = false;
    uint8_t *ext = fatfs_get_extension(chosen_file_path); // 统一用您系统的提取后缀函数

    if (ext != NULL) {
        if (strcmp((char*)ext, "txt") == 0) {
            is_code_mode = false; // TXT 走旧的分页逻辑
        } else {
            is_code_mode = true;  // 其他所有后缀全走新的代码解析滚动逻辑
        }
    }

    if (is_code_mode) 
    {
        if (!prog_decoder_open(&prog_decoder, chosen_file_path, PROG_MAX_READ_SIZE)) {
            return;
        }

        txt_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(txt_cont, 240, 180);
        lv_obj_center(txt_cont);
        lv_obj_set_style_radius(txt_cont, 0, 0); 
        lv_obj_set_style_bg_color(txt_cont, lv_color_hex(0xF0F0F0), 0); 
        lv_obj_set_style_pad_all(txt_cont, 0, 0);
        lv_obj_set_style_border_width(txt_cont, 1, 0);
        lv_obj_clear_flag(txt_cont, LV_OBJ_FLAG_SCROLLABLE);

        // 创建代码容器
        lv_obj_t * text_area = lv_obj_create(txt_cont);
        lv_obj_set_size(text_area, 240, 180); 
        lv_obj_align(text_area, LV_ALIGN_TOP_LEFT, 0, 0);
        
        lv_obj_set_style_pad_all(text_area, 5, 0);
        lv_obj_set_style_border_width(text_area, 0, 0);
        lv_obj_set_style_bg_opa(text_area, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(text_area, 0, 0);
        
        lv_obj_add_flag(text_area, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(text_area, LV_DIR_ALL); 
        lv_obj_clear_flag(text_area, LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC);
        
        // 启用 Flex 列布局
        lv_obj_set_layout(text_area, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(text_area, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(text_area, 0, 0); // 消除块与块之间的垂直间隙，实现无缝拼接

        // 【关键优化2】遍历文本，将大的文本串替换切断，分配给多个小 Label
        char * p = prog_decoder.text_buf;
        char * chunk_start = p;
        int line_count = 0;

        while (*p != '\0') {
            if (*p == '\n') {
                line_count++;
                if (line_count == LINES_PER_CHUNK) {
                    *p = '\0'; // 将这一行的换行符替换为 \0，截断字符串
                    
                    lv_obj_t * lbl = lv_label_create(text_area);
                    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP); // 不换行
                    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(lbl, &lv_font_12, 0); 
                    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
                    // 使用静态文本，不额外占用 LVGL 堆内存
                    lv_label_set_text_static(lbl, chunk_start);
                    
                    chunk_start = p + 1; // 下一块起始点跳过当前的 \0
                    line_count = 0;
                }
            }
            p++;
        }
        
        // 处理最后剩下的一小块文本
        if (*chunk_start != '\0') {
            lv_obj_t * lbl = lv_label_create(text_area);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(lbl, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(lbl, &lv_font_12, 0);
            lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_label_set_text_static(lbl, chunk_start);
        }

    } 
    else 
    {
        if (!txt_decoder_open(&decoder, chosen_file_path, TXT_CHUNK_SIZE)) {
            return; 
        }

        if (page_offsets == NULL) {
            page_offsets = (uint32_t *)lv_mem_alloc(MAX_PAGE_HISTORY * sizeof(uint32_t));
            if (page_offsets == NULL) {
                txt_decoder_close(&decoder);
                return;
            }
        }

        txt_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(txt_cont, 240, 180);
        lv_obj_center(txt_cont);
        lv_obj_set_style_radius(txt_cont, 0, 0); 
        lv_obj_set_style_bg_color(txt_cont, lv_color_hex(0xF0F0F0), 0);
        lv_obj_set_style_pad_all(txt_cont, 0, 0);
        lv_obj_set_style_border_width(txt_cont, 1, 0);
        lv_obj_clear_flag(txt_cont, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * text_area = lv_obj_create(txt_cont);
        lv_obj_set_size(text_area, 240, 160); 
        lv_obj_align(text_area, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_pad_top(text_area, 0, 0);
        lv_obj_set_style_pad_bottom(text_area, 0, 0);
        lv_obj_set_style_pad_left(text_area, 5, 0);
        lv_obj_set_style_pad_right(text_area, 5, 0);
        lv_obj_set_style_border_width(text_area, 0, 0);
        lv_obj_set_style_bg_opa(text_area, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(text_area, 0, 0);
        lv_obj_clear_flag(text_area, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN); 
        
        text_label = lv_label_create(text_area);
        lv_obj_set_width(text_label, 230); 
        lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP); 
        lv_obj_set_style_text_font(text_label, &lv_font_12, 0); 
        lv_obj_set_style_max_width(text_label, 230, 0);
        lv_obj_clear_flag(text_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN);

        lv_obj_t * bottom_bar = lv_obj_create(txt_cont);
        lv_obj_set_size(bottom_bar, 240, 20); 
        lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_pad_all(bottom_bar, 0, 0);
        lv_obj_set_style_border_width(bottom_bar, 0, 0);
        lv_obj_set_style_radius(bottom_bar, 0, 0); 
        lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0xE0E0E0), 0);
        lv_obj_clear_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * btn_prev = lv_btn_create(bottom_bar);
        lv_obj_set_size(btn_prev, 30, 20);
        lv_obj_align(btn_prev, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(btn_prev, 3, 0);
        lv_obj_add_event_cb(btn_prev, btn_prev_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t * lbl_prev = lv_label_create(btn_prev);
        lv_label_set_text(lbl_prev, "<");
        lv_obj_center(lbl_prev);

        lv_obj_t * btn_next = lv_btn_create(bottom_bar);
        lv_obj_set_size(btn_next, 30, 20);
        lv_obj_align(btn_next, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_radius(btn_next, 3, 0);
        lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t * lbl_next = lv_label_create(btn_next);
        lv_label_set_text(lbl_next, ">");
        lv_obj_center(lbl_next);

        page_label = lv_label_create(bottom_bar);
        lv_obj_align(page_label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(page_label, "0.0%");

        memset(page_offsets, 0, MAX_PAGE_HISTORY * sizeof(uint32_t));
        valid_pages = 1;
        current_idx = 0;
        
        load_page(0);
    }
}

void Update_Text_Unit(void)
{
}

void Remove_Text_Unit(void)
{
    if (txt_cont != NULL) {
        lv_obj_del(txt_cont);
        txt_cont = NULL;
        text_label = NULL;
        page_label = NULL;
    }
    
    if (is_code_mode) {
        prog_decoder_close(&prog_decoder);
    } else {
        txt_decoder_close(&decoder);
        if (page_offsets != NULL) {
            lv_mem_free(page_offsets);
            page_offsets = NULL;
        }
    }
}

// --- 内部静态函数 (TXT 使用) ---
static uint32_t align_utf8_offset(uint32_t offset) 
{
    if (offset == 0) return 0;
    char check_buf[4];
    UINT br;
    f_lseek(&decoder.file, offset);
    f_read(&decoder.file, check_buf, 4, &br);
    while (offset > 0 && br > 0 && ((check_buf[0] & 0xC0) == 0x80)) {
        offset--;
        f_lseek(&decoder.file, offset);
        f_read(&decoder.file, check_buf, 4, &br);
    }
    return offset;
}

static void load_page(uint32_t target_offset)
{
    uint32_t valid_bytes = txt_decoder_read_chunk(&decoder, target_offset);
    if (valid_bytes > 0) {
        lv_label_set_text_static(text_label, decoder.text_buf);
        lv_obj_update_layout(text_label);
        
        lv_obj_t * text_area = lv_obj_get_parent(text_label);
        lv_coord_t max_h = lv_obj_get_content_height(text_area);
        
        if (lv_obj_get_height(text_label) > max_h) 
        {   
            const lv_font_t * font = lv_obj_get_style_text_font(text_label, 0);
            lv_coord_t line_space = lv_obj_get_style_text_line_space(text_label, 0);
            lv_coord_t line_h = lv_font_get_line_height(font) + line_space;
            
            uint32_t max_lines = max_h / line_h;
            lv_coord_t safe_h = max_lines * line_h;
            
            lv_point_t p;
            p.x = lv_obj_get_content_width(text_area) - 1; 
            p.y = safe_h - 1;                              
            
            uint32_t letter_idx = lv_label_get_letter_on(text_label, &p);
            
            uint32_t byte_idx = 0;
            for (uint32_t i = 0; i <= letter_idx && decoder.text_buf[byte_idx] != '\0'; i++) {
                uint8_t c = (uint8_t)decoder.text_buf[byte_idx];
                if ((c & 0x80) == 0)      byte_idx += 1; 
                else if ((c & 0xE0) == 0xC0) byte_idx += 2; 
                else if ((c & 0xF0) == 0xE0) byte_idx += 3; 
                else if ((c & 0xF8) == 0xF0) byte_idx += 4; 
                else byte_idx += 1; 
            }
            
            decoder.text_buf[byte_idx] = '\0';
            valid_bytes = byte_idx; 
            
            lv_label_set_text_static(text_label, decoder.text_buf);
            lv_obj_update_layout(text_label);
        }

        current_offset = target_offset;
        current_valid_bytes = valid_bytes;
        
        float progress = 0.0f;
        if (decoder.file_size > 0) {
            progress = (float)(target_offset + valid_bytes) / decoder.file_size * 100.0f;
            if (progress > 100.0f) progress = 100.0f;
        }
        
        char prog_str[16];
        snprintf(prog_str, sizeof(prog_str), "%.1f%%", progress);
        lv_label_set_text(page_label, prog_str);
    }
}

static void btn_prev_cb(lv_event_t * e)
{
    if (current_idx > 0) {
        current_idx--;
        load_page(page_offsets[current_idx]);
    } 
    else {
        if (current_offset == 0) return; 
        uint32_t est_offset = (current_offset > APPROX_BYTES_PER_PAGE) ? (current_offset - APPROX_BYTES_PER_PAGE) : 0;
        est_offset = align_utf8_offset(est_offset);

        page_offsets[0] = est_offset;
        valid_pages = 1;
        current_idx = 0;
        
        load_page(est_offset);
    }
}

static void btn_next_cb(lv_event_t * e)
{
    uint32_t next_off = current_offset + current_valid_bytes;
    if (next_off >= decoder.file_size) return;

    if (current_idx < valid_pages - 1) {
        current_idx++;
    } 
    else {
        if (valid_pages < MAX_PAGE_HISTORY) {
            current_idx++;
            page_offsets[current_idx] = next_off;
            valid_pages++;
        } else {
            for (int i = 1; i < MAX_PAGE_HISTORY; i++) {
                page_offsets[i - 1] = page_offsets[i];
            }
            page_offsets[MAX_PAGE_HISTORY - 1] = next_off;
        }
    }
    load_page(page_offsets[current_idx]);
}
