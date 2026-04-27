#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "variables.h"
#include "defines.h"
#include "pinyin_find.h"

#define CAND_NUM 7
#define PY_BUF_MAX 16

// 引用外部 USB 键盘状态变量
extern volatile uint8_t g_usb_function;
extern volatile uint8_t g_usb_kbd_modifier;
extern volatile uint8_t g_usb_kbd_key;
extern volatile uint8_t g_usb_kbd_trigger;

typedef enum
{
    IME_MODE_EN = 0,
    IME_MODE_CN
} ime_mode_t;

static ime_mode_t ime_mode = IME_MODE_CN;

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

// 内部隐藏函数，替代原来的 Hide_Keyboard
static void close_keyboard_internal(void);

// ========================================================
// ASCII -> USB HID 键码映射函数
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

/***********************
 * 键盘事件
 ***********************/
static void kb_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint16_t id = lv_keyboard_get_selected_btn(kb);
        const char *txt = lv_keyboard_get_btn_text(kb, id);

        map_and_send_usb_key(txt);

        if (ime_mode == IME_MODE_CN)
        {
            if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0)
            {
                if (py_len > 0) {
                    py_len--;
                    py_buf[py_len] = 0;
                    cand_offset = 0;
                    cand_update();
                } else {
                    if (current_ta) lv_textarea_del_char(current_ta);
                }
                return;
            }

            if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, "Enter") == 0)
            {
                if (current_ta) lv_textarea_add_text(current_ta, "\n");
                return;
            }

            if (strcmp(txt, "ABC") == 0 || strcmp(txt, "!#1") == 0 ||
                strcmp(txt, "?123") == 0 || strcmp(txt, "&123") == 0)
            {
                return;
            }

			if (strlen(txt) == 1) {
				char c = txt[0];
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
					if (c >= 'A' && c <= 'Z') {
						c = c - 'A' + 'a';
					}
					py_add(c);
					cand_offset = 0;
					cand_update();
					return;
				}
			}
			
            if (current_ta) lv_textarea_add_text(current_ta, txt);
            return;
        }
    }

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        close_keyboard_internal();
    }
}

/***********************
 * 对外接口1: 创建并显示键盘
 ***********************/
void Create_Keyboard(lv_obj_t * ta)
{
    // 随用随创建：如果对象还不存在，则初始化界面
    if (kb == NULL) 
    {
        click_catcher = lv_obj_create(lv_layer_top());
        lv_obj_set_size(click_catcher, 240, 240);
        lv_obj_set_pos(click_catcher, 0, 0);
        lv_obj_set_style_bg_opa(click_catcher, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(click_catcher, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(click_catcher, catcher_click_cb, LV_EVENT_CLICKED, NULL);

        cand_panel = lv_obj_create(lv_layer_top());
        lv_obj_set_size(cand_panel, 240, 24);
        lv_obj_align(cand_panel, LV_ALIGN_BOTTOM_MID, 0, -120);
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
        lv_obj_set_size(kb, 240, 120);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
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

    /* 以下部分相当于原本 Show_Keyboard 的功能 */
    current_ta = ta;

    if (ime_mode == IME_MODE_EN && ta != NULL) {
        lv_keyboard_set_textarea(kb, ta);
    } else {
        lv_keyboard_set_textarea(kb, NULL);
    }

    py_clear();
    cand_offset = 0;
    cand_update();

    // 确保对象可见
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(click_catcher, LV_OBJ_FLAG_HIDDEN);
    if (cand_panel) {
        lv_obj_clear_flag(cand_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

/***********************
 * 内部函数：隐藏键盘
 * (不立刻删除，仅隐藏)
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
 * 对外接口2: 刷新状态(留空备用)
 ***********************/
void Update_Keyboard(void) {}

/***********************
 * 对外接口3: 彻底移除键盘内存
 ***********************/
void Remove_Keyboard(void)
{
    if (kb != NULL) { lv_obj_del(kb); kb = NULL; }
    if (click_catcher != NULL) { lv_obj_del(click_catcher); click_catcher = NULL; }
    if (cand_panel != NULL) { lv_obj_del(cand_panel); cand_panel = NULL; }
    current_ta = NULL;
}
