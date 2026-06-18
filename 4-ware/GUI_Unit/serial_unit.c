#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "variables.h"
#include "defines.h"
#include "keyboard.h"
#include "malloc.h"
#include "usbh_serial_conf.h" 

// 声明外部字体，均支持中文
LV_FONT_DECLARE(lv_font_12);
LV_FONT_DECLARE(lv_font_16);
LV_FONT_DECLARE(lv_font_24);

#define RX_BUF_SIZE 512
#define MAX_TA_LEN  2000 // 接收框超过此长度自动清空，防止LVGL卡顿

// 预设波特率列表
static const uint32_t baud_rates[] = {9600, 115200, 460800, 1000000};
#define BAUD_RATE_COUNT (sizeof(baud_rates) / sizeof(baud_rates[0]))

// 串口助手的UI句柄结构体
typedef struct {
    lv_obj_t * root_scr;      
    lv_obj_t * top_cont;
    lv_obj_t * rx_cont;
    lv_obj_t * bot_cont;

    lv_obj_t * btn_open;      
    lv_obj_t * lbl_btn_open;  
    lv_obj_t * btn_baud;      // 波特率切换按钮
    lv_obj_t * lbl_baud;      // 波特率显示标签
    lv_obj_t * lbl_status;    
    lv_obj_t * ta_rx;         
    lv_obj_t * ta_tx;         
    lv_obj_t * btn_send;      
    
    uint8_t  baud_idx;        // 当前波特率索引
    bool is_opened;           
    bool is_connected;        
    uint8_t * rx_buf;         
} serial_ui_t;

static serial_ui_t * s_ui = NULL;

// ================= 事件回调 =================

// 波特率切换按钮回调
static void btn_baud_event_cb(lv_event_t * e)
{
    if (!s_ui) return;

    // 循环切换索引
    s_ui->baud_idx = (s_ui->baud_idx + 1) % BAUD_RATE_COUNT;
    lv_label_set_text_fmt(s_ui->lbl_baud, "%lu", baud_rates[s_ui->baud_idx]);

    // 如果串口处于打开状态，立即动态应用新波特率
    if (s_ui->is_opened) {
        app_usb_serial_config(baud_rates[s_ui->baud_idx], 8, 0, 0);
    }
}

// 打开/关闭按钮回调
static void btn_open_event_cb(lv_event_t * e)
{
    if (!s_ui) return;

    if (!s_ui->is_opened) 
    {
        if (app_usb_serial_is_connected()) 
        {
            if (app_usb_serial_open() == 0) 
            {
                uint32_t baud = baud_rates[s_ui->baud_idx];
                app_usb_serial_config(baud, 8, 0, 0);
                
                s_ui->is_opened = true;
                lv_label_set_text(s_ui->lbl_btn_open, "关闭");
                lv_label_set_text(s_ui->lbl_status, "已打开");
                lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0x00A000), 0);
            }
        }
    } 
    else 
    {
        app_usb_serial_close();
        s_ui->is_opened = false;
        lv_label_set_text(s_ui->lbl_btn_open, "打开");
        lv_label_set_text(s_ui->lbl_status, "已连接");
        lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0x000000), 0);
    }
}

// 发送按钮回调
static void btn_send_event_cb(lv_event_t * e)
{
    if (!s_ui || !s_ui->is_opened) return;

    const char * tx_text = lv_textarea_get_text(s_ui->ta_tx);
    size_t len = strlen(tx_text);
    if (len > 0) 
    {
        app_usb_serial_send((const uint8_t *)tx_text, len);
        lv_textarea_set_text(s_ui->ta_tx, ""); // 发送后清空
    }
}

// 文本框输入事件 (拦截回车键，实现直接发送)
static void ta_tx_event_cb(lv_event_t * e)
{
    lv_obj_t * ta = lv_event_get_target(e);
    const char * txt = lv_textarea_get_text(ta);
    size_t len = strlen(txt);

    // 如果检测到输入了换行符，代表按下了“回车”，直接发送并清空
    if (len > 0 && txt[len - 1] == '\n') 
    {
        if (len > 1 && s_ui && s_ui->is_opened) {
            app_usb_serial_send((const uint8_t *)txt, len - 1);
        }
        lv_textarea_set_text(ta, "");
    }
}

// 键盘获取焦点：UI收缩
static void ta_tx_focus_cb(lv_event_t * e)
{
    if (!s_ui) return;
    
    // 假设键盘占用高度70像素，缩小显示区，上移底边栏
    lv_obj_set_height(s_ui->rx_cont, 68);
    lv_obj_set_height(s_ui->ta_rx, 68);
    lv_obj_set_y(s_ui->bot_cont, 100);
    
    // 滚动光标到底部
    lv_textarea_set_cursor_pos(s_ui->ta_rx, LV_TEXTAREA_CURSOR_LAST);
    
    // 呼出键盘
    Create_Keyboard_EN(s_ui->ta_tx);
}

