#include "cmd_unit.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "malloc.h"
#include "lvgl.h"
#include "page_manager.h"
#include "variables.h"
#include "defines.h"
#include "shell.h"
#include "keyboard.h"

#define MAX_CMD_LINES  60   // 最大保留行数
#define MAX_CMD_LEN    80   // 单行最大字符数

// ================= 数据结构 =================
// 将所有页面状态、组件指针、缓冲指针全部封装进结构体中
typedef struct {
    Shell shell_obj;
    char shell_buf[256];

    // UI 控件指针
    lv_obj_t * main_cont;
    lv_obj_t * log_cont;
    lv_obj_t * ta_input;
    lv_obj_t * btn_close;
    lv_obj_t * btn_tab;
    lv_obj_t * log_labels[MAX_CMD_LINES];

    // 日志缓存 (指向 CCM 内存区)
    char (*shell_log_buffer)[MAX_CMD_LEN];

    // 环形队列状态
    uint16_t log_head;
    uint16_t log_col;
    uint16_t log_count;
    uint8_t  need_update;

    // 子页面状态
    uint8_t  sub_page;      // 0=模式选择, 1=Shell
    lv_obj_t *mode_cont;    // 模式选择容器
    lv_obj_t *usb_cont;     // USB模式容器 (三态更新用)

} cmd_state_t;

// 全局只需保留这一个环境指针
static cmd_state_t * cmd_state = NULL;

// ================= Shell 移植接口 =================

static signed short lvgl_shell_write(char *data, unsigned short len)
{
    if (!cmd_state || cmd_state->log_cont == NULL || data == NULL || len == 0 || cmd_state->shell_log_buffer == NULL) return 0;
    
    for (int i = 0; i < len; i++) {
        if (data[i] == '\r') {
            continue; 
        } 
        else if (data[i] == '\b') {
            if (cmd_state->log_col > 0) {
                cmd_state->log_col--;
                cmd_state->shell_log_buffer[cmd_state->log_head][cmd_state->log_col] = '\0';
            }
        } 
        else if (data[i] == '\n') {
            cmd_state->log_head = (cmd_state->log_head + 1) % MAX_CMD_LINES;
            if (cmd_state->log_count < MAX_CMD_LINES) cmd_state->log_count++;
            cmd_state->log_col = 0;
            cmd_state->shell_log_buffer[cmd_state->log_head][cmd_state->log_col] = '\0';
        }
        else if (data[i] == '\033' || data[i] == '\x1B') {
            i++;
            if (i < len && data[i] == '[') {
                i++;
                while (i < len && ((data[i] >= '0' && data[i] <= '9') || data[i] == ';')) {
                    i++;
                }
            }
        } 
        else {
            if (cmd_state->log_col < MAX_CMD_LEN - 1) {
                cmd_state->shell_log_buffer[cmd_state->log_head][cmd_state->log_col++] = data[i];
                cmd_state->shell_log_buffer[cmd_state->log_head][cmd_state->log_col] = '\0'; 
            }
        }
    }
    
    cmd_state->need_update = 1;
    return len;
}

// ================= 前向声明 =================
static void btn_close_cb(lv_event_t * e);
static void btn_tab_cb(lv_event_t * e);
static void ta_input_event_cb(lv_event_t * e);
static void ta_input_focus_cb(lv_event_t * e);
static void ta_input_defocus_cb(lv_event_t * e);

// ================= 辅助函数 =================

static bool is_usb_device_connected(void)
{
    return (g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY);
}

/* USB 容器三态刷新：不可点击(灰) / 未开启(绿) / 已开启(蓝，可点击关闭) */
static void update_usb_cont_state(void)
{
    if (!cmd_state || !cmd_state->usb_cont) return;

    lv_obj_t * cont = cmd_state->usb_cont;
    lv_obj_t * label = lv_obj_get_child(cont, 0);

    if (g_usb_function == USBD_CMD) {
        /* 已开启，可点击关闭 */
        lv_obj_clear_state(cont, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0xBBDEFB), 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(0x1976D2), 0);
        if (label) lv_label_set_text(label, "USB已启动\n点击关闭");
    } else if (!is_usb_device_connected()) {
        /* 不可点击 */
        lv_obj_add_state(cont, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0xE0E0E0), 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(0xAAAAAA), 0);
        if (label) lv_label_set_text(label, "使用USB启动\nLetter Shell");
    } else {
        /* 未开启，可点击启动 */
        lv_obj_clear_state(cont, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(cont, lv_color_hex(0xE8F5E9), 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(0x4CAF50), 0);
        if (label) lv_label_set_text(label, "使用USB启动\nLetter Shell");
    }
}

