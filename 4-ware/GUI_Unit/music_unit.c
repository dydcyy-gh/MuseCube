#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "page_manager.h"
#include <stdio.h>
#include <string.h>
#include "variables.h"
#include "defines.h"
#include "music.h"
#include "spectrum_dsp.h"
#include "ff.h"      // 用于 f_stat 检查文件是否存在
#include "lrc.h"     // 引入歌词解析器

// Extern image resources
extern const lv_img_dsc_t music_before;
extern const lv_img_dsc_t music_cycle;
extern const lv_img_dsc_t music_exit;
extern const lv_img_dsc_t music_like;
extern const lv_img_dsc_t music_menu;
extern const lv_img_dsc_t music_next;
extern const lv_img_dsc_t music_order;
extern const lv_img_dsc_t music_playing;
extern const lv_img_dsc_t music_random;
extern const lv_img_dsc_t music_suspend;

LV_IMG_DECLARE(pic_back);

static lv_obj_t *wallpaper_bg = NULL;
static lv_obj_t *music_unit = NULL;
static lv_obj_t *obj_album = NULL;
static lv_obj_t *obj_spectrum = NULL;
static lv_obj_t *obj_lyrics = NULL; // 歌词容器

static lv_obj_t *img_album = NULL;
static lv_obj_t *label_song_name = NULL;
static lv_obj_t *label_format_info = NULL;
static lv_obj_t *bar_progress = NULL;
static lv_obj_t *label_curr_time = NULL;
static lv_obj_t *label_total_time = NULL;

// Icon objects that need updating
static lv_obj_t *btn_mode = NULL;
static lv_obj_t *btn_play = NULL;
static lv_obj_t *btn_like = NULL;

static lv_obj_t *spectrum_bars[SPECTRUM_NUM] = {NULL};
static uint8_t like_state = 0;

// 歌词显示使用的 Label (最多显示5行: 前2行, 当前行, 后2行)
static lv_obj_t *lrc_labels[5] = {NULL};
static lrc_decoder_t lrc_decoder = {0};
static int16_t last_lrc_index = -2; // 记录上一次歌词行号，避免重复刷新
static bool show_lyrics = false;    // 状态机：是否处于歌词显示模式

// Cache for change detection
static char last_music_name[128] = {0};

// Helper to check if file exists
static bool file_exists(const char *path) {
    FILINFO fno;
    return (f_stat(path, &fno) == FR_OK);
}

