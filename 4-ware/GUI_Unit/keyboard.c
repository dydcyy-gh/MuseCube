#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "variables.h"
#include "defines.h"
#include "pinyin_find.h"
#include "keyboard.h"
#include "variables.h"

#define CAND_NUM 7
#define PY_BUF_MAX 16

typedef enum
{
    IME_MODE_EN = 0,
    IME_MODE_CN
} ime_mode_t;

static ime_mode_t ime_mode = IME_MODE_CN;
static bool is_pure_en_mode = false; 

static lv_obj_t *kb = NULL;
static lv_obj_t *click_catcher = NULL;
static lv_obj_t *current_ta = NULL;

static lv_obj_t *cand_panel = NULL;
static lv_obj_t *cand_btn[CAND_NUM];
static lv_obj_t *ime_btn = NULL;

static lv_obj_t *py_label = NULL;       
static lv_obj_t *prev_btn = NULL;       
static lv_obj_t *next_btn = NULL;       
static int cand_offset = 0;              
static int cand_total = 0;               

static char py_buf[PY_BUF_MAX];
static uint8_t py_len = 0;

static void close_keyboard_internal(void);

// ========================================================
// ASCII -> USB HID 键码映射函数 (Device模式向外发送用)
// ========================================================
static void map_and_send_usb_key(const char *txt)
{
    if (g_usb_function != USBD_KBD) return;
    if (g_usb_kbd_trigger != 0) return;

    uint8_t mod = 0;
    uint8_t key = 0;

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        key = 0x2A; // BACKSPACE
    } else if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0) {
        key = 0x28; // ENTER
    } else if (txt[0] == ' ') {
        key = 0x2C; // SPACE
    } else if (strlen(txt) == 1) {
        char c = txt[0];
        if (c >= 'a' && c <= 'z') key = c - 'a' + 0x04;
        else if (c >= 'A' && c <= 'Z') { key = c - 'A' + 0x04; mod = 0x02; } 
        else if (c >= '1' && c <= '9') { key = c - '1' + 0x1E; }
        else if (c == '0') { key = 0x27; }
        else {
            switch(c) {
                case '-': key = 0x2D; break;
                case '_': key = 0x2D; mod = 0x02; break;
                case '=': key = 0x2E; break;
                case '+': key = 0x2E; mod = 0x02; break;
                case '[': key = 0x2F; break;
                case '{': key = 0x2F; mod = 0x02; break;
                case ']': key = 0x30; break;
                case '}': key = 0x30; mod = 0x02; break;
                case '\\': key= 0x31; break;
                case '|': key = 0x31; mod = 0x02; break;
                case ';': key = 0x33; break;
                case ':': key = 0x33; mod = 0x02; break;
                case '\'':key = 0x34; break;
                case '"': key = 0x34; mod = 0x02; break;
                case ',': key = 0x36; break;
                case '<': key = 0x36; mod = 0x02; break;
                case '.': key = 0x37; break;
                case '>': key = 0x37; mod = 0x02; break;
                case '/': key = 0x38; break;
                case '?': key = 0x38; mod = 0x02; break;
                case '!': key = 0x1E; mod = 0x02; break;
                case '@': key = 0x1F; mod = 0x02; break;
                case '#': key = 0x20; mod = 0x02; break;
                case '$': key = 0x21; mod = 0x02; break;
                case '%': key = 0x22; mod = 0x02; break;
                case '^': key = 0x23; mod = 0x02; break;
                case '&': key = 0x24; mod = 0x02; break;
                case '*': key = 0x25; mod = 0x02; break;
                case '(': key = 0x26; mod = 0x02; break;
                case ')': key = 0x27; mod = 0x02; break;
            }
        }
    }

    if (key != 0) {
        g_usb_kbd_modifier = mod;
        g_usb_kbd_key = key;
        g_usb_kbd_trigger = 1; 
    }
}

// ========================================================
// 拼音与候选框逻辑
// ========================================================
static void py_clear(void)
{
    py_len = 0;
    py_buf[0] = 0;
}

