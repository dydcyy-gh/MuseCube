#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "systick_conf.h"
#include "usbd_cdc_conf.h"
#include "usbd_msc_conf.h"
#include "usbd_uac1_conf.h"
#include "usbd_uac2_conf.h"
#include "usbd_display_conf.h"
#include "usbd_hid_conf.h"
#include "debug.h"
#include "variables.h"
#include "defines.h"
#include "video.h"
#include "usbh_msc_conf.h"
#include "usbh_hid_conf.h"
#include "usbh_serial_conf.h"

//g_usb_status完全受adc自动控制
//g_usb_function通过:usb页面选择,g_usb_status拔出监测

static uint8_t last_usb_status = 0;
static uint8_t last_usb_function = 0;

void USB_Task( void * pvParameters )
{
	while(1)
	{
		// usb拔出监测保持原样
		if(last_usb_status != g_usb_status) 
		{
			if(last_usb_status > 2 && g_usb_status < 3)
			{
				if(g_usb_function) g_usb_function = USB_NONE;
			}
		}
		last_usb_status = g_usb_status;
		
		// cherryusb初始化/反初始化
		if(g_usb_function != last_usb_function)
		{
			// 1.空闲-初始化-运行
			if(g_usb_function && !last_usb_function)
			{
				switch(g_usb_function)
				{
					case USBD_CDC:  usbd_cdc_init(0,USB_OTG_HS_PERIPH_BASE);     	break;
					case USBD_MSC:  usbd_msc_init(0,USB_OTG_HS_PERIPH_BASE);     	break;
					case USBD_UAC1: audio_v1_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_UAC2: audio_v2_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_DISP: usb_display_init(0,USB_OTG_HS_PERIPH_BASE); 	break;
					case USBD_GMPD: usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_MOU:  usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_KBD:  usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBH_MSC:  usbh_msc_init(0,USB_OTG_HS_PERIPH_BASE); 		break;
					case USBH_HID:  usbh_hid_init(0,USB_OTG_HS_PERIPH_BASE); 		break;
					case USBH_CDC:  usbh_serial_init(0,USB_OTG_HS_PERIPH_BASE); 	break;
					default: break;
				}
			}
			// 2.运行-反初始化-空闲
			else if(!g_usb_function && last_usb_function)
			{
				switch(last_usb_function)
				{
					case USBD_CDC:  usbd_cdc_deinit();						    break;
					case USBD_MSC:  usbd_msc_deinit();							break;
					case USBD_UAC1: usbd_uac1_deinit();                         break;
					case USBD_UAC2: usbd_uac2_deinit();                         break;
					case USBD_DISP: usb_display_deinit(); 						break;
					case USBD_GMPD: usbd_hid_deinit();						    break;
					case USBD_MOU:  usbd_hid_deinit();	                        break;
					case USBD_KBD:  usbd_hid_deinit();                          break;
					case USBH_MSC:  usbh_msc_deinit();                          break;
					case USBH_HID:  usbh_hid_deinit();							break;
					case USBH_CDC:  usbh_serial_deinit(); 						break;
					default: break;
				}
			}
			// 3.运行-反初始化-初始化-运行
			else
			{
				switch(last_usb_function)
				{
					case USBD_CDC:  usbd_cdc_deinit();							break;
					case USBD_MSC:  usbd_msc_deinit();							break;
					case USBD_UAC1: usbd_uac1_deinit();                         break;
					case USBD_UAC2: usbd_uac2_deinit();                         break;
					case USBD_DISP: usb_display_deinit(); 						break;
					case USBD_GMPD: usbd_hid_deinit();							break;
					case USBD_MOU:  usbd_hid_deinit();	                        break;
					case USBD_KBD:  usbd_hid_deinit();                          break;
					case USBH_MSC:  usbh_msc_deinit();                          break;
					case USBH_HID:  usbh_hid_deinit();							break;
					case USBH_CDC:  usbh_serial_deinit(); 						break;
					default: break;
				}
				Delay_ms(300);
				switch(g_usb_function)
				{
					case USBD_CDC:  usbd_cdc_init(0,USB_OTG_HS_PERIPH_BASE);     	break;
					case USBD_MSC:  usbd_msc_init(0,USB_OTG_HS_PERIPH_BASE);     	break;
					case USBD_UAC1: audio_v1_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_UAC2: audio_v2_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_DISP: usb_display_init(0,USB_OTG_HS_PERIPH_BASE); 	break;
					case USBD_GMPD: usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_MOU:  usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBD_KBD:  usbd_hid_init(0,USB_OTG_HS_PERIPH_BASE);    	break;
					case USBH_HID:  usbh_hid_init(0,USB_OTG_HS_PERIPH_BASE); 		break;
					case USBH_CDC:  usbh_serial_init(0,USB_OTG_HS_PERIPH_BASE); 	break;
					default: break;
				}
			}
			last_usb_function = g_usb_function;
		}
		
		// cherryusb功能运行保持原样 ...
		switch(g_usb_function)
		{
			case USBD_CDC:  Delay_ms(20);         break;
			case USBD_MSC:  usbd_msc_task();      break;
			case USBD_UAC1: uac1_play_song_task();break;
			case USBD_UAC2: uac2_play_song_task();break;
			case USBD_DISP: usb_display_task();   break;
			case USBD_GMPD: usbd_hid_task();      break;
			case USBD_MOU:  usbd_hid_task();      break;
			case USBD_KBD:  usbd_hid_task();      break;
			case USBH_MSC:  usbh_msc_task();      break;
			case USBH_HID:  usbh_hid_task();	  break;
			case USBH_CDC:  usbh_serial_task();   break;
			default: Delay_ms(20);                break;
		}
	}
}
