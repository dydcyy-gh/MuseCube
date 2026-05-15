#include "text_unit.h"
#include "lv_port_disp.h"
#include "variables.h"
#include "ff.h"
#include "program.h"
#include "page_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "variables.h"
#include "defines.h"
#include "malloc.h"

extern const lv_img_dsc_t canvas_before;
extern const lv_img_dsc_t canvas_big;
extern const lv_img_dsc_t canvas_exit;
extern const lv_img_dsc_t canvas_load;
extern const lv_img_dsc_t canvas_next;
extern const lv_img_dsc_t canvas_pencil;
extern const lv_img_dsc_t canvas_rubber;
extern const lv_img_dsc_t canvas_save;
extern const lv_img_dsc_t canvas_small;

static uint16_t pen_h = 0;   // 0-359
static uint8_t  pen_s = 100; // 0-100
static uint8_t  pen_v = 0;   // 0-100 (默认0即黑色)

static lv_obj_t * canvas_cont = NULL;
static lv_obj_t * pixel_canvas = NULL;
static lv_color_t * canvas_buf = NULL;

static lv_obj_t * float_panel = NULL;

static lv_obj_t * minimap = NULL;
static lv_obj_t * minimap_box = NULL;

#define CANVAS_WIDTH  40
#define CANVAS_HEIGHT 40
#define MAX_ZOOM      12
#define MIN_ZOOM      1

static int current_zoom = 6;
static uint8_t current_tool = 0; // 0: 画笔, 1: 橡皮擦(白)
static lv_color_t pen_color;     // 画笔当前颜色

static bool ignore_draw = false; // 用于阻止关闭浮动栏时的穿透绘图

static uint8_t float_page = 0;   // 浮动栏页码，0: 工具页, 1: 颜色选择页

// 手动记录画布的位置坐标
static int canvas_pos_x = 0;
static int canvas_pos_y = 0;

// --- 撤销/重做 历史记录结构 ---
#define MAX_HISTORY_STEPS 10
static lv_color_t * history_buf[MAX_HISTORY_STEPS];
static int history_idx = -1;
static int history_max = -1;

// 前置声明
static void build_float_panel_content(lv_obj_t * panel);