static void py_add(char c)
{
    if(py_len < PY_BUF_MAX-1)
    {
        py_buf[py_len++] = c;
        py_buf[py_len] = 0;
    }
}

static void cand_update(void)
{
    const char *mb = pinyin_lookup(py_buf);
    if (mb == NULL && py_len > 0) {
        mb = pinyin_lookup_prefix(py_buf);
    }
    if (mb == NULL && py_len > 0) {
        mb = pinyin_lookup_fuzzy(py_buf);
    }
    int total = 0;
    if (mb) {
        total = strlen(mb) / 3; 
    }
    cand_total = total;

    if (cand_total == 0) {
        cand_offset = 0;
    } else {
        int max_offset = cand_total - CAND_NUM;
        if (max_offset < 0) max_offset = 0;
        if (cand_offset > max_offset) cand_offset = max_offset;
    }

    for (int i = 0; i < CAND_NUM; i++) {
        lv_label_set_text(lv_obj_get_child(cand_btn[i], 0), "");
    }

    if (mb && cand_total > 0) {
        const char *p = mb + cand_offset * 3;
        for (int i = 0; i < CAND_NUM; i++) {
            if (*p == '\0') break;
            char txt[4] = {0};
            memcpy(txt, p, 3);
            lv_label_set_text(lv_obj_get_child(cand_btn[i], 0), txt);
            p += 3;
        }
    }

    if (py_label) {
        lv_label_set_text(py_label, py_buf);
    }

    if (prev_btn) {
        if (cand_offset <= 0) {
            lv_obj_add_state(prev_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(prev_btn, LV_STATE_DISABLED);
        }
    }
    if (next_btn) {
        if (cand_offset + CAND_NUM >= cand_total) {
            lv_obj_add_state(next_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(next_btn, LV_STATE_DISABLED);
        }
    }
}

static void cand_event_cb(lv_event_t * e)
{
    if (current_ta == NULL) return;

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    const char *txt = lv_label_get_text(label);

    if (strlen(txt)) {
        lv_textarea_add_text(current_ta, txt);
    }

    py_clear();
    cand_offset = 0;
    cand_update();
}

static void ime_switch_cb(lv_event_t * e)
{
    if (ime_mode == IME_MODE_CN) {
        ime_mode = IME_MODE_EN;
        lv_label_set_text(lv_obj_get_child(ime_btn, 0), "EN");
        if (kb && current_ta) lv_keyboard_set_textarea(kb, current_ta);
    } else {
        ime_mode = IME_MODE_CN;
        lv_label_set_text(lv_obj_get_child(ime_btn, 0), "中");
        if (kb) lv_keyboard_set_textarea(kb, NULL);
    }

    py_clear();
    cand_offset = 0;
    cand_update();
}

static void prev_page_cb(lv_event_t * e)
{
    if (cand_total == 0) return;
    cand_offset -= CAND_NUM;
    if (cand_offset < 0) cand_offset = 0;
    cand_update();
}

static void next_page_cb(lv_event_t * e)
{
    if (cand_total == 0) return;
    int max_offset = cand_total - CAND_NUM;
    if (max_offset < 0) max_offset = 0;
    if (cand_offset < max_offset) {
        cand_offset += CAND_NUM;
        if (cand_offset > max_offset) cand_offset = max_offset;
        cand_update();
    }
}

static void catcher_click_cb(lv_event_t * e)
{
    close_keyboard_internal();
}

// ========================================================
// 统一按键字符处理引擎 (支持软键盘与物理键盘)
// ========================================================
static void handle_keyboard_input(const char *txt, bool from_physical_kb)
{
    if (current_ta == NULL) return;

    // 1. 如果正在输入拼音，物理键盘快捷选词逻辑
    if (from_physical_kb && !is_pure_en_mode && ime_mode == IME_MODE_CN && py_len > 0) 
    {
        // 数字键 1~7 快捷选词
        if (txt[0] >= '1' && txt[0] <= '7' && strlen(txt) == 1) {
            int idx = txt[0] - '1';
            if (cand_total > 0 && idx < CAND_NUM) {
                lv_obj_t *label = lv_obj_get_child(cand_btn[idx], 0);
                const char *cand_txt = lv_label_get_text(label);
                if (strlen(cand_txt)) lv_textarea_add_text(current_ta, cand_txt);
                py_clear();
                cand_offset = 0;
                cand_update();
            }
            return;
        }
        // 空格键快捷选第一个词
        if (txt[0] == ' ') {
            if (cand_total > 0) {
                lv_obj_t *label = lv_obj_get_child(cand_btn[0], 0);
                const char *cand_txt = lv_label_get_text(label);
                if (strlen(cand_txt)) lv_textarea_add_text(current_ta, cand_txt);
                py_clear();
                cand_offset = 0;
                cand_update();
            }
            return;
        }
    }

    // 2. 中文模式输入逻辑
    if (!is_pure_en_mode && ime_mode == IME_MODE_CN)
    {
        if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
            if (py_len > 0) {
                py_len--;
                py_buf[py_len] = 0;
                cand_offset = 0;
                cand_update();
            } else {
                lv_textarea_del_char(current_ta);
            }
            return;
        }

        if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0) {
            lv_textarea_add_text(current_ta, "\n");
            return;
        }

        // 过滤掉键盘切换按键
        if (strcmp(txt, "ABC") == 0 || strcmp(txt, "!#1") == 0 ||
            strcmp(txt, "?123") == 0 || strcmp(txt, "&123") == 0) {
            return;
        }

        // 字母进入拼音缓冲区
        if (strlen(txt) == 1) {
            char c = txt[0];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a'; // 转小写
                py_add(c);
                cand_offset = 0;
                cand_update();
                return;
            }
        }
        
        // 其他符号直接上屏
        lv_textarea_add_text(current_ta, txt);
    }
    // 3. 纯英文模式输入逻辑
    else 
    {
        // 只有来源是物理键盘，才需要我们手动往 textarea 里面塞字符
        // 如果是点击软键盘，LVGL的 lv_keyboard_set_textarea 原生机制会自动写入，避免双重输入！
        if (from_physical_kb) {
            if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
                lv_textarea_del_char(current_ta);
            } else if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0) {
                lv_textarea_add_text(current_ta, "\n");
            } else {
                lv_textarea_add_text(current_ta, txt);
            }
        }
    }
}

