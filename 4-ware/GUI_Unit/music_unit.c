#include "stm32f4xx.h"
#include "lv_port_disp.h"
#include "page_manager.h"
#include <stdio.h>
#include <string.h>
#include "variables.h"
#include "defines.h"
#include "music.h"
#include "spectrum_dsp.h"
#include "ff.h"
#include "lrc.h"
#include "malloc.h"

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

// --- 状态结构体 ---
typedef struct {
	lv_obj_t *wallpaper_bg;
	lv_obj_t *music_unit;
	lv_obj_t *obj_album;
	lv_obj_t *obj_spectrum;
	lv_obj_t *obj_lyrics;

	lv_obj_t *img_album;
	lv_obj_t *label_song_name;
	lv_obj_t *label_format_info;
	lv_obj_t *bar_progress;
	lv_obj_t *label_curr_time;
	lv_obj_t *label_total_time;

	lv_obj_t *btn_mode;
	lv_obj_t *btn_play;
	lv_obj_t *btn_like;

	lv_obj_t *spectrum_bars[SPECTRUM_NUM];
	uint8_t like_state;

	lv_obj_t *lrc_labels[5];
	lrc_decoder_t lrc_decoder;
	int16_t last_lrc_index;
	bool show_lyrics;

	char last_music_name[128];

	// Update_Music_Unit 内部持久状态
	uint32_t last_current_sec;
	uint32_t last_total_sec;
	char buf_total[16];
	char buf_curr[16];
	uint8_t limit;
	uint16_t current_w_100[SPECTRUM_NUM];
	lv_coord_t last_drawn_w[SPECTRUM_NUM];
} music_state_t;

