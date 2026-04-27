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

#ifndef LV_ATTRIBUTE_IMG_CARD_ICON
#define LV_ATTRIBUTE_IMG_CARD_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_CARD_ICON uint8_t card_icon_map[] = {
  0x00, 0x00, 
  0x07, 0xfc, 
  0x0f, 0xfe, 
  0x1c, 0x06, 
  0x38, 0x06, 
  0x72, 0xa6, 
  0x62, 0xa6, 
  0x62, 0xa6, 
  0x60, 0x06, 
  0x60, 0x06, 
  0x67, 0xe6, 
  0x68, 0x16, 
  0x68, 0x16, 
  0x68, 0x16, 
  0x68, 0x16, 
  0x67, 0xe6, 
  0x60, 0x06, 
  0x7f, 0xfe, 
  0x3f, 0xfc, 
  0x00, 0x00, 
};

const lv_img_dsc_t card_icon = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 16,
  .header.h = 20,
  .data_size = 40,
  .data = card_icon_map,
};

#ifndef LV_ATTRIBUTE_IMG_CARD_ERR_ICON
#define LV_ATTRIBUTE_IMG_CARD_ERR_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_CARD_ERR_ICON uint8_t card_err_icon_map[] = {
  0x00, 0x00, 
  0x07, 0xfc, 
  0x0f, 0xfe, 
  0x1c, 0x06, 
  0x38, 0x06, 
  0x72, 0xa6, 
  0x62, 0xa6, 
  0x62, 0xa6, 
  0x60, 0x06, 
  0x60, 0x06, 
  0x60, 0x06, 
  0x6c, 0x36, 
  0x66, 0xe6, 
  0x61, 0x86, 
  0x67, 0x66, 
  0x6c, 0x36, 
  0x60, 0x06, 
  0x7f, 0xfe, 
  0x3f, 0xfc, 
  0x00, 0x00, 
};

const lv_img_dsc_t card_err_icon = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 16,
  .header.h = 20,
  .data_size = 40,
  .data = card_err_icon_map,
};