// Helper to remove padding and scrollbars
static void obj_style_init_clean(lv_obj_t *obj) {
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// 视图切换回调
static void toggle_view_event_cb(lv_event_t * e) {
    show_lyrics = !show_lyrics;
    if(show_lyrics) {
        lv_obj_add_flag(obj_album, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(obj_spectrum, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(obj_lyrics, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj_album, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(obj_spectrum, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(obj_lyrics, LV_OBJ_FLAG_HIDDEN);
    }
}

// Event Callback
static void music_icon_event_handler(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_target(e);
    int id = (int)(intptr_t)lv_event_get_user_data(e);

    switch(id) {
        case 1: // Exit
			Page_Request_Switch(PAGE_DESKTOP);
            break;
        case 2: // Menu
			Page_Request_Switch(PAGE_FILE);
            break;
        case 3: // Mode Switch
            Music_Switch_Method++;
            if(Music_Switch_Method > 2) Music_Switch_Method = 0;
            break;
        case 4: // Like
            like_state = !like_state;
            if(like_state) lv_obj_set_style_img_recolor(target, lv_palette_lighten(LV_PALETTE_RED, 1), 0);
            else lv_obj_set_style_img_recolor(target, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
            break;
        case 5: // Prev
			Music_Status = Song_Previous;
            break;
        case 6: // Play/Suspend
            Music_Suspend_Flag = !Music_Suspend_Flag;
            break;
        case 7: // Next
			Music_Status = Song_Next;
            break;
    }
}

// Helper to create an clickable icon
static lv_obj_t* create_icon_btn(lv_obj_t *parent, const void *src, int x, int y, int id) {
    lv_obj_t * img = lv_img_create(parent);
    lv_img_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    lv_obj_set_size(img, 24, 24);
    
    // Style: Grey Color 
    lv_obj_set_style_img_recolor(img, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0); 
    
    // Interaction
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img, music_icon_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)id);
    
    return img;
}

// 初始化新的歌词与图片
static void load_music_resources(const char *music_name) {
    // 1. 处理专辑图片
    char img_path[64];
    snprintf(img_path, sizeof(img_path), "0:/PICTURE/%s.bmp", music_name);
    if (file_exists(img_path)) {
        lv_img_set_src(img_album, img_path);
    } else {
        lv_img_set_src(img_album, "0:/PICTURE/default.bmp");
    }

    // 2. 处理歌词文件
    lrc_decoder_close(&lrc_decoder);
    char lrc_path[64];
    snprintf(lrc_path, sizeof(lrc_path), "0:/LYRICS/%s.lrc", music_name);
    
    // 假设歌词文件最大 16KB，最多 200 行
    if (!lrc_decoder_open(&lrc_decoder, lrc_path, 16384, 200)) {
        // 解析失败或不存在
        for(int i = 0; i < 5; i++) lv_label_set_text(lrc_labels[i], "");
        lv_label_set_text(lrc_labels[2], "No Lyrics");
        lv_obj_set_style_text_color(lrc_labels[2], lv_color_black(), 0);
    }
    last_lrc_index = -2; // 强制下次 Update 时刷新歌词
}

void Create_Music_Unit(void)
{
    if (music_unit != NULL) return;	
	
    memset(last_music_name, 0, sizeof(last_music_name));
    show_lyrics = false; 

    // 【新增】0. 创建桌面壁纸 (保证在最底层)
    wallpaper_bg = lv_img_create(lv_scr_act());
    lv_img_set_src(wallpaper_bg, &pic_back);
    lv_obj_align(wallpaper_bg, LV_ALIGN_CENTER, 0, 0);

    // 1. Main Container (0,0) 240x210
    music_unit = lv_obj_create(lv_scr_act());
    lv_obj_set_size(music_unit, 240, 210);
    lv_obj_set_pos(music_unit, 0, 0);
    obj_style_init_clean(music_unit);

	// 2. Album Art Container
    obj_album = lv_obj_create(music_unit);
    lv_obj_set_size(obj_album, 120, 120);
    lv_obj_set_pos(obj_album, 10, 10);
    obj_style_init_clean(obj_album);
	lv_obj_set_style_radius(obj_album, 12, 0);
    lv_obj_set_style_border_width(obj_album, 1, 0);
    lv_obj_set_style_border_color(obj_album, lv_color_hex(0xCCCCCC), 0);
    
    // 杜绝全屏重绘时的遮罩内存雪崩
    lv_obj_set_style_clip_corner(obj_album, true, 0); 

    img_album = lv_img_create(obj_album);
    lv_obj_center(img_album);

    // 3. Spectrum Area (138,10) 60x120
    obj_spectrum = lv_obj_create(music_unit);
    lv_obj_set_size(obj_spectrum, 60, 120);
    lv_obj_set_pos(obj_spectrum, 138, 10);
    obj_style_init_clean(obj_spectrum);
    
    // 给频谱增加点击切换事件
    lv_obj_add_flag(obj_spectrum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj_spectrum, toggle_view_event_cb, LV_EVENT_CLICKED, NULL);

	for(int i = 0; i < SPECTRUM_NUM; i++) {
        spectrum_bars[i] = lv_obj_create(obj_spectrum);
        lv_obj_set_size(spectrum_bars[i], 0, 5); 
        lv_obj_set_pos(spectrum_bars[i], 0, 120 - 6 - (i * 6));
        obj_style_init_clean(spectrum_bars[i]);
        lv_obj_set_style_bg_color(spectrum_bars[i], lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_bg_opa(spectrum_bars[i], LV_OPA_COVER, 0);
    }
	
    // 3.5 歌词显示容器 (10, 10) 188x120
	obj_lyrics = lv_obj_create(music_unit);
	lv_obj_set_size(obj_lyrics, 188, 120); // 120 + 8 + 60 = 188
	lv_obj_set_pos(obj_lyrics, 10, 10);
	obj_style_init_clean(obj_lyrics);
    
    // 保持白色背景与 50% 透明度
	lv_obj_set_style_bg_color(obj_lyrics, lv_color_white(), 0); 
	lv_obj_set_style_bg_opa(obj_lyrics, LV_OPA_50, 0); 

	lv_obj_set_style_radius(obj_lyrics, 12, 0);
	// 【修改】加回歌词面板边框，设置为浅灰色
	lv_obj_set_style_border_width(obj_lyrics, 1, 0);
	lv_obj_set_style_border_color(obj_lyrics, lv_color_hex(0xCCCCCC), 0);
	lv_obj_set_style_shadow_width(obj_lyrics, 0, 0);

	lv_obj_add_flag(obj_lyrics, LV_OBJ_FLAG_HIDDEN); // 默认隐藏
    
    // 给歌词容器增加点击切换事件
    lv_obj_add_flag(obj_lyrics, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj_lyrics, toggle_view_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建 5 行歌词 Label (自动换行、居中对齐)
	for(int i = 0; i < 5; i++) 
	{
		lrc_labels[i] = lv_label_create(obj_lyrics);
		lv_obj_set_width(lrc_labels[i], LV_PCT(100)); // 让宽度自适应容器内部(186)
		lv_label_set_long_mode(lrc_labels[i], LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_align(lrc_labels[i], LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_set_style_text_font(lrc_labels[i], &lv_font_12, 0);
		
		// 去除标签默认的内边距，确保文本真正水平居中
		lv_obj_set_style_pad_all(lrc_labels[i], 0, 0);

        if (i == 2) { 
            // 当前播放的歌词：纯黑色，并作为基准居中
            lv_obj_set_style_text_color(lrc_labels[i], lv_color_black(), 0);
            lv_obj_align(lrc_labels[i], LV_ALIGN_CENTER, 0, 0);
        } else { 
            // 其它（已播放/未播放）的歌词：淡灰色
            lv_obj_set_style_text_color(lrc_labels[i], lv_palette_main(LV_PALETTE_GREY), 0);
        }
    }
	
    // 对齐其它几行 (相对中心行上下排列)
    lv_obj_align_to(lrc_labels[1], lrc_labels[2], LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_obj_align_to(lrc_labels[0], lrc_labels[1], LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_obj_align_to(lrc_labels[3], lrc_labels[2], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_align_to(lrc_labels[4], lrc_labels[3], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

	// 初次加载资源
    load_music_resources(music_info.music_name);

    // 4. Icons (Right Side)
    // (206,10): Exit
    lv_obj_t* btn_exit = create_icon_btn(music_unit, &music_exit, 206, 10, 1);
    lv_obj_set_style_img_recolor(btn_exit, lv_palette_lighten(LV_PALETTE_ORANGE, 1), 0);
    
    // (206,42): Menu
    create_icon_btn(music_unit, &music_menu, 206, 42, 2);
    
    // (206,74): Mode (Dynamic)
    const void *mode_src = &music_order;
    if (Music_Switch_Method == Play_Randomly) mode_src = &music_random;
    else if (Music_Switch_Method == Play_Repeatly) mode_src = &music_cycle;
    btn_mode = create_icon_btn(music_unit, mode_src, 206, 74, 3);
    
    // (206,106): Like
    btn_like = create_icon_btn(music_unit, &music_like, 206, 106, 4);
    if(like_state) lv_obj_set_style_img_recolor(btn_like, lv_palette_lighten(LV_PALETTE_RED, 1), 0);

    // 5. Song Title (10,138) 120x24
    lv_obj_t *cont_song = lv_obj_create(music_unit);
    lv_obj_set_size(cont_song, 120, 24);
    lv_obj_set_pos(cont_song, 10, 138);
    obj_style_init_clean(cont_song);

    label_song_name = lv_label_create(cont_song);
    lv_obj_set_style_text_font(label_song_name, &lv_font_16, 0);
    lv_label_set_long_mode(label_song_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_song_name, 120);
    lv_obj_align(label_song_name, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(label_song_name, music_info.music_name);
	lv_obj_set_style_anim_speed(label_song_name, 12, 0); 

    // 7. Controls Container (138,138) 92x24
    lv_obj_t *cont_ctrl = lv_obj_create(music_unit);
    lv_obj_set_size(cont_ctrl, 92, 24);
    lv_obj_set_pos(cont_ctrl, 138, 138);
    obj_style_init_clean(cont_ctrl);

    // Controls Icons
    // (0,0): Before
    create_icon_btn(cont_ctrl, &music_before, 0, 0, 5);

    // (34,0): Play/Suspend (Dynamic)
    const void *play_src = (Music_Suspend_Flag == 1) ? &music_playing : &music_suspend;
    btn_play = create_icon_btn(cont_ctrl, play_src, 34, 0, 6);

    // (68,0): Next
    create_icon_btn(cont_ctrl, &music_next, 68, 0, 7);

    // 8. Format Info (10,162) 220x15
    lv_obj_t *cont_fmt = lv_obj_create(music_unit);
    lv_obj_set_size(cont_fmt, 220, 15);
    lv_obj_set_pos(cont_fmt, 10, 162);
    obj_style_init_clean(cont_fmt);

	label_format_info = lv_label_create(cont_fmt);
    lv_obj_set_style_text_font(label_format_info, &lv_font_12, 0);
    lv_obj_set_width(label_format_info, 220);
	lv_label_set_long_mode(label_format_info, LV_LABEL_LONG_DOT); 
    lv_obj_align(label_format_info, LV_ALIGN_LEFT_MID, 0, 0);
	
    // 9. Progress Bar Area (10,180) 220x20
    lv_obj_t *cont_prog = lv_obj_create(music_unit);
    lv_obj_set_size(cont_prog, 220, 20);
    lv_obj_set_pos(cont_prog, 10, 180);
    obj_style_init_clean(cont_prog);

    bar_progress = lv_bar_create(cont_prog);
    lv_obj_set_size(bar_progress, 140, 6); 
    lv_obj_align(bar_progress, LV_ALIGN_CENTER, 0, 0); 
    lv_obj_set_style_radius(bar_progress, 2, 0);
    lv_obj_set_style_border_width(bar_progress, 0, 0); 
	lv_obj_set_style_bg_color(bar_progress, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_progress, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
    lv_obj_set_style_anim_time(bar_progress, 100, 0);

    label_curr_time = lv_label_create(cont_prog);
    lv_obj_set_style_text_font(label_curr_time, &lv_font_12, 0);
    lv_label_set_text(label_curr_time, "00:00");
    lv_obj_align(label_curr_time, LV_ALIGN_LEFT_MID, 0, 0);

    label_total_time = lv_label_create(cont_prog);
    lv_obj_set_style_text_font(label_total_time, &lv_font_12, 0);
    lv_label_set_text(label_total_time, "00:00");
    lv_obj_align(label_total_time, LV_ALIGN_RIGHT_MID, 0, 0);
	
	Spectrum_Init();
}

void Update_Music_Unit(void)
{
    if (music_unit == NULL) return;

    // Check if music name has changed compared to cache
    if (strncmp(last_music_name, music_info.music_name, sizeof(last_music_name)) != 0) {
        
        // 1. Update Cache
        strncpy(last_music_name, music_info.music_name, sizeof(last_music_name) - 1);
        last_music_name[sizeof(last_music_name) - 1] = '\0'; // Ensure safety

        // 2. Update Text
        lv_label_set_text(label_song_name, music_info.music_name);

        // 3. Load Resources (Image fallback & LRC parsing)
        load_music_resources(music_info.music_name);

        const char* fmt_str = "unk";
        switch(music_info.file_format) {
            case 1: fmt_str = "wav"; break;
            case 2: fmt_str = "mp3"; break;
            case 3: fmt_str = "flac"; break;
            case 4: fmt_str = "aac"; break;
            case 5: fmt_str = "ape"; break;
        }
        
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %dbit %dkHz %dkbps", 
                 fmt_str, 
                 music_info.bit_depth, 
                 music_info.samplerate / 1000, 
                 music_info.bitrate / 1000);
        lv_label_set_text(label_format_info, buf);
    }

    // --- 歌词更新逻辑 ---
    if (lrc_decoder.line_count > 0 && show_lyrics) {
        // 当前播放时间毫秒 (如果 music_info 只有 current_sec，就乘 1000)
        uint32_t curr_ms = music_info.current_sec * 1000;
        int16_t current_idx = lrc_decoder_get_line_index(&lrc_decoder, curr_ms);

        if (current_idx != last_lrc_index) {
            last_lrc_index = current_idx;

            // 刷新 5 行标签内容
            for (int i = 0; i < 5; i++) {
                int16_t target_idx = current_idx + (i - 2); // 偏移量: -2, -1, 0, 1, 2
                
				if (target_idx >= 0 && target_idx < lrc_decoder.line_count) {
                    char *text_ptr = lrc_decoder.lines[target_idx].text;
                    while(*text_ptr == ' ' || *text_ptr == '\t') {
                        text_ptr++;
                    }
                    lv_label_set_text(lrc_labels[i], text_ptr);
                } else {
                    lv_label_set_text(lrc_labels[i], ""); // 越界清空
                }
            }

            // 动态对齐（因为自动换行可能导致高度改变，需要重新相对居中对齐）
            lv_obj_align(lrc_labels[2], LV_ALIGN_CENTER, 0, 0);
            lv_obj_align_to(lrc_labels[1], lrc_labels[2], LV_ALIGN_OUT_TOP_MID, 0, -5);
            lv_obj_align_to(lrc_labels[0], lrc_labels[1], LV_ALIGN_OUT_TOP_MID, 0, -5);
            lv_obj_align_to(lrc_labels[3], lrc_labels[2], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
            lv_obj_align_to(lrc_labels[4], lrc_labels[3], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        }
    }

    // Update Mode Icon
    if(btn_mode) {
        if (Music_Switch_Method == Play_In_Order) lv_img_set_src(btn_mode, &music_order);
        else if (Music_Switch_Method == Play_Randomly) lv_img_set_src(btn_mode, &music_random);
        else if (Music_Switch_Method == Play_Repeatly) lv_img_set_src(btn_mode, &music_cycle);
    }

    // Update Play/Pause Icon
    if(btn_play) {
        if (Music_Suspend_Flag == 1) lv_img_set_src(btn_play, &music_suspend);
        else lv_img_set_src(btn_play, &music_playing);
    }

    // Update Progress
    lv_bar_set_range(bar_progress, 0, music_info.total_sec);
    lv_bar_set_value(bar_progress, music_info.current_sec, LV_ANIM_OFF);

    // Time Labels
    static uint32_t last_current_sec = 0xFFFFFFFF;
    static uint32_t last_total_sec = 0xFFFFFFFF;

    static char buf_total[16];
    static char buf_curr[16];

    if (music_info.total_sec != last_total_sec) {
        last_total_sec = music_info.total_sec;
        lv_bar_set_range(bar_progress, 0, music_info.total_sec);
        
        snprintf(buf_total, sizeof(buf_total), "%02d:%02d", music_info.total_sec/60, music_info.total_sec%60);
        lv_label_set_text_static(label_total_time, buf_total); 
    }

    if (music_info.current_sec != last_current_sec) {
        last_current_sec = music_info.current_sec;
        lv_bar_set_value(bar_progress, music_info.current_sec, LV_ANIM_OFF);
        
        snprintf(buf_curr, sizeof(buf_curr), "%02d:%02d", music_info.current_sec/60, music_info.current_sec%60);
        lv_label_set_text_static(label_curr_time, buf_curr); 
    }
	
	// Update DSP Horizontal Bars (仅在歌词未显示时重绘以节省性能)
	static uint8_t limit = 0; limit = !limit; if(limit) return;
	
	Spectrum_Loop(); 
	
	static uint16_t current_w_100[SPECTRUM_NUM] = {0}; 
	static lv_coord_t last_drawn_w[SPECTRUM_NUM] = {0};

    // 如果处于专辑页面，更新柱形图宽度
    if (!show_lyrics && spectrum_bars[0] != NULL) 
	{
        for(int i = 0; i < SPECTRUM_NUM; i++) {
            uint16_t target_w_100 = g_spectrum_data[i] * 60; 

            if (current_w_100[i] < target_w_100) {
                current_w_100[i] += (target_w_100 - current_w_100[i]) >> 1;
            } else if (current_w_100[i] > target_w_100) {
                current_w_100[i] -= (current_w_100[i] - target_w_100) >> 1;
            }

            lv_coord_t w = current_w_100[i] / 100;
            if(w < 0) w = 0;
            if(w > 60) w = 60;

            if(w != last_drawn_w[i]) { 
                lv_obj_set_width(spectrum_bars[i], w);
                last_drawn_w[i] = w;
            }
        }
    }
}

void Remove_Music_Unit(void)
{
    if (music_unit != NULL) {
        lv_obj_del(music_unit);
        music_unit = NULL;
        
        // 【新增】销毁壁纸对象
        if (wallpaper_bg != NULL) {
            lv_obj_del(wallpaper_bg);
            wallpaper_bg = NULL;
        }
        
        obj_album = NULL;
        obj_spectrum = NULL;
        obj_lyrics = NULL;
        img_album = NULL;
        label_song_name = NULL;
        label_format_info = NULL;
        bar_progress = NULL;
        label_curr_time = NULL;
        label_total_time = NULL;
        btn_mode = NULL;
        btn_play = NULL;

        for(int i = 0; i < 5; i++) lrc_labels[i] = NULL;
        
        for(int i = 0; i < SPECTRUM_NUM; i++) {
            spectrum_bars[i] = NULL;
        }

        memset(last_music_name, 0, sizeof(last_music_name));
        
        // 释放歌词内存
        lrc_decoder_close(&lrc_decoder);
		
		Spectrum_Deinit();
    }
}