// --- 更新略缩图与黑框显示状态 ---
static void update_minimap(void)
{
    if(!minimap || !minimap_box) return;

    int w = CANVAS_WIDTH * current_zoom;
    int h = CANVAS_HEIGHT * current_zoom;

    bool show = (w > 240 || h > 240);
    if (!show) {
        lv_obj_add_flag(minimap, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(minimap_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(minimap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(minimap_box, LV_OBJ_FLAG_HIDDEN);

    // 计算缩略图上的视野框大小
    int box_w = (240 * CANVAS_WIDTH) / w; 
    int box_h = (240 * CANVAS_HEIGHT) / h;
    if(box_w > CANVAS_WIDTH) box_w = CANVAS_WIDTH;
    if(box_h > CANVAS_HEIGHT) box_h = CANVAS_HEIGHT;

    lv_obj_set_size(minimap_box, box_w, box_h);

    // 手动计算框的位置：画布向左/上偏移（负值），对应的略缩图框向右/下移动
    int map_x = 10;
    int map_y = 10;
    
    if (w > 240) {
        map_x = 10 + ((-canvas_pos_x) * CANVAS_WIDTH) / w;
    }
    if (h > 240) {
        map_y = 10 + ((-canvas_pos_y) * CANVAS_HEIGHT) / h;
    }
    
    lv_obj_set_pos(minimap_box, map_x, map_y);
}

// 缩放刷新应用
static void apply_zoom(void)
{
    lv_img_set_zoom(pixel_canvas, 256 * current_zoom);
    
    int w = CANVAS_WIDTH * current_zoom;
    int h = CANVAS_HEIGHT * current_zoom;

    // 大小判定：如果小于等于屏幕，交给系统原生居中（完美消除1pix缝隙）
    if (w <= 240 && h <= 240) {
        canvas_pos_x = (240 - w) / 2;
        canvas_pos_y = (240 - h) / 2;
        lv_obj_align(pixel_canvas, LV_ALIGN_CENTER, 0, 0);
    } else {
        // 只有当画布大于屏幕时，才使用绝对坐标，并进行边缘约束
        lv_obj_align(pixel_canvas, LV_ALIGN_TOP_LEFT, 0, 0); // 必须先重置对齐基准
        
        if (w <= 240) {
            canvas_pos_x = (240 - w) / 2;
        } else {
            if (canvas_pos_x > 0) canvas_pos_x = 0;
            if (canvas_pos_x < 240 - w) canvas_pos_x = 240 - w;
        }

        if (h <= 240) {
            canvas_pos_y = (240 - h) / 2;
        } else {
            if (canvas_pos_y > 0) canvas_pos_y = 0;
            if (canvas_pos_y < 240 - h) canvas_pos_y = 240 - h;
        }
        
        lv_obj_set_pos(pixel_canvas, canvas_pos_x, canvas_pos_y);
    }

    update_minimap();
}

// --- 保存画布相关的函数 ---
static void save_history_step(void)
{
    if (history_idx < history_max) {
        for (int i = history_idx + 1; i <= history_max; i++) {
            if (history_buf[i]) {
                free_bsc(history_buf[i]);
                history_buf[i] = NULL;
            }
        }
    }

    history_idx++;
    if (history_idx >= MAX_HISTORY_STEPS) {
        if (history_buf[0]) free_bsc(history_buf[0]);
        for (int i = 0; i < MAX_HISTORY_STEPS - 1; i++) {
            history_buf[i] = history_buf[i + 1];
        }
        history_idx = MAX_HISTORY_STEPS - 1;
    }
    history_max = history_idx;

    uint32_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT);
    history_buf[history_idx] = (lv_color_t *)malloc_bsc(buf_size);
    if (history_buf[history_idx] != NULL) {
        memcpy(history_buf[history_idx], canvas_buf, buf_size);
    }
}

static void undo_step(void)
{
    if (history_idx > 0) {
        history_idx--;
        uint32_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT);
        memcpy(canvas_buf, history_buf[history_idx], buf_size);
        lv_obj_invalidate(pixel_canvas);
    }
}

static void redo_step(void)
{
    if (history_idx < history_max) {
        history_idx++;
        uint32_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT);
        memcpy(canvas_buf, history_buf[history_idx], buf_size);
        lv_obj_invalidate(pixel_canvas);
    }
}

static void clear_history(void)
{
    for (int i = 0; i < MAX_HISTORY_STEPS; i++) {
        if (history_buf[i]) {
            free_bsc(history_buf[i]);
            history_buf[i] = NULL;
        }
    }
    history_idx = -1;
    history_max = -1;
}

// BMP文件头结构体配置(1字节对齐)
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMP_FILE_HEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMP_INFO_HEADER;
#pragma pack(pop)

static void Save_Canvas_To_BMP(const char * path)
{
    if (canvas_buf == NULL) return;

    FIL file;
    UINT bw;
    
    if (f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return;

    BMP_FILE_HEADER bfh = {0};
    BMP_INFO_HEADER bih = {0};

    int row_size = ((CANVAS_WIDTH * 3) + 3) & ~3;
    int image_size = row_size * CANVAS_HEIGHT;

    bfh.bfType = 0x4D42;
    bfh.bfSize = sizeof(BMP_FILE_HEADER) + sizeof(BMP_INFO_HEADER) + image_size;
    bfh.bfOffBits = sizeof(BMP_FILE_HEADER) + sizeof(BMP_INFO_HEADER);

    bih.biSize = sizeof(BMP_INFO_HEADER);
    bih.biWidth = CANVAS_WIDTH;
    bih.biHeight = -CANVAS_HEIGHT;
    bih.biPlanes = 1;
    bih.biBitCount = 24;

    f_write(&file, &bfh, sizeof(bfh), &bw);
    f_write(&file, &bih, sizeof(bih), &bw);

    uint8_t * row_buf = (uint8_t *)malloc_bsc(row_size);
    if (row_buf == NULL) {
        f_close(&file);
        return;
    }

    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        memset(row_buf, 0, row_size);
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            lv_color_t color = canvas_buf[y * CANVAS_WIDTH + x];
            row_buf[x * 3 + 0] = (color.ch.blue << 3)  | (color.ch.blue >> 2);
            row_buf[x * 3 + 1] = (color.ch.green << 2) | (color.ch.green >> 4);
            row_buf[x * 3 + 2] = (color.ch.red << 3)   | (color.ch.red >> 2);
        }
        f_write(&file, row_buf, row_size, &bw);
    }

    free_bsc(row_buf);
    f_close(&file);
}

