#include "calculator_unit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "page_manager.h"
#include "variables.h"
#include "malloc.h"

// ================= 数据结构 =================
typedef struct {
    char display_text[128]; // 用于存储完整的数学表达式
    bool is_evaluated;      // 标记当前显示的是否为计算结果
    bool is_error;          // 标记是否发生了错误(如除以0)
} calc_state_t;

static calc_state_t * calc_state = NULL;

// ================= UI 控件 =================
static lv_obj_t * main_cont = NULL;
static lv_obj_t * display_label = NULL;
static lv_obj_t * btnm = NULL;

static const char * btnm_map[] = {
    "AC",  "7", "8", "9", "/", "\n",
    "DEL", "4", "5", "6", "*", "\n",
    "+/-", "1", "2", "3", "-", "\n",
    "%",   ".", "0", "=", "+", ""
};

// ================= 数学表达式解析器 =================

static double parse_expression(const char **p);

// 解析基础因子 (数字、正负号、百分号)
static double parse_factor(const char **p) {
    double val = 0.0;
    int sign = 1;
    
    if (**p == '-') { sign = -1; (*p)++; }
    else if (**p == '+') { (*p)++; }

    char *end;
    val = strtod(*p, &end);
    *p = end;

    if (**p == '%') {
        val /= 100.0;
        (*p)++;
    }
    return val * sign;
}

// 解析乘除法
static double parse_term(const char **p) {
    double val = parse_factor(p);
    while (**p == '*' || **p == '/') {
        char op = **p;
        (*p)++;
        double next_val = parse_factor(p);
        if (op == '*') {
            val *= next_val;
        } else {
            if (next_val == 0) {
                if (calc_state) calc_state->is_error = true; 
                return 0; // 除零错误
            }
            val /= next_val;
        }
    }
    return val;
}

// 解析加减法
static double parse_expression(const char **p) {
    double val = parse_term(p);
    while (**p == '+' || **p == '-') {
        char op = **p;
        (*p)++;
        double next_val = parse_term(p);
        if (op == '+') val += next_val;
        else val -= next_val;
    }
    return val;
}

// ================= 逻辑函数 =================

static void calc_reset(void)
{
    if (!calc_state) return;
    calc_state->is_evaluated = false;
    calc_state->is_error = false;
    strcpy(calc_state->display_text, "0");
    if (display_label) lv_label_set_text(display_label, calc_state->display_text);
}

static void calc_execute(void)
{
    if (!calc_state || calc_state->is_error) return;

    const char *p = calc_state->display_text;
    calc_state->is_error = false;
    
    double result = parse_expression(&p);

    if (calc_state->is_error) {
        strcpy(calc_state->display_text, "Error");
    } else {
        // %g 自动去除多余的零
        snprintf(calc_state->display_text, sizeof(calc_state->display_text), "%g", result);
    }
    calc_state->is_evaluated = true;
}

static bool is_operator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}

// ================= 事件回调 =================

static void btnm_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    if (id == LV_BTNMATRIX_BTN_NONE || !calc_state) return;

    const char * txt = lv_btnmatrix_get_btn_text(obj, id);
    size_t len = strlen(calc_state->display_text);

    // 如果处于错误状态，除了 AC 其他键按下时自动复位
    if (calc_state->is_error) {
        if (strcmp(txt, "AC") != 0) calc_reset();
    }

    if (txt[0] >= '0' && txt[0] <= '9') {
        // 如果是数字
        if (calc_state->is_evaluated || strcmp(calc_state->display_text, "0") == 0) {
            strcpy(calc_state->display_text, txt);
            calc_state->is_evaluated = false;
        } else if (len < sizeof(calc_state->display_text) - 2) {
            strcat(calc_state->display_text, txt);
        }
    } 
    else if (strcmp(txt, ".") == 0) {
        if (calc_state->is_evaluated) {
            strcpy(calc_state->display_text, "0.");
            calc_state->is_evaluated = false;
        } else if (len < sizeof(calc_state->display_text) - 2) {
            // 简单防止连续两个点，实际应检查当前数字部分是否已有小数点
            if (calc_state->display_text[len-1] != '.') {
                strcat(calc_state->display_text, ".");
            }
        }
    }
    else if (strcmp(txt, "AC") == 0) {
        calc_reset();
    }
    else if (strcmp(txt, "DEL") == 0) {
        if (calc_state->is_evaluated) {
            calc_reset();
        } else if (len > 1) {
            calc_state->display_text[len - 1] = '\0';
            // 如果删的只剩负号，归零
            if (strcmp(calc_state->display_text, "-") == 0) {
                strcpy(calc_state->display_text, "0");
            }
        } else {
            strcpy(calc_state->display_text, "0");
        }
    }
    else if (strcmp(txt, "+/-") == 0) {
        if (calc_state->is_evaluated) calc_state->is_evaluated = false;
        
        if (strcmp(calc_state->display_text, "0") != 0) {
            // 找到最后一个非数字/小数点的字符位置
            int i = len - 1;
            while (i >= 0 && (isdigit((int)calc_state->display_text[i]) || calc_state->display_text[i] == '.' || calc_state->display_text[i] == '%')) {
                i--;
            }
            
            // 如果前一个是负号且(是表达式开头 或 它前面是运算符)，则去掉该负号(转为正)
            if (i >= 0 && calc_state->display_text[i] == '-' && (i == 0 || is_operator(calc_state->display_text[i-1]))) {
                memmove(&calc_state->display_text[i], &calc_state->display_text[i+1], len - i);
            } 
            else if (len < sizeof(calc_state->display_text) - 2) {
                // 否则，在当前数字前面插入一个负号
                memmove(&calc_state->display_text[i+2], &calc_state->display_text[i+1], len - i);
                calc_state->display_text[i+1] = '-';
            }
        }
    }
    else if (strcmp(txt, "%") == 0) {
        if (calc_state->is_evaluated) calc_state->is_evaluated = false;
        if (len < sizeof(calc_state->display_text) - 2 && !is_operator(calc_state->display_text[len-1])) {
            strcat(calc_state->display_text, "%");
        }
    }
    else if (strcmp(txt, "=") == 0) {
        calc_execute();
    } 
    else if (is_operator(txt[0])) {
        // 输入运算符 (+, -, *, /)
        calc_state->is_evaluated = false;
        
        if (len > 0) {
            char last_char = calc_state->display_text[len-1];
            if (is_operator(last_char)) {
                // 替换最后的运算符
                calc_state->display_text[len-1] = txt[0];
            } else if (len < sizeof(calc_state->display_text) - 2) {
                // 正常追加运算符
                strcat(calc_state->display_text, txt);
            }
        }
    }

    lv_label_set_text(display_label, calc_state->display_text);
}

