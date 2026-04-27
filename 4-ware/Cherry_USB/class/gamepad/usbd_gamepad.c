/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usbd_gamepad.h"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t gamepad_report_buffer[64];

static int xinput_vendor_class_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    struct xinput_in_report xinput_report;

    memset(&xinput_report, 0, sizeof(xinput_report));
    xinput_report.report_size = 20;

    memcpy(*data, &xinput_report, sizeof(xinput_report));
    *len = sizeof(xinput_report);
    return 0;
}

int usbd_gamepad_xinput_send_report(uint8_t ep, struct usb_gamepad_report *report)
{
    struct xinput_in_report *xinput_report;

    xinput_report = (struct xinput_in_report *)gamepad_report_buffer;
    memset(xinput_report, 0, sizeof(*xinput_report));
    xinput_report->report_size = 20;

    if (report->buttons & USB_GAMEPAD_BUTTON_DU) xinput_report->buttons |= XINPUT_BUTTON_MASK_UP;
    if (report->buttons & USB_GAMEPAD_BUTTON_DD) xinput_report->buttons |= XINPUT_BUTTON_MASK_DOWN;
    if (report->buttons & USB_GAMEPAD_BUTTON_DL) xinput_report->buttons |= XINPUT_BUTTON_MASK_LEFT;
    if (report->buttons & USB_GAMEPAD_BUTTON_DR) xinput_report->buttons |= XINPUT_BUTTON_MASK_RIGHT;
    if (report->buttons & USB_GAMEPAD_BUTTON_S2) xinput_report->buttons |= XINPUT_BUTTON_MASK_START;
    if (report->buttons & USB_GAMEPAD_BUTTON_S1) xinput_report->buttons |= XINPUT_BUTTON_MASK_BACK;
    if (report->buttons & USB_GAMEPAD_BUTTON_L3) xinput_report->buttons |= XINPUT_BUTTON_MASK_L3;
    if (report->buttons & USB_GAMEPAD_BUTTON_R3) xinput_report->buttons |= XINPUT_BUTTON_MASK_R3;
    if (report->buttons & USB_GAMEPAD_BUTTON_L1) xinput_report->buttons |= XINPUT_BUTTON_MASK_LB;
    if (report->buttons & USB_GAMEPAD_BUTTON_R1) xinput_report->buttons |= XINPUT_BUTTON_MASK_RB;
    if (report->buttons & USB_GAMEPAD_BUTTON_A1) xinput_report->buttons |= XINPUT_BUTTON_MASK_GUIDE;
    if (report->buttons & USB_GAMEPAD_BUTTON_B1) xinput_report->buttons |= XINPUT_BUTTON_MASK_A;
    if (report->buttons & USB_GAMEPAD_BUTTON_B2) xinput_report->buttons |= XINPUT_BUTTON_MASK_B;
    if (report->buttons & USB_GAMEPAD_BUTTON_B3) xinput_report->buttons |= XINPUT_BUTTON_MASK_X;
    if (report->buttons & USB_GAMEPAD_BUTTON_B4) xinput_report->buttons |= XINPUT_BUTTON_MASK_Y;

    xinput_report->lt = report->lt;
    xinput_report->rt = report->rt;
    if (xinput_report->lt == 0 && (report->buttons & USB_GAMEPAD_BUTTON_L2)) xinput_report->lt = 0xFF;
    if (xinput_report->rt == 0 && (report->buttons & USB_GAMEPAD_BUTTON_R2)) xinput_report->rt = 0xFF;

    xinput_report->lx = (int16_t)report->lx * 256;
    xinput_report->ly = (int16_t)report->ly * 256;
    xinput_report->rx = (int16_t)report->rx * 256;
    xinput_report->ry = (int16_t)report->ry * 256;
	
    return usbd_ep_start_write(0, ep, gamepad_report_buffer, sizeof(struct xinput_in_report));
}

struct usbd_interface *usbd_gamepad_xinput_init_intf(struct usbd_interface *intf)
{
    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = xinput_vendor_class_request_handler;
    intf->notify_handler = NULL;

    return intf;
}