// 对应 Case 8：加载BMP到内存
static void Load_Canvas_From_BMP(const char * path)
{
    if (canvas_buf == NULL) return;

    FIL file;
    UINT br;
    
    if (f_open(&file, path, FA_READ) != FR_OK) return;

    BMP_FILE_HEADER bfh;
    BMP_INFO_HEADER bih;

    f_read(&file, &bfh, sizeof(bfh), &br);
    f_read(&file, &bih, sizeof(bih), &br);

    // 校验文件合法性 (必须等于画布尺寸且为24位色)
    if (bih.biWidth != CANVAS_WIDTH || abs(bih.biHeight) != CANVAS_HEIGHT || bih.biBitCount != 24) {
        f_close(&file);
        return;
    }

    f_lseek(&file, bfh.bfOffBits);

    int row_size = ((CANVAS_WIDTH * 3) + 3) & ~3;
    uint8_t * row_buf = (uint8_t *)malloc_bsc(row_size);
    if (row_buf == NULL) {
        f_close(&file);
        return;
    }

    bool top_down = (bih.biHeight < 0);

    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        f_read(&file, row_buf, row_size, &br);
        // 如果文件标明是正高度(从下到上)，则转换y轴；如果是负高度(从上到下)，直接使用。
        int dest_y = top_down ? y : (CANVAS_HEIGHT - 1 - y);
        
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            uint8_t b = row_buf[x * 3 + 0];
            uint8_t g = row_buf[x * 3 + 1];
            uint8_t r = row_buf[x * 3 + 2];
            
            lv_color_t color;
            // RGB888 -> RGB565
            color.ch.blue = b >> 3;
            color.ch.green = g >> 2;
            color.ch.red = r >> 3;
            
            canvas_buf[dest_y * CANVAS_WIDTH + x] = color;
        }
    }

    free_bsc(row_buf);
    f_close(&file);

    lv_obj_invalidate(pixel_canvas); // 标记需要重新绘制
    update_minimap();                // 刷新略缩图
    save_history_step();             // 压入历史记录
}

// --- 色环取色回调 ---
static void colorwheel_cb(lv_event_t * e)
{
    lv_obj_t * cw = lv_event_get_target(e);
    
    // 只从色环获取色��� (Hue)
    pen_h = lv_colorwheel_get_hsv(cw).h;
    
    // 结合当前的 S 和 V 计算真实颜色
    pen_color = lv_color_hsv_to_rgb(pen_h, pen_s, pen_v);
    current_tool = 0; 

    // 更新页面切换按钮（即按键4）的背景色
    lv_obj_t * panel = lv_obj_get_parent(cw);
    lv_obj_t * btn4 = lv_obj_get_child(panel, 1); 
    if (btn4) {
        lv_obj_set_style_bg_color(btn4, pen_color, 0);
    }
}