// ================= 生命周期函数 =================

void Create_Calculator_Unit(void)
{
    if (main_cont != NULL) return;

    calc_state = (calc_state_t *)malloc_bsc(sizeof(calc_state_t));
    if (!calc_state) return;

    main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, 240, 180); 
    lv_obj_align(main_cont, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // --- 顶部显示区域容器 ---
    lv_obj_t * top_cont = lv_obj_create(main_cont);
    lv_obj_set_size(top_cont, 240, 45);
    lv_obj_align(top_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(top_cont, 0, 0);
    lv_obj_set_style_border_width(top_cont, 0, 0);
    lv_obj_set_style_pad_all(top_cont, 5, 0);
    lv_obj_clear_flag(top_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 数值显示标签 (右对齐)
    display_label = lv_label_create(top_cont);
    lv_obj_set_width(display_label, 230);
    // 使用 LV_LABEL_LONG_SCROLL_CIRCULAR 或类似模式可以在表达式很长时滚动
    lv_label_set_long_mode(display_label, LV_LABEL_LONG_CLIP); 
    lv_obj_set_style_text_align(display_label, LV_TEXT_ALIGN_RIGHT, 0); 
    lv_obj_set_style_text_color(display_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(display_label, &lv_font_16, 0); 
    lv_obj_align(display_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // --- 底部按键矩阵 ---
    btnm = lv_btnmatrix_create(main_cont);
    lv_obj_set_size(btnm, 240, 135); 
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_btnmatrix_set_map(btnm, btnm_map);
    
    // 美化按键矩阵
    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_pad_all(btnm, 2, 0);
    lv_obj_set_style_pad_row(btnm, 2, 0);
    lv_obj_set_style_pad_column(btnm, 2, 0);
    lv_obj_clear_flag(btnm, LV_OBJ_FLAG_SCROLLABLE);

    // 默认样式（淡灰底、黑字、直角0）
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0xE0E0E0), LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, lv_color_hex(0x000000), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, &lv_font_16, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);

    // 高亮右侧运算符列和等号
    lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CUSTOM_1); 
    lv_btnmatrix_set_btn_ctrl(btnm, 4, LV_BTNMATRIX_CTRL_CUSTOM_2);
    lv_btnmatrix_set_btn_ctrl(btnm, 9, LV_BTNMATRIX_CTRL_CUSTOM_2);
    lv_btnmatrix_set_btn_ctrl(btnm, 14, LV_BTNMATRIX_CTRL_CUSTOM_2);
    lv_btnmatrix_set_btn_ctrl(btnm, 19, LV_BTNMATRIX_CTRL_CUSTOM_2);
    lv_btnmatrix_set_btn_ctrl(btnm, 18, LV_BTNMATRIX_CTRL_CUSTOM_2);

    // 深灰色用于运算符列和等号
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0xD0D0D0), LV_PART_ITEMS | LV_STATE_DEFAULT | LV_BTNMATRIX_CTRL_CUSTOM_2);

    lv_obj_add_event_cb(btnm, btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 初始化计算器状态
    calc_reset();
}

void Update_Calculator_Unit(void)
{
}

void Remove_Calculator_Unit(void)
{
    if (main_cont) {
        lv_obj_del(main_cont);
        main_cont = NULL;
    }
    display_label = NULL;
    btnm = NULL;

    if (calc_state) {
        free_bsc(calc_state);
        calc_state = NULL;
    }
}
