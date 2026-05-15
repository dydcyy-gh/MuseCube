#if 1

#include "lv_port_disp.h"
#include "lvgl.h"
#include "lcd_bsp.h"
#include "lcd_pwm.h"
#include "malloc.h"
#include <stdbool.h>

#define MY_DISP_HOR_RES (240) //宽度
#define MY_DISP_VER_RES (240) //高度

static lv_disp_draw_buf_t draw_buf_dsc_2;
static lv_disp_drv_t disp_drv;

static lv_color_t *buf_2_1 = NULL;                  
static lv_color_t *buf_2_2 = NULL;  

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

void lv_port_disp_deinit(void)
{
    draw_buf_dsc_2.buf1 = NULL;
    draw_buf_dsc_2.buf2 = NULL;

    if(buf_2_1) {
        free_bsc(buf_2_1);
        buf_2_1 = NULL;
    }
    if(buf_2_2) {
        free_bsc(buf_2_2);
        buf_2_2 = NULL;
    }
}

void lv_port_disp_reinit(void)
{
    buf_2_1 = (lv_color_t *)malloc_bsc(MY_DISP_HOR_RES * 20 * sizeof(lv_color_t));
    buf_2_2 = (lv_color_t *)malloc_bsc(MY_DISP_HOR_RES * 20 * sizeof(lv_color_t));

    if(buf_2_1 != NULL && buf_2_2 != NULL) {
        lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 20);
    }
}

void lv_port_disp_init(void)
{
    LCD_Init();

    lv_port_disp_reinit();

    if(!buf_2_1 || !buf_2_2) return;

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc_2; // draw_buf_dsc_2 的地址是不变的
    lv_disp_drv_register(&disp_drv);
}

static lv_disp_drv_t *current_disp_drv = NULL;

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
	current_disp_drv = disp_drv;
    LCD_Color_Fill(area->x1, area->y1, area->x2, area->y2, (uint16_t *)color_p);
}

void LCD_DMA_TransferComplete(void)
{
    if(current_disp_drv != NULL)
    {
        lv_disp_flush_ready(current_disp_drv);
        current_disp_drv = NULL;
    }
}

#else
typedef int keep_pedantic_happy;
#endif
