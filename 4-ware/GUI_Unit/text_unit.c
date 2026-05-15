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
#include "file_unit.h"
#include "malloc.h"

// --- 宏定义 ---
#define MAX_PAGE_HISTORY      256
#define TXT_CHUNK_SIZE        1024
#define APPROX_BYTES_PER_PAGE 512
#define PROG_MAX_READ_SIZE    8192  // 代码文件最大读取限制

#define LINES_PER_CHUNK       10

// --- 状态结构体 ---
typedef struct {
    lv_obj_t * txt_cont;
    lv_obj_t * text_label;
    lv_obj_t * page_label;

    txt_decoder_t decoder;
    prog_decoder_t prog_decoder;

    bool is_code_mode;

    // 滑动窗口历史记录 (TXT 模式使用)
    uint32_t * page_offsets;
    int valid_pages;
    int current_idx;
    uint32_t current_offset;
    uint32_t current_valid_bytes;
} text_state_t;

static text_state_t * ts = NULL;

// --- 内部函数声明 ---
static void load_page(uint32_t target_offset);
static void btn_prev_cb(lv_event_t * e);
static void btn_next_cb(lv_event_t * e);

// --- 接口实现 ---
void Create_Text_Unit(void)
{
    if (ts != NULL) return;

    ts = (text_state_t *)malloc_ccm(sizeof(text_state_t));
    if (!ts) return;
    memset(ts, 0, sizeof(text_state_t));

    uint8_t *ext = fatfs_get_extension(chosen_file_path);

    if (ext != NULL) {
        if (strcmp((char*)ext, "txt") == 0) {
            ts->is_code_mode = false;
        } else {
            ts->is_code_mode = true;
        }
    }

    if (ts->is_code_mode)
    {
        if (!prog_decoder_open(&ts->prog_decoder, chosen_file_path, PROG_MAX_READ_SIZE)) {
            free_ccm(ts);
            ts = NULL;
            return;
        }
        chosen_file_path_free();

        ts->txt_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(ts->txt_cont, 240, 180);
        lv_obj_center(ts->txt_cont);
        lv_obj_set_style_radius(ts->txt_cont, 0, 0);
        lv_obj_set_style_bg_color(ts->txt_cont, lv_color_hex(0xF0F0F0), 0);
        lv_obj_set_style_pad_all(ts->txt_cont, 0, 0);
        lv_obj_set_style_border_width(ts->txt_cont, 1, 0);
        lv_obj_clear_flag(ts->txt_cont, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * text_area = lv_obj_create(ts->txt_cont);
        lv_obj_set_size(text_area, 240, 180);
        lv_obj_align(text_area, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_set_style_pad_all(text_area, 5, 0);
        lv_obj_set_style_border_width(text_area, 0, 0);
        lv_obj_set_style_bg_opa(text_area, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(text_area, 0, 0);

        lv_obj_add_flag(text_area, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(text_area, LV_DIR_ALL);
        lv_obj_clear_flag(text_area, LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC);

        lv_obj_set_layout(text_area, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(text_area, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(text_area, 0, 0);

        char * p = ts->prog_decoder.text_buf;
        char * chunk_start = p;
        int line_count = 0;

        while (*p != '\0') {
            if (*p == '\n') {
                line_count++;
                if (line_count == LINES_PER_CHUNK) {
                    *p = '\0';

                    lv_obj_t * lbl = lv_label_create(text_area);
                    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
                    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(lbl, &lv_font_12, 0);
                    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
                    lv_label_set_text_static(lbl, chunk_start);

                    chunk_start = p + 1;
                    line_count = 0;
                }
            }
            p++;
        }

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
        if (!txt_decoder_open(&ts->decoder, chosen_file_path, TXT_CHUNK_SIZE)) {
            free_ccm(ts);
            ts = NULL;
            return;
        }
        chosen_file_path_free();

        if (ts->page_offsets == NULL) {
            ts->page_offsets = (uint32_t *)lv_mem_alloc(MAX_PAGE_HISTORY * sizeof(uint32_t));
            if (ts->page_offsets == NULL) {
                txt_decoder_close(&ts->decoder);
                free_ccm(ts);
                ts = NULL;
                return;
            }
        }

        ts->txt_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(ts->txt_cont, 240, 180);
        lv_obj_center(ts->txt_cont);
        lv_obj_set_style_radius(ts->txt_cont, 0, 0);
        lv_obj_set_style_bg_color(ts->txt_cont, lv_color_hex(0xF0F0F0), 0);
        lv_obj_set_style_pad_all(ts->txt_cont, 0, 0);
        lv_obj_set_style_border_width(ts->txt_cont, 1, 0);
        lv_obj_clear_flag(ts->txt_cont, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * text_area = lv_obj_create(ts->txt_cont);
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

        ts->text_label = lv_label_create(text_area);
        lv_obj_set_width(ts->text_label, 230);
        lv_label_set_long_mode(ts->text_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(ts->text_label, &lv_font_12, 0);
        lv_obj_set_style_max_width(ts->text_label, 230, 0);
        lv_obj_clear_flag(ts->text_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN);

        lv_obj_t * bottom_bar = lv_obj_create(ts->txt_cont);
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

        ts->page_label = lv_label_create(bottom_bar);
        lv_obj_align(ts->page_label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(ts->page_label, "0.0%");

        memset(ts->page_offsets, 0, MAX_PAGE_HISTORY * sizeof(uint32_t));
        ts->valid_pages = 1;
        ts->current_idx = 0;

        load_page(0);
    }
}

void Update_Text_Unit(void)
{
}

void Remove_Text_Unit(void)
{
    if (!ts) return;

    if (ts->txt_cont != NULL) {
        lv_obj_del(ts->txt_cont);
    }

    if (ts->is_code_mode) {
        prog_decoder_close(&ts->prog_decoder);
    } else {
        txt_decoder_close(&ts->decoder);
        if (ts->page_offsets != NULL) {
            lv_mem_free(ts->page_offsets);
        }
    }

    free_ccm(ts);
    ts = NULL;
}

// --- 内部静态函数 (TXT 使用) ---
static uint32_t align_utf8_offset(uint32_t offset)
{
    if (offset == 0) return 0;
    char check_buf[4];
    UINT br;
    f_lseek(&ts->decoder.file, offset);
    f_read(&ts->decoder.file, check_buf, 4, &br);
    while (offset > 0 && br > 0 && ((check_buf[0] & 0xC0) == 0x80)) {
        offset--;
        f_lseek(&ts->decoder.file, offset);
        f_read(&ts->decoder.file, check_buf, 4, &br);
    }
    return offset;
}

static void load_page(uint32_t target_offset)
{
    uint32_t valid_bytes = txt_decoder_read_chunk(&ts->decoder, target_offset);
    if (valid_bytes > 0) {
        lv_label_set_text_static(ts->text_label, ts->decoder.text_buf);
        lv_obj_update_layout(ts->text_label);

        lv_obj_t * text_area = lv_obj_get_parent(ts->text_label);
        lv_coord_t max_h = lv_obj_get_content_height(text_area);

        if (lv_obj_get_height(ts->text_label) > max_h)
        {
            const lv_font_t * font = lv_obj_get_style_text_font(ts->text_label, 0);
            lv_coord_t line_space = lv_obj_get_style_text_line_space(ts->text_label, 0);
            lv_coord_t line_h = lv_font_get_line_height(font) + line_space;

            uint32_t max_lines = max_h / line_h;
            lv_coord_t safe_h = max_lines * line_h;

            lv_point_t p;
            p.x = lv_obj_get_content_width(text_area) - 1;
            p.y = safe_h - 1;

            uint32_t letter_idx = lv_label_get_letter_on(ts->text_label, &p);

            uint32_t byte_idx = 0;
            for (uint32_t i = 0; i <= letter_idx && ts->decoder.text_buf[byte_idx] != '\0'; i++) {
                uint8_t c = (uint8_t)ts->decoder.text_buf[byte_idx];
                if ((c & 0x80) == 0)      byte_idx += 1;
                else if ((c & 0xE0) == 0xC0) byte_idx += 2;
                else if ((c & 0xF0) == 0xE0) byte_idx += 3;
                else if ((c & 0xF8) == 0xF0) byte_idx += 4;
                else byte_idx += 1;
            }

            ts->decoder.text_buf[byte_idx] = '\0';
            valid_bytes = byte_idx;

            lv_label_set_text_static(ts->text_label, ts->decoder.text_buf);
            lv_obj_update_layout(ts->text_label);
        }

        ts->current_offset = target_offset;
        ts->current_valid_bytes = valid_bytes;

        float progress = 0.0f;
        if (ts->decoder.file_size > 0) {
            progress = (float)(target_offset + valid_bytes) / ts->decoder.file_size * 100.0f;
            if (progress > 100.0f) progress = 100.0f;
        }

        char prog_str[16];
        snprintf(prog_str, sizeof(prog_str), "%.1f%%", progress);
        lv_label_set_text(ts->page_label, prog_str);
    }
}

static void btn_prev_cb(lv_event_t * e)
{
    if (ts->current_idx > 0) {
        ts->current_idx--;
        load_page(ts->page_offsets[ts->current_idx]);
    }
    else {
        if (ts->current_offset == 0) return;
        uint32_t est_offset = (ts->current_offset > APPROX_BYTES_PER_PAGE) ? (ts->current_offset - APPROX_BYTES_PER_PAGE) : 0;
        est_offset = align_utf8_offset(est_offset);

        ts->page_offsets[0] = est_offset;
        ts->valid_pages = 1;
        ts->current_idx = 0;

        load_page(est_offset);
    }
}

static void btn_next_cb(lv_event_t * e)
{
    uint32_t next_off = ts->current_offset + ts->current_valid_bytes;
    if (next_off >= ts->decoder.file_size) return;

    if (ts->current_idx < ts->valid_pages - 1) {
        ts->current_idx++;
    }
    else {
        if (ts->valid_pages < MAX_PAGE_HISTORY) {
            ts->current_idx++;
            ts->page_offsets[ts->current_idx] = next_off;
            ts->valid_pages++;
        } else {
            for (int i = 1; i < MAX_PAGE_HISTORY; i++) {
                ts->page_offsets[i - 1] = ts->page_offsets[i];
            }
            ts->page_offsets[MAX_PAGE_HISTORY - 1] = next_off;
        }
    }
    load_page(ts->page_offsets[ts->current_idx]);
}