static void enter_shell_subpage(void)
{
    if (!cmd_state) return;

    /* 销毁模式选择UI */
    if (cmd_state->btn_close) {
        lv_obj_del(cmd_state->btn_close);
        cmd_state->btn_close = NULL;
    }
    if (cmd_state->mode_cont) {
        lv_obj_del(cmd_state->mode_cont);
        cmd_state->mode_cont = NULL;
    }

    /* 创建 Shell UI */
    cmd_state->main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cmd_state->main_cont, 240, 210);
    lv_obj_set_pos(cmd_state->main_cont, 0, 0);
    lv_obj_set_style_bg_color(cmd_state->main_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(cmd_state->main_cont, 0, 0);
    lv_obj_set_style_border_width(cmd_state->main_cont, 0, 0);
    lv_obj_set_style_radius(cmd_state->main_cont, 0, 0);

    lv_obj_add_flag(cmd_state->main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(cmd_state->main_cont, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(cmd_state->main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(cmd_state->main_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);

    cmd_state->log_cont = lv_obj_create(cmd_state->main_cont);
    lv_obj_set_size(cmd_state->log_cont, 360, 190);
    lv_obj_set_pos(cmd_state->log_cont, 0, 0);
    lv_obj_set_style_bg_color(cmd_state->log_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cmd_state->log_cont, 0, 0);
    lv_obj_set_style_radius(cmd_state->log_cont, 0, 0);
    lv_obj_set_style_pad_all(cmd_state->log_cont, 2, 0);

    lv_obj_set_flex_flow(cmd_state->log_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cmd_state->log_cont, 0, 0);
    lv_obj_set_scrollbar_mode(cmd_state->log_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(cmd_state->log_cont, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    for (int i = 0; i < MAX_CMD_LINES; i++) {
        cmd_state->log_labels[i] = lv_label_create(cmd_state->log_cont);
        lv_obj_set_style_text_font(cmd_state->log_labels[i], &lv_font_12, 0);
        lv_obj_set_style_text_color(cmd_state->log_labels[i], lv_color_hex(0x000000), 0);
        lv_obj_set_width(cmd_state->log_labels[i], LV_SIZE_CONTENT);
        lv_label_set_text(cmd_state->log_labels[i], "");
        lv_obj_add_flag(cmd_state->log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    cmd_state->btn_tab = lv_btn_create(cmd_state->main_cont);
    lv_obj_set_size(cmd_state->btn_tab, 40, 20);
    lv_obj_set_pos(cmd_state->btn_tab, 0, 190);
    lv_obj_set_style_bg_color(cmd_state->btn_tab, lv_color_hex(0xDCDCDC), 0);
    lv_obj_set_style_shadow_width(cmd_state->btn_tab, 0, 0);
    lv_obj_set_style_border_width(cmd_state->btn_tab, 1, 0);
    lv_obj_set_style_border_color(cmd_state->btn_tab, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_radius(cmd_state->btn_tab, 0, 0);
    lv_obj_set_style_pad_all(cmd_state->btn_tab, 0, 0);

    lv_obj_set_style_transform_width(cmd_state->btn_tab, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(cmd_state->btn_tab, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transition(cmd_state->btn_tab, NULL, LV_STATE_DEFAULT);
    lv_obj_clear_flag(cmd_state->btn_tab, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_t * lbl_tab = lv_label_create(cmd_state->btn_tab);
    lv_label_set_text(lbl_tab, "Tab");
    lv_obj_set_style_text_color(lbl_tab, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_tab, &lv_font_12, 0);
    lv_obj_center(lbl_tab);
    lv_obj_add_event_cb(cmd_state->btn_tab, btn_tab_cb, LV_EVENT_CLICKED, NULL);

    cmd_state->ta_input = lv_textarea_create(cmd_state->main_cont);
    lv_obj_set_size(cmd_state->ta_input, 320, 20);
    lv_obj_set_pos(cmd_state->ta_input, 40, 190);
    lv_obj_set_style_bg_color(cmd_state->ta_input, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_text_color(cmd_state->ta_input, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cmd_state->ta_input, 0, 0);
    lv_obj_set_style_radius(cmd_state->ta_input, 0, 0);
    lv_obj_set_style_text_font(cmd_state->ta_input, &lv_font_12, 0);
    lv_obj_set_style_pad_all(cmd_state->ta_input, 2, 0);
    lv_obj_set_scrollbar_mode(cmd_state->ta_input, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(cmd_state->ta_input, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_add_event_cb(cmd_state->ta_input, ta_input_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(cmd_state->ta_input, ta_input_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(cmd_state->ta_input, ta_input_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    cmd_state->btn_close = lv_btn_create(lv_scr_act());
    lv_obj_set_size(cmd_state->btn_close, 30, 30);
    lv_obj_align(cmd_state->btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(cmd_state->btn_close, lv_color_hex(0xADD8E6), 0);
    lv_obj_set_style_bg_opa(cmd_state->btn_close, LV_OPA_50, 0);
    lv_obj_set_style_shadow_width(cmd_state->btn_close, 0, 0);
    lv_obj_set_style_border_width(cmd_state->btn_close, 0, 0);

    lv_obj_t * lbl_close = lv_label_create(cmd_state->btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0x000000), 0);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(cmd_state->btn_close, btn_close_cb, LV_EVENT_CLICKED, NULL);

    cmd_state->shell_obj.write = lvgl_shell_write;
    cmd_state->shell_obj.read = NULL;
    shellInit(&cmd_state->shell_obj, cmd_state->shell_buf, sizeof(cmd_state->shell_buf));
    cmd_state->shell_obj.status.isChecked = 1;
    cmd_state->shell_obj.status.isActive = 1;

    lv_obj_add_state(cmd_state->ta_input, LV_STATE_FOCUSED);

    cmd_state->sub_page = 1;
}

// ================= 事件回调 =================

static void btn_close_cb(lv_event_t * e)
{
    Page_Back();
}

static void usb_mode_cb(lv_event_t * e)
{
    if (!cmd_state) return;
    if (g_usb_function == USBD_CMD) {
        g_usb_function = USB_NONE;
    } else if (is_usb_device_connected()) {
        g_usb_function = USBD_CMD;
    }
    update_usb_cont_state();
}

static void direct_mode_cb(lv_event_t * e)
{
    if (!cmd_state) return;
    enter_shell_subpage();
}

static void btn_tab_cb(lv_event_t * e)
{
    if (cmd_state) {
        shellHandler(&cmd_state->shell_obj, '\t');
    }
}

static void ta_input_event_cb(lv_event_t * e)
{
    lv_obj_t * ta = lv_event_get_target(e);
    const char * txt = lv_textarea_get_text(ta);
    size_t len = strlen(txt);

    if (len > 0 && txt[len - 1] == '\n') {
        char * cmd = (char *)malloc_bsc(len);
        if (cmd) {
            strncpy(cmd, txt, len - 1);
            cmd[len - 1] = '\0';

            if (cmd_state) {
                for (size_t i = 0; i < len - 1; i++) {
                    shellHandler(&cmd_state->shell_obj, cmd[i]);
                }
                shellHandler(&cmd_state->shell_obj, '\r');
            }

            free_bsc(cmd);
        }
        lv_textarea_set_text(ta, "");
    }
}

static void ta_input_focus_cb(lv_event_t * e)
{
    if (!cmd_state) return;
    
    lv_obj_set_height(cmd_state->log_cont, 120);
    lv_obj_set_y(cmd_state->ta_input, 120);
    if (cmd_state->btn_tab) lv_obj_set_y(cmd_state->btn_tab, 120); 
    
    lv_obj_scroll_to_y(cmd_state->log_cont, LV_COORD_MAX, LV_ANIM_OFF);
    Create_Keyboard_EN(cmd_state->ta_input);
    lv_obj_scroll_to_x(cmd_state->main_cont, 0, LV_ANIM_OFF);
}

static void ta_input_defocus_cb(lv_event_t * e)
{
    if (!cmd_state) return;
    
    lv_obj_set_height(cmd_state->log_cont, 190);
    lv_obj_set_y(cmd_state->ta_input, 190);
    if (cmd_state->btn_tab) lv_obj_set_y(cmd_state->btn_tab, 190); 
}

// ================= 生命周期函数 =================

void Create_Cmd_Unit(void)
{
    if (cmd_state) return;

    cmd_state = (cmd_state_t *)malloc_bsc(sizeof(cmd_state_t));
    if (!cmd_state) return;
    memset(cmd_state, 0, sizeof(cmd_state_t));

    cmd_state->shell_log_buffer = malloc_ccm(MAX_CMD_LINES * MAX_CMD_LEN);
    if (!cmd_state->shell_log_buffer) {
        free_bsc(cmd_state);
        cmd_state = NULL;
        return;
    }
    memset(cmd_state->shell_log_buffer, 0, MAX_CMD_LINES * MAX_CMD_LEN);

    /* ================= 模式选择 UI ================= */

    cmd_state->mode_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cmd_state->mode_cont, 240, 210);
    lv_obj_set_pos(cmd_state->mode_cont, 0, 0);
    lv_obj_set_style_bg_color(cmd_state->mode_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(cmd_state->mode_cont, 0, 0);
    lv_obj_set_style_border_width(cmd_state->mode_cont, 0, 0);
    lv_obj_set_style_radius(cmd_state->mode_cont, 0, 0);

    /* 关闭按钮 */
    cmd_state->btn_close = lv_btn_create(lv_scr_act());
    lv_obj_set_size(cmd_state->btn_close, 30, 30);
    lv_obj_align(cmd_state->btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(cmd_state->btn_close, lv_color_hex(0xADD8E6), 0);
    lv_obj_set_style_bg_opa(cmd_state->btn_close, LV_OPA_50, 0);
    lv_obj_set_style_shadow_width(cmd_state->btn_close, 0, 0);
    lv_obj_set_style_border_width(cmd_state->btn_close, 0, 0);

    lv_obj_t * lbl_close = lv_label_create(cmd_state->btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0x000000), 0);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(cmd_state->btn_close, btn_close_cb, LV_EVENT_CLICKED, NULL);

    /* USB 模式按钮 */
    cmd_state->usb_cont = lv_btn_create(cmd_state->mode_cont);
    lv_obj_set_size(cmd_state->usb_cont, 200, 60);
    lv_obj_align(cmd_state->usb_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_shadow_width(cmd_state->usb_cont, 0, 0);
    lv_obj_set_style_border_width(cmd_state->usb_cont, 2, 0);
    lv_obj_set_style_radius(cmd_state->usb_cont, 8, 0);

    lv_obj_t * usb_label = lv_label_create(cmd_state->usb_cont);
    lv_obj_set_style_text_color(usb_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(usb_label, &lv_font_16, 0);
    lv_obj_center(usb_label);
    lv_obj_add_event_cb(cmd_state->usb_cont, usb_mode_cb, LV_EVENT_CLICKED, NULL);

    /* 直接启动按钮 */
    lv_obj_t * direct_cont = lv_btn_create(cmd_state->mode_cont);
    lv_obj_set_size(direct_cont, 200, 60);
    lv_obj_align(direct_cont, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_style_shadow_width(direct_cont, 0, 0);
    lv_obj_set_style_bg_color(direct_cont, lv_color_hex(0xE3F2FD), 0);
    lv_obj_set_style_border_color(direct_cont, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(direct_cont, 2, 0);
    lv_obj_set_style_radius(direct_cont, 8, 0);

    lv_obj_t * direct_label = lv_label_create(direct_cont);
    lv_label_set_text(direct_label, "直接启动\nLetter Shell");
    lv_obj_set_style_text_color(direct_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(direct_label, &lv_font_16, 0);
    lv_obj_center(direct_label);
    lv_obj_add_event_cb(direct_cont, direct_mode_cb, LV_EVENT_CLICKED, NULL);

    update_usb_cont_state();

    cmd_state->sub_page = 0;
}

void Update_Cmd_Unit(void)
{
    if (!cmd_state) return;

	Update_Keyboard();
		
    if (cmd_state->sub_page == 0) {
        update_usb_cont_state();
        return;
    }

    /* Shell 页面：更新日志显示 */
    if (!cmd_state->need_update || cmd_state->log_cont == NULL || cmd_state->shell_log_buffer == NULL) return;
    cmd_state->need_update = 0;

    int start_idx = (cmd_state->log_count == MAX_CMD_LINES) ? ((cmd_state->log_head + 1) % MAX_CMD_LINES) : 0;
    int display_count = (cmd_state->log_count == MAX_CMD_LINES) ? MAX_CMD_LINES : (cmd_state->log_head + 1);

    for (int i = 0; i < display_count; i++) {
        int idx = (start_idx + i) % MAX_CMD_LINES;
        lv_label_set_text_static(cmd_state->log_labels[i], cmd_state->shell_log_buffer[idx]);
        lv_obj_clear_flag(cmd_state->log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = display_count; i < MAX_CMD_LINES; i++) {
        lv_obj_add_flag(cmd_state->log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (lv_obj_get_state(cmd_state->log_cont) & LV_STATE_PRESSED) {
        return;
    }

    lv_obj_scroll_to_y(cmd_state->log_cont, LV_COORD_MAX, LV_ANIM_OFF);
}

void Remove_Cmd_Unit(void)
{
    Remove_Keyboard();

    if (cmd_state) {
        if (cmd_state->btn_close) {
            lv_obj_del(cmd_state->btn_close);
        }

        if (cmd_state->sub_page == 0) {
            /* 模式选择页面：删除模式容器 */
            if (cmd_state->mode_cont) {
                lv_obj_del(cmd_state->mode_cont);
            }
        } else {
            /* Shell 页面：删除主容器 */
            if (cmd_state->main_cont) {
                lv_obj_del(cmd_state->main_cont);
            }
        }

        if (cmd_state->shell_log_buffer) {
            free_ccm(cmd_state->shell_log_buffer);
        }

        free_bsc(cmd_state);
        cmd_state = NULL;
    }
}
