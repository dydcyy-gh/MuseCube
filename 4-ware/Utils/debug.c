#include "stm32f4xx.h"
#include "tsdb_log.h"
#include "debug_unit.h"
#include "usbd_cdc_conf.h"
#include "variables.h"
#include "defines.h"

void Debug_Dump_TSDB_To_USB(int count)
{
    if (kv_debug_mode != Debug_Mode_None) return;
    usb_printf("--- TSDB Dump Start (%d) ---\r\n", count);
    tsdb_show_recent_on_usb(count);
    usb_printf("--- TSDB Dump End ---\r\n");
}

void Debug_Dump_TSDB_To_LVGL(int count)
{
    if (kv_debug_mode != Debug_Mode_None) return;
    lvgl_printf("--- TSDB Dump Start (%d) ---\n", count);
    tsdb_show_recent_on_lvgl(count);
    lvgl_printf("--- TSDB Dump End ---\n");
}