// --- 浮动工具栏按钮回调 ---
static void float_btn_event_cb(lv_event_t * e)
{
    uint32_t btn_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    
    switch(btn_id) {
        case 0: undo_step(); break; // 撤回
        case 1: Page_Back(); break; // 退出
        case 2: redo_step(); break; // 取消撤回
        case 3: if(current_zoom > MIN_ZOOM) {current_zoom--;apply_zoom();} break;// 缩小
        case 4: 
            float_page = !float_page; // 翻页
            if(float_panel) build_float_panel_content(float_panel);
            break;
        case 5: if(current_zoom < MAX_ZOOM) {current_zoom++;apply_zoom();} break;// 放大
        case 6: Save_Canvas_To_BMP("0:/PICTURE/test.bmp"); break;// 保存SD
        
        case 7: // 画笔/橡皮切换
        {
            current_tool = !current_tool; 
            // 立即获取触发事件的按钮，并更新其子对象（图标）的图片源
            lv_obj_t * btn = lv_event_get_target(e);
            lv_obj_t * img = lv_obj_get_child(btn, 0); // 图标是我们为该按钮创建的第一个(唯一)子对象
            if (img != NULL) {
                lv_img_set_src(img, current_tool == 0 ? &canvas_pencil : &canvas_rubber);
            }
            break;
        }
        
        case 8: Load_Canvas_From_BMP("0:/PICTURE/test.bmp"); break;// 加载SD
    }
}

// --- HSV 调整按钮回调 ---
static void hsv_adjust_event_cb(lv_event_t * e)
{
    uint32_t action = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    int step = 10; // S和V 每次增减 10%
    int h_step = 5; // H 每次增减 5度
    
    switch (action) {
        case 0: // S-
            if (pen_s >= step) pen_s -= step; else pen_s = 0;
            break;
        case 1: // S+
            if (pen_s <= 100 - step) pen_s += step; else pen_s = 100;
            break;
        case 2: // V-
            if (pen_v >= step) pen_v -= step; else pen_v = 0;
            break;
        case 3: // V+
            if (pen_v <= 100 - step) pen_v += step; else pen_v = 100;
            break;
        case 4: // H-
            if (pen_h >= h_step) pen_h -= h_step; else pen_h = 360 - h_step + pen_h;
            break;
        case 5: // H+
            pen_h = (pen_h + h_step) % 360;
            break;
    }
    
    // 重新计算真实的画笔颜色
    pen_color = lv_color_hsv_to_rgb(pen_h, pen_s, pen_v);
    current_tool = 0;

    // 刷新中间的中心按钮颜色
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * panel = lv_obj_get_parent(btn);
    lv_obj_t * btn4 = lv_obj_get_child(panel, 1); 
    if (btn4) {
        lv_obj_set_style_bg_color(btn4, pen_color, 0);
    }
}

// 辅助创建微调按钮的函数
static void create_adjust_btn(lv_obj_t * parent, int x, int y, const char * text, uint32_t action)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 22, 22);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 11, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x888888), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_transform_zoom(label, 150, 0); // 缩小字体防止越界
    
    lv_obj_add_event_cb(btn, hsv_adjust_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)action);
}

