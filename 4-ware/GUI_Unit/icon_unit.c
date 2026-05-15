#include "stm32f4xx.h" 
#include "lv_port_disp.h"
#include "pin_ctrl.h"
#include "variables.h"

// 引用状态栏中的 Flex 容器
extern lv_obj_t *icon_flex_cont;

// ==========================================
// 1. 保存图标 (Saving)
// ==========================================
static lv_obj_t * icon1;
extern const lv_img_dsc_t saving_icon;

void Create_Saving_Icon(void)
{
	if (icon1 != NULL) return;
    if (icon_flex_cont == NULL) return;
    
	icon1 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon1, &saving_icon);
	
	lv_obj_set_style_img_recolor(icon1, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon1, LV_OPA_COVER, 0);
}

void Remove_Saving_Icon(void)
{
	if (icon1 != NULL) 
	{
        lv_obj_del(icon1);
        icon1 = NULL;
    }
}

void Update_Saving_Icon(void)
{
    if(g_maintain_status) Create_Saving_Icon();
    else Remove_Saving_Icon();
}


// ==========================================
// 2. 耳机图标 (Headphone)
// ==========================================
static lv_obj_t * icon2;
extern const lv_img_dsc_t headphone_icon;

void Create_Headphone_Icon(void)
{
	if (icon2 != NULL) return;
    if (icon_flex_cont == NULL) return;

	icon2 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon2, &headphone_icon);
	
	lv_obj_set_style_img_recolor(icon2, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon2, LV_OPA_COVER, 0);
}

void Remove_Headphone_Icon(void)
{
	if (icon2 != NULL) 
	{
        lv_obj_del(icon2);
        icon2 = NULL;
    }
}

void Update_Headphone_Icon(void)
{
    if(g_headphone_status) Create_Headphone_Icon();
    else Remove_Headphone_Icon();
}

// ==========================================
// 3. SD卡图标 (TF Card)
// ==========================================
static lv_obj_t * icon3;
extern const lv_img_dsc_t card_err_icon;

void Create_Card_Icon(void)
{
	if (icon3 != NULL) return;
    if (icon_flex_cont == NULL) return;

	icon3 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon3, &card_err_icon);

	lv_obj_set_style_img_recolor(icon3, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon3, LV_OPA_COVER, 0);
}

void Remove_Card_Icon(void)
{
	if (icon3 != NULL) 
	{
        lv_obj_del(icon3);
        icon3 = NULL;
    }
}

void Update_Card_Icon(void)
{
    if(!g_TFcard_inited) Create_Card_Icon();
    else Remove_Card_Icon();
}

// ==========================================
// 4. USB图标 (USB)
// ==========================================
static lv_obj_t * icon4;
extern const lv_img_dsc_t usb_icon;

void Create_USB_Icon(void)
{
	if (icon4 != NULL) return;
    if (icon_flex_cont == NULL) return;

	icon4 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon4, &usb_icon);
	
	lv_obj_set_style_img_recolor(icon4, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon4, LV_OPA_COVER, 0);
}

void Remove_USB_Icon(void)
{
	if (icon4 != NULL) 
	{
        lv_obj_del(icon4);
        icon4 = NULL;
    }
}

void Update_USB_Icon(void)
{
    if(g_usb_status) Create_USB_Icon();
    else Remove_USB_Icon();
}

// ==========================================
// 5. 音量图标 (Volume)
// ==========================================
static lv_obj_t * icon5;
extern const lv_img_dsc_t volume_icon_0;

void Create_Volume_Icon(void)
{
	if (icon5 != NULL) return;
    if (icon_flex_cont == NULL) return;

	icon5 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon5, &volume_icon_0);
	
	lv_obj_set_style_img_recolor(icon5, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon5, LV_OPA_COVER, 0);
}

void Remove_Volume_Icon(void)
{
	if (icon5 != NULL) 
	{
        lv_obj_del(icon5);
        icon5 = NULL;
    }
}

void Update_Volume_Icon(void)
{
    Create_Volume_Icon();
}


// ==========================================
// 6. 退出图标 (Exit)
// ==========================================
static lv_obj_t * icon6;
extern const lv_img_dsc_t exit_icon;

void Create_Exit_Icon(void)
{
	if (icon6 != NULL) return;
    if (icon_flex_cont == NULL) return;
    
	icon6 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon6, &exit_icon);
	
	lv_obj_set_style_img_recolor(icon6, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon6, LV_OPA_COVER, 0);
}

void Remove_Exit_Icon(void)
{
	if (icon6 != NULL) 
	{
        lv_obj_del(icon6);
        icon6 = NULL;
    }
}

void Update_Exit_Icon(void)
{
	return;
}

// ==========================================
// 7. 最小化图标 (Minimize)
// ==========================================
static lv_obj_t * icon7;
extern const lv_img_dsc_t minimize_icon;

void Create_Minimize_Icon(void)
{
	if (icon7 != NULL) return;
    if (icon_flex_cont == NULL) return;
    
	icon7 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon7, &minimize_icon);
	
	lv_obj_set_style_img_recolor(icon7, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon7, LV_OPA_COVER, 0);
}

void Remove_Minimize_Icon(void)
{
	if (icon7 != NULL) 
	{
        lv_obj_del(icon7);
        icon7 = NULL;
    }
}

void Update_Minimize_Icon(void)
{
	return;
}

// ==========================================
// 8. 关闭图标 (Close)
// ==========================================
static lv_obj_t * icon8;
extern const lv_img_dsc_t close_icon;

void Create_Close_Icon(void)
{
	if (icon8 != NULL) return;
    if (icon_flex_cont == NULL) return;
    
	icon8 = lv_img_create(icon_flex_cont);
	lv_img_set_src(icon8, &close_icon);
	
	lv_obj_set_style_img_recolor(icon8, lv_color_black(), 0);
	lv_obj_set_style_img_recolor_opa(icon8, LV_OPA_COVER, 0);
}

void Remove_Close_Icon(void)
{
	if (icon8 != NULL) 
	{
        lv_obj_del(icon8);
        icon8 = NULL;
    }
}

void Update_Close_Icon(void)
{
	return;
}
