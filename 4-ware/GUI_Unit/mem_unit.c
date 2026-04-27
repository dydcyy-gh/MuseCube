#include "stm32f4xx.h" 
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cpu_usage.h"

#define MAX_TASK_DISPLAY_COUNT 20  /* 支持最大读取的任务数 */

static lv_obj_t *mem_cont;
static lv_obj_t *title_row;
static lv_obj_t *mem_chart;
static lv_obj_t *task_table;
static lv_obj_t *mem_label_title;
static lv_obj_t *mem_label_info1;
static lv_obj_t *mem_label_info2;
static lv_chart_series_t *mem_ser;

/* 显示模式枚举 */
typedef enum {
    VIEW_MODE_RAM1 = 0,
    VIEW_MODE_RAM2,
    VIEW_MODE_CPU,
    VIEW_MODE_TASKS,
    VIEW_MODE_MAX
} view_mode_t;

static uint8_t current_view_mode = VIEW_MODE_RAM1; 

/* 标题文本点击事件回调 */
static void mem_label_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        /* 切换模式：0 -> 1 -> 2 -> 3 -> 0 */
        current_view_mode++;
        if (current_view_mode >= VIEW_MODE_MAX) {
            current_view_mode = VIEW_MODE_RAM1;
        }
        
        /* 切换时清空旧的历史数据，让曲线重新开始 */
        lv_chart_set_all_value(mem_chart, mem_ser, LV_CHART_POINT_NONE);

        /* 根据模式切换UI元素的隐藏与显示 */
        if (current_view_mode == VIEW_MODE_TASKS) {
            lv_label_set_text(mem_label_title, "Tasks");
            lv_label_set_text(mem_label_info2, "Powered By FreeRTOS");
            
            /* 隐藏图表和Info1，显示任务表格 */
            lv_obj_add_flag(mem_chart, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mem_label_info1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(task_table, LV_OBJ_FLAG_HIDDEN);
        } else {
            /* 更新回其他模式标题 */
            if (current_view_mode == VIEW_MODE_RAM1) lv_label_set_text(mem_label_title, "RAM1");
            else if (current_view_mode == VIEW_MODE_RAM2) lv_label_set_text(mem_label_title, "RAM2");
            else if (current_view_mode == VIEW_MODE_CPU) lv_label_set_text(mem_label_title, "CPU");
            
            /* 显示图表和Info1，隐藏任务表格 */
            lv_obj_clear_flag(mem_chart, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(mem_label_info1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(task_table, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Create_Mem_Chart(void)
{
    current_view_mode = VIEW_MODE_RAM1;

    /* 1. 创建大容器 */
    mem_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(mem_cont, 240, 180);
    lv_obj_center(mem_cont);
    lv_obj_set_style_radius(mem_cont, 0, 0);  
    lv_obj_set_style_border_width(mem_cont, 0, 0);
    lv_obj_set_flex_flow(mem_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mem_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(mem_cont, 10, 0);
    lv_obj_set_style_pad_row(mem_cont, 10, 0);
    lv_obj_clear_flag(mem_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* ================= 2. 创建上方文本部分 ================= */
    
    /* 2.1 创建标题容器 (水平布局) */
    title_row = lv_obj_create(mem_cont);
    lv_obj_set_size(title_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_pad_column(title_row, 10, 0); 
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    /* 2.2 创建标题标签 (包含点击事件) */
    mem_label_title = lv_label_create(title_row);
    lv_label_set_text(mem_label_title, "RAM1");
    lv_obj_set_style_text_font(mem_label_title, &lv_font_24, 0);
    
    /* 绑定点击回调到标题文本 */
    lv_obj_add_flag(mem_label_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mem_label_title, mem_label_event_cb, LV_EVENT_CLICKED, NULL);

    /* 2.3 创建第一行信息标签 (在图表/任务列表上方) */
    mem_label_info2 = lv_label_create(title_row);
    lv_obj_set_style_text_font(mem_label_info2, &lv_font_12, 0);
    lv_obj_set_style_translate_y(mem_label_info2, -4, 0);

    /* 2.4 创建第二行信息标签 (显示在图表下方，仅RAM/CPU模式可见) */
    mem_label_info1 = lv_label_create(mem_cont);
    lv_obj_set_style_text_font(mem_label_info1, &lv_font_12, 0);


    /* ================= 3. 创建 图表(RAM/CPU) 和 表格(Tasks) ================= */
    
    /* 3.1 图表创建 */
    mem_chart = lv_chart_create(mem_cont);
    lv_obj_set_size(mem_chart, 220, 100);
    lv_chart_set_type(mem_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(mem_chart, 30);       
    lv_chart_set_update_mode(mem_chart, LV_CHART_UPDATE_MODE_SHIFT); 
    lv_chart_set_range(mem_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100); 
    lv_chart_set_div_line_count(mem_chart, 6, 11);
    lv_obj_set_style_line_width(mem_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(mem_chart, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_chart_set_axis_tick(mem_chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
    lv_chart_set_axis_tick(mem_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 0, 0, false, 0);
    lv_obj_set_style_bg_opa(mem_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mem_chart, 2, 0);
    lv_obj_set_style_border_color(mem_chart, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(mem_chart, 0, 0); 
    lv_obj_set_style_pad_all(mem_chart, 0, 0); 
    lv_obj_set_style_line_width(mem_chart, 2, LV_PART_ITEMS); 
    lv_obj_set_style_line_color(mem_chart, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
    lv_obj_set_style_size(mem_chart, 0, LV_PART_INDICATOR);
    mem_ser = lv_chart_add_series(mem_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    /* 3.2 表格创建 */
    task_table = lv_table_create(mem_cont);
    /* 高度设为120 (刚好铺满剩余空间) */
    lv_obj_set_size(task_table, 220, 120); 
    lv_obj_add_flag(task_table, LV_OBJ_FLAG_HIDDEN);          
    
    /* 开启滚动并隐藏滚动条 */
    lv_obj_add_flag(task_table, LV_OBJ_FLAG_SCROLLABLE);      
    lv_obj_set_scroll_dir(task_table, LV_DIR_VER);            
    lv_obj_set_scrollbar_mode(task_table, LV_SCROLLBAR_MODE_OFF);

    /* ====== 外边框设置 ====== */
    lv_obj_set_style_bg_opa(task_table, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(task_table, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(task_table, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_radius(task_table, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(task_table, 0, LV_PART_MAIN);
    
    /* ====== 单元格设置 ====== */
    lv_obj_set_style_border_width(task_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(task_table, LV_OPA_TRANSP, LV_PART_ITEMS);
    
    /* 通过样式消除按下和聚焦时的默认变色背景，保持可滚动性 */
    lv_obj_set_style_bg_opa(task_table, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(task_table, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_FOCUSED);
    
    lv_obj_set_style_pad_top(task_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(task_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(task_table, 2, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(task_table, 2, LV_PART_ITEMS);
    lv_obj_set_style_text_font(task_table, &lv_font_12, LV_PART_ITEMS);
    
    /* 设置列宽 (总计 220) */
    lv_table_set_col_width(task_table, 0, 100);  // 任务名
    lv_table_set_col_width(task_table, 1, 40);  // 状态
    lv_table_set_col_width(task_table, 2, 40);  // 占比
    lv_table_set_col_width(task_table, 3, 40);  // 空间
}

void Update_Mem_Chart(void)
{
    if(!mem_cont) return;
    
    static TickType_t last_call_tick = 0;
    TickType_t current_tick = xTaskGetTickCount();
    if((current_tick - last_call_tick) < pdMS_TO_TICKS(1000)) return;
    last_call_tick = current_tick;

    if (current_view_mode == VIEW_MODE_TASKS) {
        /* ----- 进程列表 模式 ----- */
        TaskStatus_t task_status_array[MAX_TASK_DISPLAY_COUNT];
        uint32_t total_runtime;
        
        UBaseType_t task_count = uxTaskGetSystemState(task_status_array, MAX_TASK_DISPLAY_COUNT, &total_runtime);
        
        /* 表格行数为 任务数 + 1 (表头占用第 0 行) */
        lv_table_set_row_cnt(task_table, task_count + 1);
        
        /* 表头写入第 0 行 */
        lv_table_set_cell_value(task_table, 0, 0, "名称");
        lv_table_set_cell_value(task_table, 0, 1, "状态");
        lv_table_set_cell_value(task_table, 0, 2, "占比");
        lv_table_set_cell_value(task_table, 0, 3, "空间");
        
        uint32_t total_runtime_div = total_runtime / 100;
        if (total_runtime_div == 0) total_runtime_div = 1;
        
        for (int i = 0; i < task_count; i++) {
            char state_char;
            switch(task_status_array[i].eCurrentState) {
                case eRunning:   state_char = 'R'; break;
                case eReady:     state_char = 'r'; break;
                case eBlocked:   state_char = 'B'; break;
                case eSuspended: state_char = 'S'; break;
                case eDeleted:   state_char = 'D'; break;
                default:         state_char = '?'; break;
            }
            
            uint32_t runtime_pct = task_status_array[i].ulRunTimeCounter / total_runtime_div;
            if (runtime_pct > 100) runtime_pct = 100; 
            
            /* 实际任务数据写入 i + 1 行 */
            int row = i + 1;
            lv_table_set_cell_value(task_table, row, 0, task_status_array[i].pcTaskName);
            lv_table_set_cell_value_fmt(task_table, row, 1, "%c", state_char);
            lv_table_set_cell_value_fmt(task_table, row, 2, "%lu%%", runtime_pct);
            lv_table_set_cell_value_fmt(task_table, row, 3, "%u", task_status_array[i].usStackHighWaterMark);
        }
    }
    else if (current_view_mode == VIEW_MODE_CPU) {
        /* ----- CPU 模式 ----- */
        uint8_t cpu_use = Get_CPU_Usage();
        uint8_t task_num = uxTaskGetNumberOfTasks();
        
        uint32_t total_sec = current_tick / pdMS_TO_TICKS(1000);
        uint32_t sec = total_sec % 60;
        uint32_t min = (total_sec / 60) % 60;
        uint32_t hr  = (total_sec / 3600) % 24;
        uint32_t day = total_sec / 86400;

        lv_chart_set_next_value(mem_chart, mem_ser, (lv_coord_t)cpu_use);
        
        lv_label_set_text_fmt(mem_label_info2, "使用率:%d%% 速度:168MHz", cpu_use);
        lv_label_set_text_fmt(mem_label_info1, "进程数:%d 运行时间:%d:%02d:%02d:%02d", 
                              task_num, day, hr, min, sec);
    } 
    else {
        /* ----- RAM 模式 (BSC 或 CCM) ----- */
        mem_monitor_t mem_mon;
        
        if (current_view_mode == VIEW_MODE_RAM1) tlsf_monitor_bsc(&mem_mon);
        else tlsf_monitor_ccm(&mem_mon);

        lv_chart_set_next_value(mem_chart, mem_ser, mem_mon.used_pct);
        
        lv_label_set_text_fmt(mem_label_info1, "已使用/总内存: %.1fkB/%.1fkB", 
            mem_mon.used_size/1024.0f, mem_mon.total_size/1024.0f);

        lv_label_set_text_fmt(mem_label_info2, "使用率:%d%% 碎片率:%d%%", 
            mem_mon.used_pct, mem_mon.frag_pct);
    }
}

void Remove_Mem_Chart(void)
{
    if (mem_cont) 
    {
        lv_obj_del(mem_cont);
        
        mem_cont = NULL;
        title_row = NULL;
        mem_chart = NULL;
        task_table = NULL;
        mem_label_title = NULL;
        mem_label_info1 = NULL;
        mem_label_info2 = NULL;
        mem_ser = NULL;
    }
}