// --- 动态构建浮动栏内容 ---
static void build_float_panel_content(lv_obj_t * panel)
{
    // 清除面板所有子对象
    lv_obj_clean(panel);

    if (float_page == 0) {
        // 定义第一页各个按钮的图标指针映射
        const lv_img_dsc_t * icons[9] = {
            &canvas_before,     // 0: 撤回
            &canvas_exit,       // 1: 退出
            &canvas_next,       // 2: 取消撤回
            &canvas_small,      // 3: 缩小
            NULL,               // 4: 翻页/当前颜色提示 (不用图标)
            &canvas_big,        // 5: 放大
            &canvas_save,       // 6: 保存
            current_tool == 0 ? &canvas_pencil : &canvas_rubber, // 7: 切换画笔/橡皮
            &canvas_load        // 8: 加载
        };

        // 第一页：九宫格工具
        for (int i = 0; i < 9; i++) {
            lv_obj_t * btn = lv_obj_create(panel);
            
            lv_obj_set_size(btn, 30, 30);
            
            int row = i / 3;
            int col = i % 3;
            
            // 边距10，按钮30，间距10。步进即为40
            lv_obj_set_pos(btn, 10 + col * 40, 10 + row * 40);
            
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xCCCCCC), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_50, 0); 
            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            
            // 额外给中间的按键4上个色提示当前画笔颜色
            if (i == 4) {
                lv_obj_set_style_bg_color(btn, pen_color, 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(btn, 1, 0);
                lv_obj_set_style_border_color(btn, lv_color_hex(0x000000), 0);
            }

            // 如果该位置有图标映射，则创建图片并居中
            if (icons[i] != NULL) {
                lv_obj_t * img = lv_img_create(btn);
                lv_img_set_src(img, icons[i]);
                lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
                
                // alpha_1bit 图片通常需要重新着色来确保显示（这里设为深灰色/黑色）
                lv_obj_set_style_img_recolor(img, lv_color_hex(0x333333), 0);
                lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
            }

            lv_obj_add_event_cb(btn, float_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        }
    } else {
        // 第二页：画笔颜色选择
        lv_obj_t * cw = lv_colorwheel_create(panel, true);
        lv_obj_set_size(cw, 120, 120); // 扩大到120
        lv_obj_align(cw, LV_ALIGN_CENTER, 0, 0);

        // 彻底隐藏滚轮（设置透明度为0）
        lv_obj_set_style_opa(cw, LV_OPA_TRANSP, LV_PART_KNOB); 
        lv_obj_set_style_arc_width(cw, 12, LV_PART_MAIN);

        // 永远将色环的 S 和 V 设为 100，保证色环始终是彩虹色！
        lv_color_hsv_t display_hsv = {pen_h, 100, 100};
        lv_colorwheel_set_hsv(cw, display_hsv); 
        
        lv_obj_add_event_cb(cw, colorwheel_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // 保留原按钮4，用于切换回第一页。放在中心(130/2=65, 65-15=50)
        lv_obj_t * btn = lv_obj_create(panel);
        lv_obj_set_size(btn, 30, 30);
        lv_obj_set_pos(btn, 50, 50); // 适配130x130的绝对中心位置
        lv_obj_set_style_radius(btn, 15, 0);
        lv_obj_set_style_bg_color(btn, pen_color, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0); 
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, float_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)4);

        // 添加六个环绕分布的调节按钮
        const char* texts[6] = {"-S", "+S", "-V", "+V", "-H", "+H"};
        uint32_t actions[6] = {0, 1, 2, 3, 4, 5};
        
        int center_x = 65;
        int center_y = 65;
        int radius = 33; // 放在中心按钮外侧，色环内侧 (15 < 33 < 48)

        for(int i = 0; i < 6; i++) {
            // 每隔60度放一个按钮 (i * PI / 3)
            double angle = i * 60.0 * 3.14159265 / 180.0;
            // 坐标减去 11 (22/2) 使得按钮中心与计算坐标对齐
            int bx = center_x + (int)(radius * cos(angle)) - 11;
            int by = center_y + (int)(radius * sin(angle)) - 11;
            
            create_adjust_btn(panel, bx, by, texts[i], actions[i]);
        }
    }
}

