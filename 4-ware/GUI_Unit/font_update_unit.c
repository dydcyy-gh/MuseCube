#include "font_update_unit.h"
#include "lvgl.h"
#include "variables.h"

static lv_obj_t *fu_cont = NULL;
static lv_obj_t *fu_label_title = NULL;
static lv_obj_t *fu_bar_progress = NULL;
static lv_obj_t *fu_label_pct = NULL;    
static lv_obj_t *fu_label_status = NULL;
static lv_obj_t *fu_label_file = NULL;
static lv_obj_t *fu_label_time = NULL;

static const char *font_names[] = {
    "UNIGBK.bin", "Font8.bin", "Font12.bin", "Font16.bin", "Font24.bin"
};

// 缓存状态，防止20ms频繁重绘卡顿
static uint8_t last_state = 0xFF;
static uint8_t last_progress = 0xFF;
static uint8_t last_file_idx = 0xFF;
static uint8_t last_error = 0xFF;

// 计时相关
static uint32_t start_time = 0;
static uint32_t last_elapsed_sec = 0xFFFFFFFF;

void Create_Font_Update_Unit(void)
{
    if (fu_cont != NULL) return;

    // 重置所有缓存和时间
    last_state = 0xFF;
    last_progress = 0xFF;
    last_file_idx = 0xFF;
    last_error = 0xFF;
    last_elapsed_sec = 0xFFFFFFFF;
    start_time = lv_tick_get(); // 记录界面创建时的系统滴答(毫秒)

    // 1. 主容器 (保留卡片风格)
    fu_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(fu_cont, 240, 240);
    lv_obj_center(fu_cont);
    lv_obj_set_style_radius(fu_cont, 15, 0); 
    lv_obj_set_style_bg_color(fu_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(fu_cont, 0, 0);
    lv_obj_set_style_shadow_width(fu_cont, 20, 0);
    lv_obj_set_style_shadow_color(fu_cont, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_shadow_ofs_y(fu_cont, 5, 0);
    lv_obj_clear_flag(fu_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 标题 (24号字)
    fu_label_title = lv_label_create(fu_cont);
    lv_obj_set_style_text_font(fu_label_title, &lv_font_24, 0);
    lv_label_set_text(fu_label_title, "SYSTEM UPDATE"); 
    lv_obj_set_style_text_color(fu_label_title, lv_color_hex(0x222222), 0);
    lv_obj_align(fu_label_title, LV_ALIGN_TOP_MID, 0, 20);

    // 3. 进度条 (直角、取消动画、自定义颜色)
    fu_bar_progress = lv_bar_create(fu_cont);
    lv_obj_set_size(fu_bar_progress, 190, 14); 
    lv_obj_align(fu_bar_progress, LV_ALIGN_CENTER, 0, 0);
    
    // 取消圆角，变成直角硬核风格
    lv_obj_set_style_radius(fu_bar_progress, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(fu_bar_progress, 0, LV_PART_INDICATOR);
    
    // 自定义颜色：暗灰底色 + 翠绿高亮 (不使用默认蓝)
    lv_obj_set_style_bg_color(fu_bar_progress, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(fu_bar_progress, lv_color_hex(0x00C853), LV_PART_INDICATOR);
    
    // 取消动画时间设置
    lv_obj_set_style_anim_time(fu_bar_progress, 0, 0); 
    lv_bar_set_range(fu_bar_progress, 0, 100);
    lv_bar_set_value(fu_bar_progress, 0, LV_ANIM_OFF);

    // 4. 百分比标签 (放在进度条右上方)
    fu_label_pct = lv_label_create(fu_cont);
    lv_obj_set_style_text_font(fu_label_pct, &lv_font_12, 0); 
    lv_obj_set_style_text_color(fu_label_pct, lv_color_hex(0x00C853), 0); 
    lv_label_set_text(fu_label_pct, "0%");
    lv_obj_align_to(fu_label_pct, fu_bar_progress, LV_ALIGN_OUT_TOP_RIGHT, 0, -5);

    // 5. 主状态标签 (16号字，放在进度条左上方)
    fu_label_status = lv_label_create(fu_cont);
    lv_obj_set_style_text_font(fu_label_status, &lv_font_16, 0);
    lv_label_set_text(fu_label_status, "Preparing...");
    lv_obj_set_style_text_color(fu_label_status, lv_color_hex(0x555555), 0);
    lv_obj_align_to(fu_label_status, fu_bar_progress, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

    // 6. 文件详情标签 (12号字，放在进度条左下方)
    fu_label_file = lv_label_create(fu_cont);
    lv_obj_set_style_text_font(fu_label_file, &lv_font_12, 0);
    lv_label_set_text(fu_label_file, "Wait...");
    lv_obj_set_style_text_color(fu_label_file, lv_color_hex(0x888888), 0);
    lv_obj_align_to(fu_label_file, fu_bar_progress, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // 7. 时间标签 (12号字，放在进度条右下方)
    fu_label_time = lv_label_create(fu_cont);
    lv_obj_set_style_text_font(fu_label_time, &lv_font_12, 0);
    lv_label_set_text(fu_label_time, "Time: 0s");
    lv_obj_set_style_text_color(fu_label_time, lv_color_hex(0x888888), 0);
    lv_obj_align_to(fu_label_time, fu_bar_progress, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);
}

void Update_Font_Update_Unit(void)
{
    if (fu_cont == NULL) return;

    uint8_t state = g_font_update_state;
    uint8_t progress = g_font_update_progress;
    uint8_t file_idx = g_font_update_file_index;
    uint8_t error = g_font_update_error;

    // 计算已流逝的秒数 (只有非完成/非错误状态才继续计时)
    uint32_t elapsed_sec = last_elapsed_sec; 
    if (state != 3 && state != 0xFF) {
        elapsed_sec = (lv_tick_get() - start_time) / 1000;
    }

    // === 性能拦截：没有任何改变，直接返回 ===
    if (state == last_state && progress == last_progress && 
        file_idx == last_file_idx && error == last_error && 
        elapsed_sec == last_elapsed_sec) {
        return; 
    }

    // --- 1. 更新进度条和百分比 (强制 LV_ANIM_OFF 避免高负载卡死) ---
    if (progress != last_progress) {
        lv_bar_set_value(fu_bar_progress, progress, LV_ANIM_OFF); 
        lv_label_set_text_fmt(fu_label_pct, "%d%%", progress);
        last_progress = progress;
    }

    // --- 2. 更新时间 ---
    if (elapsed_sec != last_elapsed_sec) {
        lv_label_set_text_fmt(fu_label_time, "Time: %lds", elapsed_sec);
        last_elapsed_sec = elapsed_sec;
    }

    // --- 3. 更新状态和文件文本 ---
    if (state != last_state || file_idx != last_file_idx || error != last_error) {
        switch (state) {
            case 0:
                lv_label_set_text(fu_label_status, "Preparing...");
                lv_label_set_text(fu_label_file, "Init flash");
                break;
            case 1:
                lv_label_set_text(fu_label_status, "Erasing Flash");
                lv_label_set_text(fu_label_file, "Do not power off");
                break;
            case 2:
                lv_label_set_text_fmt(fu_label_status, "Writing (%d/5)", file_idx + 1);
                if (file_idx < 5) {
                    lv_label_set_text_fmt(fu_label_file, "File: %s", font_names[file_idx]);
                }
                break;
            case 3:
                lv_obj_set_style_text_color(fu_label_status, lv_color_hex(0x00C853), 0); // 成功绿
                lv_label_set_text(fu_label_status, "Update Complete!");
                lv_label_set_text(fu_label_file, "Done.");
                lv_label_set_text(fu_label_pct, "100%");
                lv_bar_set_value(fu_bar_progress, 100, LV_ANIM_OFF); // 同样禁用动画
                break;
            case 0xFF:
                lv_obj_set_style_text_color(fu_label_status, lv_color_hex(0xFF3B30), 0); // 错误红
                lv_label_set_text(fu_label_status, "Update Failed!");
                lv_label_set_text_fmt(fu_label_file, "Error Code: %d", error);
                // 出错时进度条颜色变红警告
                lv_obj_set_style_bg_color(fu_bar_progress, lv_color_hex(0xFF3B30), LV_PART_INDICATOR);
                break;
            default:
                break;
        }
        
        last_state = state;
        last_file_idx = file_idx;
        last_error = error;
    }
}

void Remove_Font_Update_Unit(void)
{
    if (fu_cont != NULL) {
        lv_obj_del(fu_cont);
        fu_cont = NULL;
        fu_label_title = NULL;
        fu_bar_progress = NULL;
        fu_label_pct = NULL;
        fu_label_status = NULL;
        fu_label_file = NULL;
        fu_label_time = NULL;
    }
}
