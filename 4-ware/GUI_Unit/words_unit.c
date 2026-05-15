#include "words_unit.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "malloc.h"
#include "lvgl.h"
#include "page_manager.h"
#include "variables.h"
#include "wordlist.h"
#include "ff.h" // 包含FatFs用于目录遍历

#define WORDS_DIR_PATH "0:/WORDLIST"
#define DEFAULT_WORDS_FILE "0:/WORDLIST/TOEFL.txt"
#define LIST_ITEMS_PER_PAGE 6

// ================= 数据结构 =================
typedef struct {
    wordlist_t current_wl;
    bool is_wl_loaded;
    uint32_t current_word_idx;
    bool is_meaning_shown;
    uint32_t list_page_idx;
    
    char current_book_path[64]; // 保存当前词书路径

    lv_obj_t * main_cont;
    
    // 背单词页面
    lv_obj_t * flashcard_cont;
    lv_obj_t * lbl_word;
    lv_obj_t * lbl_phonetic;
    lv_obj_t * lbl_meaning;
    
    // 单词目录页面
    lv_obj_t * list_cont;
    lv_obj_t * list_items_cont;
    lv_obj_t * list_item_labels[LIST_ITEMS_PER_PAGE];
    lv_obj_t * lbl_list_page_info;

    // 选书页面
    lv_obj_t * book_cont;
    lv_obj_t * book_list_cont;
} words_state_t;

static words_state_t * ws = NULL;

// ================= 辅助函数：刷新 UI =================