// 键盘失去焦点：UI展开
static void ta_tx_defocus_cb(lv_event_t * e)
{
    if (!s_ui) return;
    
    // 恢复原来的尺寸和位置
    lv_obj_set_height(s_ui->rx_cont, 138);
    lv_obj_set_height(s_ui->ta_rx, 138);
    lv_obj_set_y(s_ui->bot_cont, 170);
}

// ================= 生命周期函数 =================

void Create_Serial_Unit(void)
{
    if (s_ui != NULL) return; 

    // 1. 动态分配内存
    s_ui = (serial_ui_t *)malloc_bsc(sizeof(serial_ui_t));
    if (!s_ui) return;
    memset(s_ui, 0, sizeof(serial_ui_t));

    s_ui->rx_buf = (uint8_t *)malloc_bsc(RX_BUF_SIZE);
    if (!s_ui->rx_buf) {
        free_bsc(s_ui);
        s_ui = NULL;
        return;
    }

    s_ui->baud_idx = 1; // 默认索引 1 -> 115200

    // 2. 创建主页面容器 (240x210) 贴顶显示
    s_ui->root_scr = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_ui->root_scr, 240, 210);
    lv_obj_align(s_ui->root_scr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_ui->root_scr, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_scrollbar_mode(s_ui->root_scr, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    lv_obj_set_style_pad_all(s_ui->root_scr, 0, 0); 
    lv_obj_set_style_border_width(s_ui->root_scr, 0, 0);
    lv_obj_set_style_radius(s_ui->root_scr, 0, 0);
    lv_obj_set_style_text_font(s_ui->root_scr, &lv_font_12, 0); 

    /* ==========================================
       3. 顶部容器 (固定宽 240)
       ========================================== */
    s_ui->top_cont = lv_obj_create(s_ui->root_scr);
    lv_obj_set_size(s_ui->top_cont, 240, 32);
    lv_obj_set_pos(s_ui->top_cont, 0, 0); 
    lv_obj_clear_flag(s_ui->top_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_ui->top_cont, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    lv_obj_set_style_pad_all(s_ui->top_cont, 2, 0);
    lv_obj_set_style_border_width(s_ui->top_cont, 0, 0);

    // 打开按钮
    s_ui->btn_open = lv_btn_create(s_ui->top_cont);
    lv_obj_set_size(s_ui->btn_open, 46, 28);
    lv_obj_align(s_ui->btn_open, LV_ALIGN_LEFT_MID, 0, 0); 
    lv_obj_set_style_shadow_width(s_ui->btn_open, 0, 0); 

    s_ui->lbl_btn_open = lv_label_create(s_ui->btn_open);
    lv_label_set_text(s_ui->lbl_btn_open, "打开");
    lv_obj_center(s_ui->lbl_btn_open);
    lv_obj_add_event_cb(s_ui->btn_open, btn_open_event_cb, LV_EVENT_CLICKED, NULL);

    // 波特率切换按钮 (相对按钮对齐)
    s_ui->btn_baud = lv_btn_create(s_ui->top_cont);
    lv_obj_set_size(s_ui->btn_baud, 70, 28);
    lv_obj_align_to(s_ui->btn_baud, s_ui->btn_open, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    lv_obj_set_style_shadow_width(s_ui->btn_baud, 0, 0); 
    lv_obj_set_style_bg_color(s_ui->btn_baud, lv_color_hex(0xE0E0E0), 0); // 区分颜色
    lv_obj_set_style_text_color(s_ui->btn_baud, lv_color_hex(0x000000), 0);
    lv_obj_add_event_cb(s_ui->btn_baud, btn_baud_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui->lbl_baud = lv_label_create(s_ui->btn_baud);
    lv_label_set_text_fmt(s_ui->lbl_baud, "%lu", baud_rates[s_ui->baud_idx]);
    lv_obj_set_style_text_font(s_ui->lbl_baud, &lv_font_12, 0);
    lv_obj_center(s_ui->lbl_baud);

    // 状态标签
    s_ui->lbl_status = lv_label_create(s_ui->top_cont);
    lv_obj_align(s_ui->lbl_status, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_label_set_text(s_ui->lbl_status, "未连接");
    lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0xA00000), 0);

    /* ==========================================
       4. 中间接收区 (容器宽 240, 文本宽 360 实现左右滚动)
       ========================================== */
    s_ui->rx_cont = lv_obj_create(s_ui->root_scr);
    lv_obj_set_size(s_ui->rx_cont, 240, 138); 
    lv_obj_set_pos(s_ui->rx_cont, 0, 32);     // 紧贴 top_cont 下方
    lv_obj_add_flag(s_ui->rx_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_ui->rx_cont, LV_DIR_HOR); // 仅允许外部容器横向滚动
    lv_obj_set_scrollbar_mode(s_ui->rx_cont, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    lv_obj_set_style_pad_all(s_ui->rx_cont, 0, 0);
    lv_obj_set_style_border_width(s_ui->rx_cont, 0, 0);
    
    // 显示文本框 (宽度撑满到360)
    s_ui->ta_rx = lv_textarea_create(s_ui->rx_cont);
    lv_obj_set_size(s_ui->ta_rx, 360, 138); // 宽度360，高度匹配容器
    lv_obj_set_pos(s_ui->ta_rx, 0, 0);
    lv_obj_set_style_text_font(s_ui->ta_rx, &lv_font_12, 0); 
    lv_obj_set_style_pad_all(s_ui->ta_rx, 2, 0);
    lv_obj_set_style_shadow_width(s_ui->ta_rx, 0, 0); 
    lv_obj_set_style_border_width(s_ui->ta_rx, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui->ta_rx, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    lv_textarea_set_max_length(s_ui->ta_rx, MAX_TA_LEN + 100);
    lv_textarea_set_text(s_ui->ta_rx, "");
    lv_obj_clear_flag(s_ui->ta_rx, LV_OBJ_FLAG_CLICKABLE); // 设为只读

    /* ==========================================
       5. 底部发送区 (固定宽 240)
       ========================================== */
    s_ui->bot_cont = lv_obj_create(s_ui->root_scr);
    lv_obj_set_size(s_ui->bot_cont, 240, 40);
    lv_obj_set_pos(s_ui->bot_cont, 0, 170); // 初始Y坐标 170 (210 - 40)
    lv_obj_clear_flag(s_ui->bot_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_ui->bot_cont, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    lv_obj_set_style_pad_all(s_ui->bot_cont, 2, 0);
    lv_obj_set_style_border_width(s_ui->bot_cont, 0, 0);

    // 发送按钮 
    s_ui->btn_send = lv_btn_create(s_ui->bot_cont);
    lv_obj_set_size(s_ui->btn_send, 50, 36);
    lv_obj_align(s_ui->btn_send, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_shadow_width(s_ui->btn_send, 0, 0); 
    lv_obj_add_event_cb(s_ui->btn_send, btn_send_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_send = lv_label_create(s_ui->btn_send);
    lv_label_set_text(lbl_send, "发送");
    lv_obj_center(lbl_send);

    // 发送输入框 
    s_ui->ta_tx = lv_textarea_create(s_ui->bot_cont);
    lv_obj_set_size(s_ui->ta_tx, 178, 36);
    lv_obj_align_to(s_ui->ta_tx, s_ui->btn_send, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    lv_textarea_set_one_line(s_ui->ta_tx, false); // 允许框内上下滚动和多行，不可横向滚
    lv_textarea_set_text(s_ui->ta_tx, "");
    lv_obj_set_style_pad_all(s_ui->ta_tx, 4, 0);
    lv_obj_set_style_shadow_width(s_ui->ta_tx, 0, 0); 
    lv_obj_set_scrollbar_mode(s_ui->ta_tx, LV_SCROLLBAR_MODE_OFF); // 隐藏滚动条
    
    // 绑定事件：值改变(用于发)、聚焦(用于缩放)、失焦(用于恢复)
    lv_obj_add_event_cb(s_ui->ta_tx, ta_tx_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui->ta_tx, ta_tx_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_ui->ta_tx, ta_tx_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    // 初始状态同步
    s_ui->is_connected = app_usb_serial_is_connected();
    if(s_ui->is_connected) {
        lv_label_set_text(s_ui->lbl_status, "已连接");
        lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0x000000), 0);
    }
}

// 20ms周期刷新任务
void Update_Serial_Unit(void)
{
    if (!s_ui) return;
    
    Update_Keyboard();

    // 1. 监测硬件插拔状态
    bool curr_conn = app_usb_serial_is_connected();
    if (curr_conn != s_ui->is_connected) 
    {
        s_ui->is_connected = curr_conn;
        if (!curr_conn) 
        {
            if (s_ui->is_opened) {
                app_usb_serial_close();
                s_ui->is_opened = false;
                lv_label_set_text(s_ui->lbl_btn_open, "打开");
            }
            lv_label_set_text(s_ui->lbl_status, "未连接");
            lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0xA00000), 0);
        } 
        else 
        {
            lv_label_set_text(s_ui->lbl_status, "已连接");
            lv_obj_set_style_text_color(s_ui->lbl_status, lv_color_hex(0x000000), 0);
        }
    }

    // 2. 数据读取更新
    if (s_ui->is_opened) 
    {
        int len = app_usb_serial_recv(s_ui->rx_buf, RX_BUF_SIZE - 1);
        if (len > 0) 
        {
            s_ui->rx_buf[len] = '\0'; 
            
            const char * current_text = lv_textarea_get_text(s_ui->ta_rx);
            if (current_text && strlen(current_text) > MAX_TA_LEN) 
            {
                lv_textarea_set_text(s_ui->ta_rx, "");
                lv_textarea_add_text(s_ui->ta_rx, "[缓冲区已清空]\n");
            }

            lv_textarea_add_text(s_ui->ta_rx, (const char *)s_ui->rx_buf);
        }
    }
}

void Remove_Serial_Unit(void)
{
    Remove_Keyboard();

    if (s_ui) 
    {
        if (s_ui->is_opened) {
            app_usb_serial_close();
        }
        if (s_ui->root_scr) {
            lv_obj_del(s_ui->root_scr);
        }
        if (s_ui->rx_buf) {
            free_bsc(s_ui->rx_buf);
        }
        free_bsc(s_ui);
        
        s_ui = NULL;
    }
}
