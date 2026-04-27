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

#ifndef LV_ATTRIBUTE_IMG_USB_ICON
#define LV_ATTRIBUTE_IMG_USB_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_USB_ICON uint8_t usb_icon_map[] = {
  0x00, 0x00, 
  0x1f, 0xe0, 
  0x3f, 0xf0, 
  0x30, 0x30, 
  0x34, 0xb0, 
  0x34, 0xb0, 
  0x30, 0x30, 
  0x7f, 0xf8, 
  0x7f, 0xf8, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x60, 0x18, 
  0x7f, 0xf8, 
  0x3f, 0xf0, 
  0x00, 0x00, 
};

const lv_img_dsc_t usb_icon = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 14,
  .header.h = 20,
  .data_size = 40,
  .data = usb_icon_map,
};

#ifndef LV_ATTRIBUTE_IMG_USB_ERR_ICON
#define LV_ATTRIBUTE_IMG_USB_ERR_ICON
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_USB_ERR_ICON uint8_t usb_err_icon_map[] = {
  0x00, 0x00, 
  0x1f, 0xe0, 
  0x3f, 0xf0, 
  0x30, 0x30, 
  0x34, 0xb0, 
  0x34, 0xb0, 
  0x30, 0x30, 
  0x7f, 0xf8, 
  0x7f, 0xf8, 
  0x60, 0x18, 
  0x63, 0x18, 
  0x63, 0x18, 
  0x63, 0x18, 
  0x63, 0x18, 
  0x60, 0x18, 
  0x63, 0x18, 
  0x60, 0x18, 
  0x7f, 0xf8, 
  0x3f, 0xf0, 
  0x00, 0x00, 
};

const lv_img_dsc_t usb_err_icon = {
  .header.cf = LV_IMG_CF_ALPHA_1BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 14,
  .header.h = 20,
  .data_size = 40,
  .data = usb_err_icon_map,
};
