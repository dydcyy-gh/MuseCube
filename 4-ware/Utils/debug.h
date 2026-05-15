#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdint.h>
#include "tsdb_log.h"
#include "debug_unit.h"
#include "usbd_cdc_conf.h"
#include "variables.h"
#include "defines.h"

#define debug_printf(format, ...) do {                                  \
    if (debug_mode == Debug_Mode_None) break;                           \
    switch (debug_mode) {                                               \
        case Debug_Mode_TSDB:                                           \
            tsdb_printf(format, ##__VA_ARGS__);                         \
            break;                                                      \
        case Debug_Mode_USBD:                                           \
            usb_printf(format, ##__VA_ARGS__);                          \
            break;                                                      \
        case Debug_Mode_LVGL:                                           \
            lvgl_printf(format, ##__VA_ARGS__);                         \
            break;                                                      \
        default:                                                        \
            break;                                                      \
    }                                                                   \
} while(0)

// 历史数据导出 (Debug_Mode_None 模式下调用)
void Debug_Dump_TSDB_To_USB(int count);
void Debug_Dump_TSDB_To_LVGL(int count);

#endif
