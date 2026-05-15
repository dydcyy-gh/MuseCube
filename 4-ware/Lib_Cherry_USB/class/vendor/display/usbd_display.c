/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbd_core.h"
#include "usbd_display.h"
#include "main.h"

struct usbd_display_priv {
    struct usb_mempool pool;
    struct usbd_endpoint out_ep;
    struct usbd_endpoint in_ep;
    struct usbd_display_frame *current_frame;
    uint8_t busid;
    bool rx_paused;
} g_usbd_display;

int usbd_display_frame_create(struct usbd_display_frame *frame, uint32_t count)
{
    return usb_mempool_create(&g_usbd_display.pool, frame, sizeof(struct usbd_display_frame), count);
}

struct usbd_display_frame *usbd_display_frame_alloc(void)
{
    return (struct usbd_display_frame *)usb_mempool_alloc(&g_usbd_display.pool);
}

int usbd_display_frame_free(struct usbd_display_frame *frame)
{
    return usb_mempool_free(&g_usbd_display.pool, (uintptr_t *)frame);
}

int usbd_display_frame_send(struct usbd_display_frame *frame)
{
    return usb_mempool_send(&g_usbd_display.pool, (uintptr_t *)frame);
}

int usbd_display_frame_recv(struct usbd_display_frame **frame, uint32_t timeout)
{
    return usb_mempool_recv(&g_usbd_display.pool, (uintptr_t **)frame, timeout);
}

volatile uint32_t usb_display_buf_offset;
volatile bool next_is_sof = true;

static void display_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
		case USBD_EVENT_DISCONNECTED:
        case USBD_EVENT_SUSPEND:
            Taskmanager_Ctrl(1, 5, 1); // 恢复 LVGL 任务
            break;
        case USBD_EVENT_CONFIGURED:
			Taskmanager_Ctrl(1, 3, 1); // 挂起 LVGL 任务
            g_usbd_display.busid = busid;
            usb_display_buf_offset = 0;
            g_usbd_display.rx_paused = false;
            next_is_sof = true;
            g_usbd_display.current_frame = NULL;
            usb_mempool_reset(&g_usbd_display.pool);
            
            g_usbd_display.current_frame = usbd_display_frame_alloc();
            if (g_usbd_display.current_frame) {
                usbd_ep_start_read(busid, g_usbd_display.out_ep.ep_addr, g_usbd_display.current_frame->frame_buf, g_usbd_display.current_frame->frame_bufsize);
            } else {
                g_usbd_display.rx_paused = true;
            }
            break;
        default:
            break;
    }
}

void usbd_display_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    struct usbd_display_frame *frame = g_usbd_display.current_frame;
    
    if (frame == NULL) {
        return; 
    }

    usb_display_buf_offset += nbytes;

    bool is_short_packet = ((nbytes % usbd_get_ep_mps(0, ep)) != 0) || (nbytes == 0);
    bool buffer_full = (usb_display_buf_offset >= frame->frame_bufsize);

    if (is_short_packet || buffer_full) {
        frame->frame_size = usb_display_buf_offset;
        frame->is_sof = next_is_sof;
        frame->is_eof = is_short_packet;
        
        next_is_sof = is_short_packet; 

        usbd_display_frame_send(frame);

        g_usbd_display.current_frame = usbd_display_frame_alloc();
        if (g_usbd_display.current_frame) {
            usb_display_buf_offset = 0;
            usbd_ep_start_read(busid, ep, g_usbd_display.current_frame->frame_buf, g_usbd_display.current_frame->frame_bufsize);
        } else {
            g_usbd_display.rx_paused = true;
        }
    } else {
        usbd_ep_start_read(busid, ep, &frame->frame_buf[usb_display_buf_offset], frame->frame_bufsize - usb_display_buf_offset);
    }
}

void usbd_display_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
}

struct usbd_interface *usbd_display_init_intf(struct usbd_interface *intf,
                                              const uint8_t out_ep,
                                              const uint8_t in_ep,
                                              struct usbd_display_frame *frame,
                                              uint32_t count)
{
    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = display_notify_handler;

    g_usbd_display.out_ep.ep_addr = out_ep;
    g_usbd_display.out_ep.ep_cb = usbd_display_bulk_out;
    g_usbd_display.in_ep.ep_addr = in_ep;
    g_usbd_display.in_ep.ep_cb = usbd_display_bulk_in;
    usbd_add_endpoint(0, &g_usbd_display.out_ep);
    usbd_add_endpoint(0, &g_usbd_display.in_ep);

    usbd_display_frame_create(frame, count);
    return intf;
}

int usbd_display_dequeue(struct usbd_display_frame **frame, uint32_t timeout)
{
    return usbd_display_frame_recv(frame, timeout);
}

int usbd_display_enqueue(struct usbd_display_frame *frame)
{
    int ret = usbd_display_frame_free(frame);

    // 关键流控：如果USB因为缓存不足暂停了读取，现在屏幕刷完了释放了缓存，我们恢复USB接收
    if (g_usbd_display.rx_paused) 
	{
        g_usbd_display.current_frame = usbd_display_frame_alloc();
        if (g_usbd_display.current_frame) 
		{
            g_usbd_display.rx_paused = false;
            usb_display_buf_offset = 0;
            usbd_ep_start_read(g_usbd_display.busid, g_usbd_display.out_ep.ep_addr, 
                               g_usbd_display.current_frame->frame_buf, 
                               g_usbd_display.current_frame->frame_bufsize);
        }
    }

    return ret;
}
