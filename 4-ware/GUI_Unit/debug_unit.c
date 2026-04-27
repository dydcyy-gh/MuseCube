#include "debug_unit.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "malloc.h"
#include "lvgl.h"
#include "page_manager.h"
#include "variables.h"
#include "debug.h"

// 日志配置参数
#define MAX_LOG_LINES 52
#define MAX_LOG_LEN   128

static lv_obj_t *log_cont = NULL;
static lv_obj_t *log_labels[MAX_LOG_LINES] = {NULL};

static char (*log_buffer)[MAX_LOG_LEN] = NULL;

static uint16_t log_head = 0;
static uint16_t log_count = 0;
static uint8_t  need_update = 0;

volatile uint8_t tsdb_lvgl_need_dump = 0;

static void btn_back_event_cb(lv_event_t * e)
{
    debug_mode = Debug_Mode_None;
    Page_Request_Switch(PAGE_LOG_CTRL);
}

void Create_Debug_Unit(void)
{
    if (log_cont != NULL) return;

    if (log_buffer == NULL) {
        log_buffer = malloc_ccm(MAX_LOG_LINES * MAX_LOG_LEN);
    }
    if (log_buffer == NULL) {
        Remove_Debug_Unit();
        return;
    }

    log_head = 0;
    log_count = 0;
    need_update = 0;
    memset(log_buffer, 0, MAX_LOG_LINES * MAX_LOG_LEN);

    log_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(log_cont, 240, 180);
    lv_obj_center(log_cont);
    lv_obj_set_scrollbar_mode(log_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(log_cont, 5, LV_PART_MAIN);

    lv_obj_set_style_radius(log_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(log_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(log_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(log_cont, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_flex_flow(log_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(log_cont, 0, LV_PART_MAIN);

    for (int i = 0; i < MAX_LOG_LINES; i++) {
        log_labels[i] = lv_label_create(log_cont);
        lv_obj_set_style_text_font(log_labels[i], &lv_font_12, 0);

        lv_obj_set_height(log_labels[i], 16);
        lv_obj_set_width(log_labels[i], LV_SIZE_CONTENT);
        
        lv_label_set_text(log_labels[i], "");

        lv_obj_add_flag(log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t * btn_back = lv_btn_create(log_cont);
    lv_obj_set_size(btn_back, 20, 20);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_back, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_set_style_pad_all(btn_back, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_back, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_back, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);

    extern const lv_img_dsc_t exit_icon;
    lv_obj_t * lbl_back = lv_img_create(btn_back);
    lv_img_set_src(lbl_back, &exit_icon);
    lv_obj_center(lbl_back);
    lv_obj_set_style_img_recolor(lbl_back, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(lbl_back, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);

    if (tsdb_lvgl_need_dump) {
        tsdb_lvgl_need_dump = 0;
        Debug_Dump_TSDB_To_LVGL(50);
    }
}

void Update_Debug_Unit(void)
{
    if (!need_update || log_cont == NULL || log_buffer == NULL) return;

    need_update = 0;

    int start_idx = (log_count == MAX_LOG_LINES) ? log_head : 0;

    for (int i = 0; i < log_count; i++) {
        int idx = (start_idx + i) % MAX_LOG_LINES;
        lv_label_set_text_static(log_labels[i], log_buffer[idx]);
        lv_obj_clear_flag(log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = log_count; i < MAX_LOG_LINES; i++) {
        lv_obj_add_flag(log_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (lv_obj_get_state(log_cont) & LV_STATE_PRESSED) {
        return;
    }

    lv_obj_scroll_to_y(log_cont, LV_COORD_MAX, LV_ANIM_OFF);
    lv_obj_scroll_to_x(log_cont, 0, LV_ANIM_OFF);
}
void Remove_Debug_Unit(void)
{
    if (log_cont != NULL) {
        lv_obj_del(log_cont);
        log_cont = NULL;
        memset(log_labels, 0, sizeof(log_labels));
    }

    if (log_buffer != NULL) {
        free_ccm(log_buffer);
        log_buffer = NULL;
    }
}

void lvgl_printf(const char *format, ...)
{
    if (log_cont == NULL || log_buffer == NULL) return;
    if (__get_IPSR() != 0) return;

    char buf[128];
    va_list args;

    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf) - 2, format, args);
    va_end(args);

    if (len <= 0) return;

    if (len >= sizeof(buf) - 2) len = sizeof(buf) - 3;

    for (int i = 0; i < len; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') buf[i] = ' ';
    }

    buf[len++] = '\n';
    buf[len] = '\0';

    strncpy(log_buffer[log_head], buf, MAX_LOG_LEN - 1);
    log_buffer[log_head][MAX_LOG_LEN - 1] = '\0';

    log_head = (log_head + 1) % MAX_LOG_LINES;
    if (log_count < MAX_LOG_LINES) log_count++;

    need_update = 1;
}
