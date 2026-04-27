#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_FILE_EXIT_ICON
#define LV_ATTRIBUTE_IMG_FILE_EXIT_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_FILE_EXIT_ICON uint8_t file_exit_icon_map[] = {
  0x00, 0x30, 
  0x00, 0x70, 
  0x00, 0xe0, 
  0x01, 0xc0, 
  0x03, 0x80, 
  0x07, 0x00, 
  0x0e, 0x00, 
  0x1c, 0x00, 
  0x1c, 0x00, 
  0x0e, 0x00, 
  0x07, 0x00, 
  0x03, 0x80, 
  0x01, 0xc0, 
  0x00, 0xe0, 
  0x00, 0x70, 
  0x00, 0x30, 
};

const lv_img_dsc_t file_exit_icon = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 16,
  .header.h = 16,
  .data_size = 32,
  .data = file_exit_icon_map,
};

#ifndef LV_ATTRIBUTE_IMG_FILE_FILE_ICON
#define LV_ATTRIBUTE_IMG_FILE_FILE_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_FILE_FILE_ICON uint8_t file_file_icon_map[] = {
	0x3f, 0xc0, 
	0x20, 0x60, 
	0x20, 0x50, 
	0x20, 0x48, 
	0x20, 0x7c, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x20, 0x04, 
	0x3f, 0xfc, 
};

const lv_img_dsc_t file_file_icon = {
	.header.cf = LV_IMG_CF_ALPHA_1BIT,
	.header.always_zero = 0,
	.header.reserved = 0,
	.header.w = 16,
	.header.h = 16,
	.data_size = 32,
	.data = file_file_icon_map,
};

#ifndef LV_ATTRIBUTE_IMG_FILE_FOLDER_ICON
#define LV_ATTRIBUTE_IMG_FILE_FOLDER_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_FILE_FOLDER_ICON uint8_t file_folder_icon_map[] = {
	0x00, 0x00, 
	0xf8, 0x00, 
	0x84, 0x00, 
	0x87, 0xfe, 
	0xf8, 0x01, 
	0x80, 0x01, 
	0x80, 0x01, 
	0x80, 0x01, 
	0x80, 0x01, 
	0x80, 0x01, 
	0x80, 0x01, 
	0x8f, 0xf1, 
	0x90, 0x09, 
	0x90, 0x09, 
	0xff, 0xff, 
	0x00, 0x00, 
};

const lv_img_dsc_t file_folder_icon = {
	.header.cf = LV_IMG_CF_ALPHA_1BIT,
	.header.always_zero = 0,
	.header.reserved = 0,
	.header.w = 16,
	.header.h = 16,
	.data_size = 32,
	.data = file_folder_icon_map,
};