// --- 主画布事件处理 ---
static void canvas_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // 果手指松开，重置屏蔽绘图标志位
    if (code == LV_EVENT_RELEASED) {
        ignore_draw = false;
    }

    // 检查是否点在浮动栏外并需要关闭浮动栏
    if (code == LV_EVENT_PRESSED && float_panel != NULL) {
        lv_indev_t * indev = lv_indev_get_act();
        lv_point_t point;
        lv_indev_get_point(indev, &point); 
        
        if(point.x < float_panel->coords.x1 || point.x > float_panel->coords.x2 ||
           point.y < float_panel->coords.y1 || point.y > float_panel->coords.y2) {
            lv_obj_del(float_panel);
            float_panel = NULL;
            float_page = 0; // 关闭时重置为第一页
            // 屏蔽本次按压期间的所有绘图触发
            ignore_draw = true; 
            return; 
        }
    }

    if (ignore_draw) return;

    if (code == LV_EVENT_PRESSED) {
        save_history_step();
    }

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        lv_indev_t * indev = lv_indev_get_act();
        if(indev == NULL) return;

        lv_point_t point;
        lv_indev_get_point(indev, &point); 

        // 使用画布实际在屏幕的物理位置进行换算
        lv_coord_t px_x = pixel_canvas->coords.x1;
        lv_coord_t px_y = pixel_canvas->coords.y1;

        int16_t logical_x = (int16_t)floor((point.x - px_x) / current_zoom);
        int16_t logical_y = (int16_t)floor((point.y - px_y) / current_zoom);

        if (logical_x >= 0 && logical_x < CANVAS_WIDTH && logical_y >= 0 && logical_y < CANVAS_HEIGHT) {
            // 【修改】使用笔的颜色作为绘制颜色
            lv_color_t color = (current_tool == 0) ? pen_color : lv_color_white();
            lv_canvas_set_px_color(pixel_canvas, logical_x, logical_y, color);
            lv_obj_invalidate(pixel_canvas);
            lv_obj_invalidate(minimap); // 刷新略缩图
        }
    }
}