static void refresh_flashcard_ui(void)
{
    if (!ws->is_wl_loaded) return;

    word_item_t item;
    if (wordlist_get_by_index(&ws->current_wl, ws->current_word_idx, &item)) {
        lv_label_set_text(ws->lbl_word, item.word);

        if (strlen(item.phonetic) > 0) {
            lv_label_set_text_fmt(ws->lbl_phonetic, "[%s]", item.phonetic);
        } else {
            lv_label_set_text(ws->lbl_phonetic, "");
        }

        lv_label_set_text(ws->lbl_meaning, item.meaning);

        if (ws->is_meaning_shown) {
            lv_obj_clear_flag(ws->lbl_meaning, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ws->lbl_meaning, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void refresh_list_ui(void)
{
    if (!ws->is_wl_loaded) return;

    uint32_t start_idx = ws->list_page_idx * LIST_ITEMS_PER_PAGE;
    uint32_t total_pages = (ws->current_wl.total_words + LIST_ITEMS_PER_PAGE - 1) / LIST_ITEMS_PER_PAGE;
    lv_label_set_text_fmt(ws->lbl_list_page_info, "%lu / %lu", ws->list_page_idx + 1, total_pages);

    for (int i = 0; i < LIST_ITEMS_PER_PAGE; i++) {
        uint32_t target_idx = start_idx + i;
        if (target_idx < ws->current_wl.total_words) {
            word_item_t item;
            if (wordlist_get_by_index(&ws->current_wl, target_idx, &item)) {
                char flat_meaning[128];
                strncpy(flat_meaning, item.meaning, sizeof(flat_meaning)-1);
                flat_meaning[sizeof(flat_meaning)-1] = '\0';
                for(int j = 0; flat_meaning[j] != '\0'; j++) {
                    if(flat_meaning[j] == '\n') flat_meaning[j] = ' ';
                }
                lv_label_set_text_fmt(ws->list_item_labels[i], "%s %s", item.word, flat_meaning);
                lv_obj_clear_flag(ws->list_item_labels[i], LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            lv_obj_add_flag(ws->list_item_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_scroll_to_x(ws->list_items_cont, 0, LV_ANIM_OFF);
}

// ================= 选书页面相关函数 =================

static void book_item_click_cb(lv_event_t * e)
{
    const char * filename = (const char *)lv_event_get_user_data(e);
    
    char new_path[128];
    snprintf(new_path, sizeof(new_path), "%s/%s", WORDS_DIR_PATH, filename);
    
    if (ws->is_wl_loaded) {
        wordlist_close(&ws->current_wl);
    }
    
    ws->is_wl_loaded = wordlist_open(&ws->current_wl, new_path);
    if (ws->is_wl_loaded) {
        strncpy(ws->current_book_path, new_path, sizeof(ws->current_book_path)-1);
        ws->current_word_idx = 0;
        ws->is_meaning_shown = false;
        ws->list_page_idx = 0;
    }
    
    lv_obj_add_flag(ws->book_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ws->flashcard_cont, LV_OBJ_FLAG_HIDDEN);
    refresh_flashcard_ui();
}

static void build_book_list(void)
{
    lv_obj_clean(ws->book_list_cont);
    
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, WORDS_DIR_PATH);
    if (res == FR_OK) {
        while (1) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break; 
            if (fno.fattrib & AM_DIR) continue; 
            
            char *ext = strrchr(fno.fname, '.');
            if (ext && (strcasecmp(ext, ".txt") == 0)) {
                lv_obj_t * item_cont = lv_obj_create(ws->book_list_cont);
                lv_obj_set_size(item_cont, 230, 40);
                lv_obj_set_style_radius(item_cont, 0, 0); 
                lv_obj_set_style_pad_all(item_cont, 5, 0);
                
                char * name_copy = (char *)malloc_ccm(strlen(fno.fname) + 1);
                if (name_copy) {
                    strcpy(name_copy, fno.fname);
                    lv_obj_add_event_cb(item_cont, book_item_click_cb, LV_EVENT_CLICKED, name_copy);
                }
                
                lv_obj_t * lbl = lv_label_create(item_cont);
                lv_label_set_text(lbl, fno.fname);
                lv_obj_center(lbl);
            }
        }
        f_closedir(&dir);
    }
}

static void btn_go_book_list_cb(lv_event_t * e)
{
    lv_obj_add_flag(ws->flashcard_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ws->book_cont, LV_OBJ_FLAG_HIDDEN);
    build_book_list();
}

static void btn_back_from_book_list_cb(lv_event_t * e)
{
    lv_obj_add_flag(ws->book_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ws->flashcard_cont, LV_OBJ_FLAG_HIDDEN);
}


// ================= 事件回调 =================

static void flashcard_click_cb(lv_event_t * e)
{
    ws->is_meaning_shown = !ws->is_meaning_shown;
    refresh_flashcard_ui();
}

static void btn_prev_word_cb(lv_event_t * e)
{
    if (!ws->is_wl_loaded) return;
    if (ws->current_word_idx > 0) {
        ws->current_word_idx--;
    } else {
        ws->current_word_idx = ws->current_wl.total_words - 1;
    }
    ws->is_meaning_shown = false;
    refresh_flashcard_ui();
}

static void btn_next_word_cb(lv_event_t * e)
{
    if (!ws->is_wl_loaded) return;
    if (ws->current_word_idx < ws->current_wl.total_words - 1) {
        ws->current_word_idx++;
    } else {
        ws->current_word_idx = 0;
    }
    ws->is_meaning_shown = false;
    refresh_flashcard_ui();
}

static void btn_go_list_cb(lv_event_t * e)
{
    if (ws->is_wl_loaded) {
        ws->list_page_idx = ws->current_word_idx / LIST_ITEMS_PER_PAGE;
    }
    lv_obj_add_flag(ws->flashcard_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ws->list_cont, LV_OBJ_FLAG_HIDDEN);
    refresh_list_ui();
}

static void btn_back_flashcard_cb(lv_event_t * e)
{
    lv_obj_add_flag(ws->list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ws->flashcard_cont, LV_OBJ_FLAG_HIDDEN);
}

static void btn_list_prev_cb(lv_event_t * e)
{
    if (ws->list_page_idx > 0) {
        ws->list_page_idx--;
        refresh_list_ui();
    }
}

static void btn_list_next_cb(lv_event_t * e)
{
    if (!ws->is_wl_loaded) return;
    uint32_t total_pages = (ws->current_wl.total_words + LIST_ITEMS_PER_PAGE - 1) / LIST_ITEMS_PER_PAGE;
    if (ws->list_page_idx < total_pages - 1) {
        ws->list_page_idx++;
        refresh_list_ui();
    }
}

// ================= 页面初始化构建 =================

void Create_Words_Unit(void)
{
    if (ws) return;

    ws = (words_state_t *)malloc_ccm(sizeof(words_state_t));
    if (!ws) return;
    memset(ws, 0, sizeof(words_state_t));

    strncpy(ws->current_book_path, DEFAULT_WORDS_FILE, sizeof(ws->current_book_path)-1);
    ws->is_wl_loaded = wordlist_open(&ws->current_wl, ws->current_book_path);
    ws->current_word_idx = 0;
    ws->is_meaning_shown = false;

    ws->main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ws->main_cont, 240, 180);
    lv_obj_center(ws->main_cont);
    lv_obj_set_style_pad_all(ws->main_cont, 0, 0);
    lv_obj_set_style_border_width(ws->main_cont, 0, 0);
    lv_obj_clear_flag(ws->main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // ================== A. 构建背单词卡片页面 ==================
    ws->flashcard_cont = lv_obj_create(ws->main_cont);
    lv_obj_set_size(ws->flashcard_cont, 240, 180);
    lv_obj_set_style_pad_all(ws->flashcard_cont, 0, 0);
    lv_obj_set_style_border_width(ws->flashcard_cont, 0, 0);
    lv_obj_clear_flag(ws->flashcard_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ws->flashcard_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ws->flashcard_cont, flashcard_click_cb, LV_EVENT_CLICKED, NULL);

    // 选书按钮：放置在左上角 (26x26 正方形，边距5，淡蓝色)
    lv_obj_t * btn_books = lv_btn_create(ws->flashcard_cont);
    lv_obj_set_size(btn_books, 26, 26);
    lv_obj_set_style_radius(btn_books, 0, 0);
    lv_obj_align(btn_books, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(btn_books, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_opa(btn_books, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn_books, 0, 0);
    lv_obj_t * lbl_books = lv_label_create(btn_books);
    lv_label_set_text(lbl_books, LV_SYMBOL_DIRECTORY);
    lv_obj_center(lbl_books);
    lv_obj_add_event_cb(btn_books, btn_go_book_list_cb, LV_EVENT_CLICKED, NULL);

    // 目录按钮：放置在右上角 (26x26 正方形，边距5，淡蓝色)
    lv_obj_t * btn_list = lv_btn_create(ws->flashcard_cont);
    lv_obj_set_size(btn_list, 26, 26);
    lv_obj_set_style_radius(btn_list, 0, 0); 
    lv_obj_align(btn_list, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_color(btn_list, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_opa(btn_list, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn_list, 0, 0);
    lv_obj_t * lbl_list = lv_label_create(btn_list);
    lv_label_set_text(lbl_list, LV_SYMBOL_LIST);
    lv_obj_center(lbl_list);
    lv_obj_add_event_cb(btn_list, btn_go_list_cb, LV_EVENT_CLICKED, NULL);

    ws->lbl_word = lv_label_create(ws->flashcard_cont);
    // 此处可确保原本字体的正常调用（假设外部已包含头文件支持或者未修改）
    lv_obj_set_style_text_font(ws->lbl_word, &lv_font_36, 0);
    lv_obj_align(ws->lbl_word, LV_ALIGN_CENTER, 0, -40);

    ws->lbl_phonetic = lv_label_create(ws->flashcard_cont);
    lv_obj_set_style_text_font(ws->lbl_phonetic, &lv_font_ipa, 0);
    lv_obj_set_style_text_color(ws->lbl_phonetic, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(ws->lbl_phonetic, LV_ALIGN_CENTER, 0, -10);

    ws->lbl_meaning = lv_label_create(ws->flashcard_cont);
    lv_obj_set_width(ws->lbl_meaning, 220);
    lv_label_set_long_mode(ws->lbl_meaning, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ws->lbl_meaning, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ws->lbl_meaning, &lv_font_12, 0);
    lv_obj_align(ws->lbl_meaning, LV_ALIGN_CENTER, 0, 25);

    // 上一个单词：放置在左下角 (65x26，加宽并显示中文，边距5，淡蓝色)
    lv_obj_t * btn_prev = lv_btn_create(ws->flashcard_cont);
    lv_obj_set_size(btn_prev, 65, 26);
    lv_obj_set_style_radius(btn_prev, 0, 0); 
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_set_style_bg_color(btn_prev, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_opa(btn_prev, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn_prev, 0, 0);
    lv_obj_t * lbl_prev = lv_label_create(btn_prev);
    lv_obj_set_style_text_font(lbl_prev, &lv_font_16, 0);
    lv_label_set_text(lbl_prev, "上一个");
    lv_obj_center(lbl_prev);
    lv_obj_add_event_cb(btn_prev, btn_prev_word_cb, LV_EVENT_CLICKED, NULL);

    // 下一个单词：放置在右下角 (65x26，加宽并显示中文，边距5，淡蓝色)
    lv_obj_t * btn_next = lv_btn_create(ws->flashcard_cont);
    lv_obj_set_size(btn_next, 65, 26);
    lv_obj_set_style_radius(btn_next, 0, 0); 
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_set_style_bg_color(btn_next, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn_next, 0, 0);
    lv_obj_t * lbl_next = lv_label_create(btn_next);
    lv_obj_set_style_text_font(lbl_next, &lv_font_16, 0);
    lv_label_set_text(lbl_next, "下一个");
    lv_obj_center(lbl_next);
    lv_obj_add_event_cb(btn_next, btn_next_word_cb, LV_EVENT_CLICKED, NULL);

    // ================== B. 构建单词目录页面 ==================
    ws->list_cont = lv_obj_create(ws->main_cont);
    lv_obj_set_size(ws->list_cont, 240, 180);
    lv_obj_set_style_pad_all(ws->list_cont, 0, 0);
    lv_obj_set_style_border_width(ws->list_cont, 0, 0);
    lv_obj_clear_flag(ws->list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ws->list_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * list_top_bar = lv_obj_create(ws->list_cont);
    lv_obj_set_size(list_top_bar, 240, 26);
    lv_obj_align(list_top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(list_top_bar, 0, 0);
    lv_obj_set_style_border_width(list_top_bar, 0, 0);
    lv_obj_set_style_radius(list_top_bar, 0, 0);
    lv_obj_set_style_bg_color(list_top_bar, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
    lv_obj_clear_flag(list_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 目录上一页按钮 (取消边框，使用 16 号字体的 <)
    lv_obj_t * btn_pg_up = lv_btn_create(list_top_bar);
    lv_obj_set_size(btn_pg_up, 26, 26);
    lv_obj_set_style_radius(btn_pg_up, 0, 0); 
    lv_obj_set_style_border_width(btn_pg_up, 0, 0);
    lv_obj_align(btn_pg_up, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_pg_up, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_pg_up, 0, 0);
    lv_obj_t * lbl_pg_up = lv_label_create(btn_pg_up);
    lv_obj_set_style_text_font(lbl_pg_up, &lv_font_16, 0);
    lv_label_set_text(lbl_pg_up, "<");
    lv_obj_center(lbl_pg_up);
    lv_obj_add_event_cb(btn_pg_up, btn_list_prev_cb, LV_EVENT_CLICKED, NULL);

    // 目录返回按钮 (保留长条作为顶部标题触控区域)
    lv_obj_t * btn_back = lv_btn_create(list_top_bar);
    lv_obj_set_size(btn_back, 188, 26); 
    lv_obj_set_style_radius(btn_back, 0, 0); 
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    ws->lbl_list_page_info = lv_label_create(btn_back);
    lv_obj_set_style_text_font(ws->lbl_list_page_info, &lv_font_12, 0);
    lv_obj_center(ws->lbl_list_page_info);
    lv_obj_add_event_cb(btn_back, btn_back_flashcard_cb, LV_EVENT_CLICKED, NULL);

    // 目录下一页按钮 (取消边框，使用 16 号字体的 >)
    lv_obj_t * btn_pg_dn = lv_btn_create(list_top_bar);
    lv_obj_set_size(btn_pg_dn, 26, 26);
    lv_obj_set_style_radius(btn_pg_dn, 0, 0); 
    lv_obj_set_style_border_width(btn_pg_dn, 0, 0);
    lv_obj_align(btn_pg_dn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_pg_dn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_pg_dn, 0, 0);
    lv_obj_t * lbl_pg_dn = lv_label_create(btn_pg_dn);
    lv_obj_set_style_text_font(lbl_pg_dn, &lv_font_16, 0);
    lv_label_set_text(lbl_pg_dn, ">");
    lv_obj_center(lbl_pg_dn);
    lv_obj_add_event_cb(btn_pg_dn, btn_list_next_cb, LV_EVENT_CLICKED, NULL);

    // 恢复列表项目容器原高度
    ws->list_items_cont = lv_obj_create(ws->list_cont);
    lv_obj_set_size(ws->list_items_cont, 240, 180 - 26); 
    lv_obj_align(ws->list_items_cont, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_set_style_pad_all(ws->list_items_cont, 0, 0);
    lv_obj_set_style_border_width(ws->list_items_cont, 0, 0);
    lv_obj_set_style_bg_opa(ws->list_items_cont, LV_OPA_TRANSP, 0);

    lv_obj_add_flag(ws->list_items_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ws->list_items_cont, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(ws->list_items_cont, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < LIST_ITEMS_PER_PAGE; i++) {
        ws->list_item_labels[i] = lv_label_create(ws->list_items_cont);
        lv_obj_set_width(ws->list_item_labels[i], LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(ws->list_item_labels[i], &lv_font_12, 0);
        lv_obj_set_pos(ws->list_item_labels[i], 5, i * 25 + 6); // 恢复原有的间距
    }

    // ================== C. 构建选书页面 ==================
    ws->book_cont = lv_obj_create(ws->main_cont);
    lv_obj_set_size(ws->book_cont, 240, 180);
    lv_obj_set_style_pad_all(ws->book_cont, 0, 0);
    lv_obj_set_style_border_width(ws->book_cont, 0, 0);
    lv_obj_clear_flag(ws->book_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ws->book_cont, LV_OBJ_FLAG_HIDDEN);

    // 顶部返回栏
    lv_obj_t * book_top_bar = lv_obj_create(ws->book_cont);
    lv_obj_set_size(book_top_bar, 240, 26);
    lv_obj_align(book_top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(book_top_bar, 0, 0);
    lv_obj_set_style_border_width(book_top_bar, 0, 0);
    lv_obj_set_style_radius(book_top_bar, 0, 0);
    lv_obj_set_style_bg_color(book_top_bar, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0); // 改为淡蓝色
    lv_obj_clear_flag(book_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 选书页面返回按钮 (26x26 正方形)
    lv_obj_t * btn_book_back = lv_btn_create(book_top_bar);
    lv_obj_set_size(btn_book_back, 26, 26);
    lv_obj_set_style_radius(btn_book_back, 0, 0); 
    lv_obj_align(btn_book_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_book_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_book_back, 0, 0);
    lv_obj_t * lbl_book_back = lv_label_create(btn_book_back);
    lv_label_set_text(lbl_book_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_book_back);
    lv_obj_add_event_cb(btn_book_back, btn_back_from_book_list_cb, LV_EVENT_CLICKED, NULL);

    // 将顶栏中间部分做成可点击按钮以返回主页面
    lv_obj_t * btn_book_title = lv_btn_create(book_top_bar);
    lv_obj_set_size(btn_book_title, 188, 26);
    lv_obj_set_style_radius(btn_book_title, 0, 0);
    lv_obj_align(btn_book_title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(btn_book_title, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_book_title, 0, 0);
    lv_obj_add_event_cb(btn_book_title, btn_back_from_book_list_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_book_title = lv_label_create(btn_book_title);
    lv_label_set_text(lbl_book_title, "本地词库列表");
    lv_obj_center(lbl_book_title);

    // 词书列表容器 (可纵向滚动)
    ws->book_list_cont = lv_obj_create(ws->book_cont);
    lv_obj_set_size(ws->book_list_cont, 240, 180 - 26);
    lv_obj_align(ws->book_list_cont, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_set_style_pad_all(ws->book_list_cont, 5, 0);
    lv_obj_set_style_border_width(ws->book_list_cont, 0, 0);
    lv_obj_set_flex_flow(ws->book_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ws->book_list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    refresh_flashcard_ui();
}

void Update_Words_Unit(void)
{
}

void Remove_Words_Unit(void)
{
    if (!ws) return;

    if (ws->main_cont) {
        lv_obj_del(ws->main_cont);
    }

    if (ws->is_wl_loaded) {
        wordlist_close(&ws->current_wl);
    }

    free_ccm(ws);
    ws = NULL;
}