static music_state_t *ms = NULL;

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
	ms->show_lyrics = !ms->show_lyrics;
	if(ms->show_lyrics) {
		lv_obj_add_flag(ms->obj_album, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(ms->obj_spectrum, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ms->obj_lyrics, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_clear_flag(ms->obj_album, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ms->obj_spectrum, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(ms->obj_lyrics, LV_OBJ_FLAG_HIDDEN);
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
			Page_Request_Switch(PAGE_FILE,"0:/MUSIC");
			break;
		case 3: // Mode Switch
			Music_Switch_Method++;
			if(Music_Switch_Method > 2) Music_Switch_Method = 0;
			break;
		case 4: // Like
			ms->like_state = !ms->like_state;
			if(ms->like_state) lv_obj_set_style_img_recolor(target, lv_palette_lighten(LV_PALETTE_RED, 1), 0);
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

	lv_obj_set_style_img_recolor(img, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
	lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);

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
		lv_img_set_src(ms->img_album, img_path);
	} else {
		lv_img_set_src(ms->img_album, "0:/PICTURE/default.bmp");
	}

	// 2. 处理歌词文件
	lrc_decoder_close(&ms->lrc_decoder);
	char lrc_path[64];
	snprintf(lrc_path, sizeof(lrc_path), "0:/LYRICS/%s.lrc", music_name);

	if (!lrc_decoder_open(&ms->lrc_decoder, lrc_path, 16384, 200)) {
		for(int i = 0; i < 5; i++) lv_label_set_text(ms->lrc_labels[i], "");
		lv_label_set_text(ms->lrc_labels[2], "No Lyrics");
		lv_obj_set_style_text_color(ms->lrc_labels[2], lv_color_black(), 0);
	}
	ms->last_lrc_index = -2;
}

void Create_Music_Unit(void)
{
	if (ms != NULL) return;

	ms = (music_state_t *)malloc_ccm(sizeof(music_state_t));
	if (!ms) return;
	memset(ms, 0, sizeof(music_state_t));

	ms->show_lyrics = false;
	ms->last_lrc_index = -2;
	ms->last_current_sec = 0xFFFFFFFF;
	ms->last_total_sec = 0xFFFFFFFF;

	// 0. 创建纯白色背景
	ms->wallpaper_bg = lv_obj_create(lv_scr_act());
	lv_obj_set_size(ms->wallpaper_bg, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(ms->wallpaper_bg, 0, 0);
	lv_obj_set_style_bg_color(ms->wallpaper_bg, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(ms->wallpaper_bg, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(ms->wallpaper_bg, 0, 0);
	lv_obj_set_style_pad_all(ms->wallpaper_bg, 0, 0);
	lv_obj_set_style_radius(ms->wallpaper_bg, 0, 0);

	// 1. Main Container
	ms->music_unit = lv_obj_create(lv_scr_act());
	lv_obj_set_size(ms->music_unit, 240, 210);
	lv_obj_set_pos(ms->music_unit, 0, 0);
	obj_style_init_clean(ms->music_unit);

	// 2. Album Art Container
	ms->obj_album = lv_obj_create(ms->music_unit);
	lv_obj_set_size(ms->obj_album, 120, 120);
	lv_obj_set_pos(ms->obj_album, 10, 10);
	obj_style_init_clean(ms->obj_album);
	lv_obj_set_style_radius(ms->obj_album, 12, 0);
	lv_obj_set_style_border_width(ms->obj_album, 1, 0);
	lv_obj_set_style_border_color(ms->obj_album, lv_color_hex(0xCCCCCC), 0);
	lv_obj_set_style_clip_corner(ms->obj_album, true, 0);

	ms->img_album = lv_img_create(ms->obj_album);
	lv_obj_center(ms->img_album);

	// 3. Spectrum Area
	ms->obj_spectrum = lv_obj_create(ms->music_unit);
	lv_obj_set_size(ms->obj_spectrum, 60, 120);
	lv_obj_set_pos(ms->obj_spectrum, 138, 10);
	obj_style_init_clean(ms->obj_spectrum);
	lv_obj_add_flag(ms->obj_spectrum, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(ms->obj_spectrum, toggle_view_event_cb, LV_EVENT_CLICKED, NULL);

	for(int i = 0; i < SPECTRUM_NUM; i++) {
		ms->spectrum_bars[i] = lv_obj_create(ms->obj_spectrum);
		lv_obj_set_size(ms->spectrum_bars[i], 0, 5);
		lv_obj_set_pos(ms->spectrum_bars[i], 0, 120 - 6 - (i * 6));
		obj_style_init_clean(ms->spectrum_bars[i]);
		lv_obj_set_style_bg_color(ms->spectrum_bars[i], lv_palette_main(LV_PALETTE_BLUE), 0);
		lv_obj_set_style_bg_opa(ms->spectrum_bars[i], LV_OPA_COVER, 0);
	}

	// 3.5 歌词显示容器
	ms->obj_lyrics = lv_obj_create(ms->music_unit);
	lv_obj_set_size(ms->obj_lyrics, 188, 120);
	lv_obj_set_pos(ms->obj_lyrics, 10, 10);
	obj_style_init_clean(ms->obj_lyrics);
	lv_obj_set_style_bg_color(ms->obj_lyrics, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(ms->obj_lyrics, LV_OPA_50, 0);
	lv_obj_set_style_radius(ms->obj_lyrics, 12, 0);
	lv_obj_set_style_border_width(ms->obj_lyrics, 1, 0);
	lv_obj_set_style_border_color(ms->obj_lyrics, lv_color_hex(0xCCCCCC), 0);
	lv_obj_set_style_shadow_width(ms->obj_lyrics, 0, 0);
	lv_obj_add_flag(ms->obj_lyrics, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_flag(ms->obj_lyrics, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(ms->obj_lyrics, toggle_view_event_cb, LV_EVENT_CLICKED, NULL);

	// 创建 5 行歌词 Label
	for(int i = 0; i < 5; i++) {
		ms->lrc_labels[i] = lv_label_create(ms->obj_lyrics);
		lv_obj_set_width(ms->lrc_labels[i], LV_PCT(100));
		lv_label_set_long_mode(ms->lrc_labels[i], LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_align(ms->lrc_labels[i], LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_set_style_text_font(ms->lrc_labels[i], &lv_font_12, 0);
		lv_obj_set_style_pad_all(ms->lrc_labels[i], 0, 0);

		if (i == 2) {
			lv_obj_set_style_text_color(ms->lrc_labels[i], lv_color_black(), 0);
			lv_obj_align(ms->lrc_labels[i], LV_ALIGN_CENTER, 0, 0);
		} else {
			lv_obj_set_style_text_color(ms->lrc_labels[i], lv_palette_main(LV_PALETTE_GREY), 0);
		}
	}

	lv_obj_align_to(ms->lrc_labels[1], ms->lrc_labels[2], LV_ALIGN_OUT_TOP_MID, 0, -5);
	lv_obj_align_to(ms->lrc_labels[0], ms->lrc_labels[1], LV_ALIGN_OUT_TOP_MID, 0, -5);
	lv_obj_align_to(ms->lrc_labels[3], ms->lrc_labels[2], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
	lv_obj_align_to(ms->lrc_labels[4], ms->lrc_labels[3], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

	// 初次加载资源
	load_music_resources(music_info.music_name);

	// 4. Icons (Right Side)
	lv_obj_t* btn_exit = create_icon_btn(ms->music_unit, &music_exit, 206, 10, 1);
	lv_obj_set_style_img_recolor(btn_exit, lv_palette_lighten(LV_PALETTE_ORANGE, 1), 0);

	create_icon_btn(ms->music_unit, &music_menu, 206, 42, 2);

	const void *mode_src = &music_order;
	if (Music_Switch_Method == Play_Randomly) mode_src = &music_random;
	else if (Music_Switch_Method == Play_Repeatly) mode_src = &music_cycle;
	ms->btn_mode = create_icon_btn(ms->music_unit, mode_src, 206, 74, 3);

	ms->btn_like = create_icon_btn(ms->music_unit, &music_like, 206, 106, 4);
	if(ms->like_state) lv_obj_set_style_img_recolor(ms->btn_like, lv_palette_lighten(LV_PALETTE_RED, 1), 0);

	// 5. Song Title
	lv_obj_t *cont_song = lv_obj_create(ms->music_unit);
	lv_obj_set_size(cont_song, 120, 24);
	lv_obj_set_pos(cont_song, 10, 138);
	obj_style_init_clean(cont_song);

	ms->label_song_name = lv_label_create(cont_song);
	lv_obj_set_style_text_font(ms->label_song_name, &lv_font_16, 0);
	lv_label_set_long_mode(ms->label_song_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_obj_set_width(ms->label_song_name, 120);
	lv_obj_align(ms->label_song_name, LV_ALIGN_LEFT_MID, 0, 0);
	lv_label_set_text(ms->label_song_name, music_info.music_name);
	lv_obj_set_style_anim_speed(ms->label_song_name, 12, 0);

	// 7. Controls Container
	lv_obj_t *cont_ctrl = lv_obj_create(ms->music_unit);
	lv_obj_set_size(cont_ctrl, 92, 24);
	lv_obj_set_pos(cont_ctrl, 138, 138);
	obj_style_init_clean(cont_ctrl);

	create_icon_btn(cont_ctrl, &music_before, 0, 0, 5);

	const void *play_src = (Music_Suspend_Flag == 1) ? &music_playing : &music_suspend;
	ms->btn_play = create_icon_btn(cont_ctrl, play_src, 34, 0, 6);

	create_icon_btn(cont_ctrl, &music_next, 68, 0, 7);

	// 8. Format Info
	lv_obj_t *cont_fmt = lv_obj_create(ms->music_unit);
	lv_obj_set_size(cont_fmt, 220, 15);
	lv_obj_set_pos(cont_fmt, 10, 162);
	obj_style_init_clean(cont_fmt);

	ms->label_format_info = lv_label_create(cont_fmt);
	lv_obj_set_style_text_font(ms->label_format_info, &lv_font_12, 0);
	lv_obj_set_width(ms->label_format_info, 220);
	lv_label_set_long_mode(ms->label_format_info, LV_LABEL_LONG_DOT);
	lv_obj_align(ms->label_format_info, LV_ALIGN_LEFT_MID, 0, 0);

	// 9. Progress Bar Area
	lv_obj_t *cont_prog = lv_obj_create(ms->music_unit);
	lv_obj_set_size(cont_prog, 220, 20);
	lv_obj_set_pos(cont_prog, 10, 180);
	obj_style_init_clean(cont_prog);

	ms->bar_progress = lv_bar_create(cont_prog);
	lv_obj_set_size(ms->bar_progress, 140, 6);
	lv_obj_align(ms->bar_progress, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_radius(ms->bar_progress, 2, 0);
	lv_obj_set_style_border_width(ms->bar_progress, 0, 0);
	lv_obj_set_style_bg_color(ms->bar_progress, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
	lv_obj_set_style_bg_color(ms->bar_progress, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
	lv_obj_set_style_anim_time(ms->bar_progress, 100, 0);

	ms->label_curr_time = lv_label_create(cont_prog);
	lv_obj_set_style_text_font(ms->label_curr_time, &lv_font_12, 0);
	lv_label_set_text(ms->label_curr_time, "00:00");
	lv_obj_align(ms->label_curr_time, LV_ALIGN_LEFT_MID, 0, 0);

	ms->label_total_time = lv_label_create(cont_prog);
	lv_obj_set_style_text_font(ms->label_total_time, &lv_font_12, 0);
	lv_label_set_text(ms->label_total_time, "00:00");
	lv_obj_align(ms->label_total_time, LV_ALIGN_RIGHT_MID, 0, 0);

	Spectrum_Init();
}

void Update_Music_Unit(void)
{
	if (ms == NULL) return;

	// Check if music name has changed
	if (strncmp(ms->last_music_name, music_info.music_name, sizeof(ms->last_music_name)) != 0) {
		strncpy(ms->last_music_name, music_info.music_name, sizeof(ms->last_music_name) - 1);
		ms->last_music_name[sizeof(ms->last_music_name) - 1] = '\0';

		lv_label_set_text(ms->label_song_name, music_info.music_name);
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
		lv_label_set_text(ms->label_format_info, buf);
	}

	// 歌词更新逻辑
	if (ms->lrc_decoder.line_count > 0 && ms->show_lyrics) {
		uint32_t curr_ms = music_info.current_sec * 1000;
		int16_t current_idx = lrc_decoder_get_line_index(&ms->lrc_decoder, curr_ms);

		if (current_idx != ms->last_lrc_index) {
			ms->last_lrc_index = current_idx;

			for (int i = 0; i < 5; i++) {
				int16_t target_idx = current_idx + (i - 2);

				if (target_idx >= 0 && target_idx < ms->lrc_decoder.line_count) {
					char *text_ptr = ms->lrc_decoder.lines[target_idx].text;
					while(*text_ptr == ' ' || *text_ptr == '\t') {
						text_ptr++;
					}
					lv_label_set_text(ms->lrc_labels[i], text_ptr);
				} else {
					lv_label_set_text(ms->lrc_labels[i], "");
				}
			}

			lv_obj_align(ms->lrc_labels[2], LV_ALIGN_CENTER, 0, 0);
			lv_obj_align_to(ms->lrc_labels[1], ms->lrc_labels[2], LV_ALIGN_OUT_TOP_MID, 0, -5);
			lv_obj_align_to(ms->lrc_labels[0], ms->lrc_labels[1], LV_ALIGN_OUT_TOP_MID, 0, -5);
			lv_obj_align_to(ms->lrc_labels[3], ms->lrc_labels[2], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
			lv_obj_align_to(ms->lrc_labels[4], ms->lrc_labels[3], LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
		}
	}

	// Update Mode Icon
	if(ms->btn_mode) {
		if (Music_Switch_Method == Play_In_Order) lv_img_set_src(ms->btn_mode, &music_order);
		else if (Music_Switch_Method == Play_Randomly) lv_img_set_src(ms->btn_mode, &music_random);
		else if (Music_Switch_Method == Play_Repeatly) lv_img_set_src(ms->btn_mode, &music_cycle);
	}

	// Update Play/Pause Icon
	if(ms->btn_play) {
		if (Music_Suspend_Flag == 1) lv_img_set_src(ms->btn_play, &music_suspend);
		else lv_img_set_src(ms->btn_play, &music_playing);
	}

	// Update Progress
	if (music_info.total_sec != ms->last_total_sec) {
		ms->last_total_sec = music_info.total_sec;
		lv_bar_set_range(ms->bar_progress, 0, music_info.total_sec);
		snprintf(ms->buf_total, sizeof(ms->buf_total), "%02d:%02d", music_info.total_sec/60, music_info.total_sec%60);
		lv_label_set_text_static(ms->label_total_time, ms->buf_total);
	}

	if (music_info.current_sec != ms->last_current_sec) {
		ms->last_current_sec = music_info.current_sec;
		lv_bar_set_value(ms->bar_progress, music_info.current_sec, LV_ANIM_OFF);
		snprintf(ms->buf_curr, sizeof(ms->buf_curr), "%02d:%02d", music_info.current_sec/60, music_info.current_sec%60);
		lv_label_set_text_static(ms->label_curr_time, ms->buf_curr);
	}

	// Update DSP Horizontal Bars
	ms->limit = !ms->limit;
	if(ms->limit) return;

	Spectrum_Loop();

	if (!ms->show_lyrics && ms->spectrum_bars[0] != NULL) {
		for(int i = 0; i < SPECTRUM_NUM; i++) {
			uint16_t target_w_100 = g_spectrum_data[i] * 60;

			if (ms->current_w_100[i] < target_w_100) {
				ms->current_w_100[i] += (target_w_100 - ms->current_w_100[i]) >> 1;
			} else if (ms->current_w_100[i] > target_w_100) {
				ms->current_w_100[i] -= (ms->current_w_100[i] - target_w_100) >> 1;
			}

			lv_coord_t w = ms->current_w_100[i] / 100;
			if(w < 0) w = 0;
			if(w > 60) w = 60;

			if(w != ms->last_drawn_w[i]) {
				lv_obj_set_width(ms->spectrum_bars[i], w);
				ms->last_drawn_w[i] = w;
			}
		}
	}
}

void Remove_Music_Unit(void)
{
	if (ms == NULL) return;

	if (ms->music_unit != NULL) {
		lv_obj_del(ms->music_unit);
	}

	if (ms->wallpaper_bg != NULL) {
		lv_obj_del(ms->wallpaper_bg);
	}

	lrc_decoder_close(&ms->lrc_decoder);
	Spectrum_Deinit();

	free_ccm(ms);
	ms = NULL;
}