void Create_Canvas_Unit(void)
{
	pen_h = 0;
    pen_s = 100;
    pen_v = 0;
    pen_color = lv_color_black();
	
    clear_history();
    current_zoom = 6;
    ignore_draw = false;
    float_page = 0;
    pen_color = lv_color_black(); // 默认画笔颜色为黑

    // 清掉当前屏幕的滚动属性，防止底层输入设备左摇杆误触发拖拽画线
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    canvas_buf = (lv_color_t *)malloc_bsc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT));
    if(canvas_buf == NULL) return;

    canvas_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(canvas_cont, 240, 240);
    lv_obj_center(canvas_cont);
    lv_obj_set_style_bg_opa(canvas_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(canvas_cont, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(canvas_cont, 0, 0);
    lv_obj_set_style_radius(canvas_cont, 0, 0);
    lv_obj_set_style_pad_all(canvas_cont, 0, 0);
    
    // 确保画布容器没有滚动属性
    lv_obj_clear_flag(canvas_cont, LV_OBJ_FLAG_SCROLLABLE);

    pixel_canvas = lv_canvas_create(canvas_cont);
    lv_canvas_set_buffer(pixel_canvas, canvas_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(pixel_canvas, lv_color_white(), LV_OPA_COVER);
    lv_img_set_antialias(pixel_canvas, false);
    
    lv_img_set_size_mode(pixel_canvas, LV_IMG_SIZE_MODE_REAL);
    
    lv_obj_clear_flag(pixel_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pixel_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pixel_canvas, canvas_event_cb, LV_EVENT_ALL, NULL);

    // 创建略缩图
    minimap = lv_img_create(lv_scr_act());
    lv_img_set_src(minimap, lv_canvas_get_img(pixel_canvas));
    lv_obj_set_pos(minimap, 10, 10);
    lv_obj_set_size(minimap, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_set_style_outline_width(minimap, 2, 0);
    lv_obj_set_style_outline_color(minimap, lv_color_hex(0x555555), 0);
    lv_obj_set_style_outline_pad(minimap, 0, 0);
    lv_obj_set_style_shadow_width(minimap, 10, 0);
    lv_obj_set_style_shadow_color(minimap, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(minimap, LV_OPA_50, 0);
    lv_obj_set_style_shadow_ofs_x(minimap, 2, 0);
    lv_obj_set_style_shadow_ofs_y(minimap, 2, 0);
    lv_obj_move_foreground(minimap);

    // 视野黑框
    minimap_box = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_opa(minimap_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(minimap_box, lv_color_black(), 0);
    lv_obj_set_style_border_width(minimap_box, 1, 0);
    lv_obj_set_style_radius(minimap_box, 0, 0);
    lv_obj_clear_flag(minimap_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(minimap_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(minimap_box);
    
    // 初始化画布位置
    apply_zoom();

    save_history_step();
}

void Update_Canvas_Unit(void)
{
    // ============================================
    // 【新增】画布边缘平移判定逻辑
    // ============================================
    lv_point_t point = {120, 120}; 
    lv_indev_t * indev = lv_indev_get_next(NULL);
    while(indev) {
        if(lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_get_point(indev, &point);
            break;
        }
        indev = lv_indev_get_next(indev);
    }

    if (pixel_canvas != NULL && float_panel == NULL) {
        int w = CANVAS_WIDTH * current_zoom;
        int h = CANVAS_HEIGHT * current_zoom;
        int pan_speed = 6; // 平移速度，可以根据需要调整
        bool need_update = false;

        if (w > 240) {
            if (point.x < 10) { canvas_pos_x += pan_speed; need_update = true; }
            if (point.x > 230) { canvas_pos_x -= pan_speed; need_update = true; }
            // 约束边界
            if (canvas_pos_x > 0) canvas_pos_x = 0;
            if (canvas_pos_x < 240 - w) canvas_pos_x = 240 - w;
        }

        if (h > 240) {
            if (point.y < 10) { canvas_pos_y += pan_speed; need_update = true; }
            if (point.y > 230) { canvas_pos_y -= pan_speed; need_update = true; }
            // 约束边界
            if (canvas_pos_y > 0) canvas_pos_y = 0;
            if (canvas_pos_y < 240 - h) canvas_pos_y = 240 - h;
        }

        if (need_update) {
            lv_obj_set_pos(pixel_canvas, canvas_pos_x, canvas_pos_y);
            update_minimap(); // 更新略缩图上的小黑框
        }
    }
    // ============================================

    static uint8_t last_R = 0;
    
    // R键松开检测 (呼出/隐藏浮动栏)
    if(!g_key_R_M_RT && last_R)
    {
        if (float_panel == NULL) {
            float_page = 0; // 呼出时保证默认在第一页
            // 创建 130x130 浮动栏
            float_panel = lv_obj_create(lv_scr_act());
            lv_obj_set_size(float_panel, 130, 130);
            
            // 计算边界限制 (130/2 = 65)
            lv_coord_t px = point.x - 65;
            lv_coord_t py = point.y - 65;
            if (px < 0) px = 0;
            if (py < 0) py = 0;
            if (px > 240 - 130) px = 240 - 130;
            if (py > 240 - 130) py = 240 - 130;
            
            lv_obj_set_pos(float_panel, px, py);
            
            lv_obj_set_style_radius(float_panel, 11, 0);
            lv_obj_set_style_bg_color(float_panel, lv_color_white(), 0);
            // 【修改】改为纯白不透明
            lv_obj_set_style_bg_opa(float_panel, LV_OPA_COVER, 0); 
            lv_obj_set_style_border_width(float_panel, 1, 0);
            lv_obj_set_style_border_color(float_panel, lv_color_hex(0x808080), 0);
            lv_obj_set_style_shadow_width(float_panel, 0, 0);
            lv_obj_set_style_pad_all(float_panel, 0, 0);
            lv_obj_clear_flag(float_panel, LV_OBJ_FLAG_SCROLLABLE);

            // 调用子函数构建浮动面板内容
            build_float_panel_content(float_panel);
        } else {
            lv_obj_del(float_panel);
            float_panel = NULL;
            float_page = 0;
        }
    }
    
    last_R = g_key_R_M_RT;
}

void Remove_Canvas_Unit(void)
{
    if (float_panel) {
        lv_obj_del(float_panel);
        float_panel = NULL;
    }

    if (minimap_box) {
        lv_obj_del(minimap_box);
        minimap_box = NULL;
    }

    if (minimap) {
        lv_obj_del(minimap);
        minimap = NULL;
    }

    if (canvas_cont) {
        lv_obj_del(canvas_cont);
        canvas_cont = NULL;
        pixel_canvas = NULL; 
    }

    if (canvas_buf) {
        free_bsc(canvas_buf);
        canvas_buf = NULL;
    }

    clear_history();
}