/***********************
 * 屏幕软键盘事件回调
 ***********************/
static void kb_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint16_t id = lv_keyboard_get_selected_btn(kb);
        const char *txt = lv_keyboard_get_btn_text(kb, id);

        map_and_send_usb_key(txt); // 如果开启了 USB Device 模式，将按键发送给PC

        // 调用统一引擎，标记为"非物理键盘"
        handle_keyboard_input(txt, false); 
    }

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        close_keyboard_internal();
    }
}

/***********************
 * 内部统一创建逻辑
 ***********************/
static void create_keyboard_internal(lv_obj_t * ta, bool pure_en)
{
    is_pure_en_mode = pure_en;
    current_ta = ta;

    if (kb == NULL)
    {
        pinyin_buf_init();

        click_catcher = lv_obj_create(lv_layer_top());
        lv_obj_set_size(click_catcher, 240, 240);
        lv_obj_set_pos(click_catcher, 0, 0);
        lv_obj_set_style_bg_opa(click_catcher, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(click_catcher, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(click_catcher, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(click_catcher, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(click_catcher, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(click_catcher, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(click_catcher, catcher_click_cb, LV_EVENT_CLICKED, NULL);

        cand_panel = lv_obj_create(lv_layer_top());
        lv_obj_set_size(cand_panel, 240, 24);
        lv_obj_align(cand_panel, LV_ALIGN_BOTTOM_MID, 0, -100); 
        lv_obj_clear_flag(cand_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0xC8C8C8), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cand_panel, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_border_width(cand_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cand_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(cand_panel, 0, LV_PART_MAIN); 

        ime_btn = lv_btn_create(cand_panel);
        lv_obj_set_size(ime_btn, 20, 20);
        lv_obj_set_pos(ime_btn, 0, 2); 
        lv_obj_add_event_cb(ime_btn, ime_switch_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_opa(ime_btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(ime_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ime_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(ime_btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_anim_time(ime_btn, 0, LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(ime_btn);
        lv_label_set_text(label, "中");
        lv_obj_center(label);

        py_label = lv_label_create(cand_panel);
        lv_obj_set_size(py_label, 30, 20);
        lv_obj_set_pos(py_label, 20, 2);                          
        lv_label_set_long_mode(py_label, LV_LABEL_LONG_CLIP);    
        lv_obj_set_style_text_align(py_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_clip_corner(py_label, true, 0);         
        lv_obj_set_style_text_color(py_label, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(py_label, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(py_label, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_top(py_label, 4, 0);
        lv_obj_set_style_pad_bottom(py_label, 4, 0);

        prev_btn = lv_btn_create(cand_panel);
        lv_obj_set_size(prev_btn, 20, 20);
        lv_obj_set_pos(prev_btn, 50, 2);
        lv_obj_add_event_cb(prev_btn, prev_page_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_opa(prev_btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(prev_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(prev_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(prev_btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_anim_time(prev_btn, 0, LV_PART_MAIN);
        label = lv_label_create(prev_btn);
        lv_label_set_text(label, "<");
        lv_obj_center(label);

        for (int i = 0; i < CAND_NUM; i++)
        {
            cand_btn[i] = lv_btn_create(cand_panel);
            lv_obj_set_size(cand_btn[i], 20, 20);
            lv_obj_set_pos(cand_btn[i], 70 + i * 20, 2);
            lv_obj_add_event_cb(cand_btn[i], cand_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_set_style_bg_opa(cand_btn[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(cand_btn[i], 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(cand_btn[i], 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(cand_btn[i], lv_color_black(), LV_PART_MAIN);
            lv_obj_set_style_anim_time(cand_btn[i], 0, LV_PART_MAIN);

            lv_obj_t *lb = lv_label_create(cand_btn[i]);
            lv_label_set_text(lb, "");
            lv_obj_center(lb);
        }

        next_btn = lv_btn_create(cand_panel);
        lv_obj_set_size(next_btn, 20, 20);
        lv_obj_set_pos(next_btn, 70 + CAND_NUM * 20, 2); 
        lv_obj_add_event_cb(next_btn, next_page_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_opa(next_btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(next_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(next_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(next_btn, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_anim_time(next_btn, 0, LV_PART_MAIN);
        label = lv_label_create(next_btn);
        lv_label_set_text(label, ">");
        lv_obj_center(label);

        kb = lv_keyboard_create(lv_layer_top());
        lv_obj_set_size(kb, 240, 100);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_set_style_pad_row(kb, 2, 0);
        lv_obj_set_style_pad_column(kb, 2, 0);
        lv_obj_set_style_pad_all(kb, 3, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(kb, lv_color_hex(0xC8C8C8), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(kb, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_bg_color(kb, lv_color_white(), LV_PART_ITEMS);
        lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(kb, lv_color_black(), LV_PART_ITEMS);
        lv_obj_set_style_border_side(kb, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);
        lv_obj_set_style_radius(kb, 3, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(kb, lv_color_hex(0xE0E0E0), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_text_font(kb, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_font(kb, &lv_font_montserrat_12, LV_PART_ITEMS);
        lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    }

    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(click_catcher, LV_OBJ_FLAG_HIDDEN);

    if (is_pure_en_mode) 
    {
        ime_mode = IME_MODE_EN;
        lv_keyboard_set_textarea(kb, ta);
        if (cand_panel) {
            lv_obj_add_flag(cand_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else 
    {
        if (ime_mode == IME_MODE_EN && ta != NULL) {
            lv_keyboard_set_textarea(kb, ta);
        } else {
            lv_keyboard_set_textarea(kb, NULL);
        }
        if (cand_panel) {
            lv_obj_clear_flag(cand_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    py_clear();
    cand_offset = 0;
    cand_update();
}

/***********************
 * 对外接口1: 创建并显示中英双语键盘
 ***********************/
void Create_Keyboard(lv_obj_t * ta)
{
    create_keyboard_internal(ta, false);
}

/***********************
 * 对外接口1_EN: 创建并显示纯英文键盘
 ***********************/
void Create_Keyboard_EN(lv_obj_t * ta)
{
    create_keyboard_internal(ta, true);
}

/**********************
 * 内部函数：隐藏键盘
 ***********************/
static void close_keyboard_internal(void)
{
    if (kb == NULL) return;

    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(click_catcher, LV_OBJ_FLAG_HIDDEN);
    if (cand_panel)
        lv_obj_add_flag(cand_panel, LV_OBJ_FLAG_HIDDEN);

    lv_keyboard_set_textarea(kb, NULL);

    if (current_ta != NULL)
    {
        lv_obj_clear_state(current_ta, LV_STATE_FOCUSED);
        lv_indev_t * indev = lv_indev_get_act();
        if (indev)
            lv_indev_reset(indev, NULL);
        current_ta = NULL;
    }

    py_clear();
}

/***********************
 * 对外接口2: 刷新并拉取物理实体键盘状态
 * 需放到 lv_task_handler() 同级的循环内定期调用
 ***********************/
void Update_Keyboard(void)
{
    // 如果有实体物理键盘按键触发
    if (g_host_kbd_trigger) 
    {
        g_host_kbd_trigger = 0;
        
        // 仅在当前有焦点且屏幕键盘处于工作状态时，实体键盘才起作用
        if (current_ta == NULL || kb == NULL) return; 

        char txt[8] = {0};
        uint8_t key = g_host_kbd_key;
        uint8_t mod = g_host_kbd_mod;
        bool shift = (mod & 0x02) || (mod & 0x20); // 判断是否按住了左右 Shift

        // HID 键码映射到字符
        if (key >= 0x04 && key <= 0x1D) { // 字母 a-z
            txt[0] = (shift ? 'A' : 'a') + (key - 0x04);
        } else if (key >= 0x1E && key <= 0x26) { // 数字 1-9
            const char num_shift[] = "!@#$%^&*(";
            txt[0] = shift ? num_shift[key - 0x1E] : ('1' + (key - 0x1E));
        } else if (key == 0x27) { // 数字 0
            txt[0] = shift ? ')' : '0';
        } else if (key == 0x2A) { // Backspace
            strcpy(txt, LV_SYMBOL_BACKSPACE);
        } else if (key == 0x28) { // Enter
            strcpy(txt, "Enter");
        } else if (key == 0x2C) { // Space
            txt[0] = ' ';
        } else {
            // 其他常用符号映射
            switch(key) {
                case 0x2D: txt[0] = shift ? '_' : '-'; break;
                case 0x2E: txt[0] = shift ? '+' : '='; break;
                case 0x2F: txt[0] = shift ? '{' : '['; break;
                case 0x30: txt[0] = shift ? '}' : ']'; break;
                case 0x31: txt[0] = shift ? '|' : '\\'; break;
                case 0x33: txt[0] = shift ? ':' : ';'; break;
                case 0x34: txt[0] = shift ? '"' : '\''; break;
                case 0x36: txt[0] = shift ? '<' : ','; break;
                case 0x37: txt[0] = shift ? '>' : '.'; break;
                case 0x38: txt[0] = shift ? '?' : '/'; break;
            }
        }

        // 把实体键盘转换出来的字符，发给通用处理引擎，标记为 from_physical_kb = true
        if (strlen(txt) > 0) {
            handle_keyboard_input(txt, true);
        }
    }
}

/***********************
 * 对外接口3: 彻底移除键盘内存
 ***********************/
void Remove_Keyboard(void)
{
    if (kb != NULL) { lv_obj_del(kb); kb = NULL; }
    if (click_catcher != NULL) { lv_obj_del(click_catcher); click_catcher = NULL; }
    if (cand_panel != NULL) { lv_obj_del(cand_panel); cand_panel = NULL; }
    current_ta = NULL;

    pinyin_buf_deinit();
}
