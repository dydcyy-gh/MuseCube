/*
 * Copyright (c) 2022, sakumisu
 * Copyright (c) 2024, zhihong chen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_msc.h"
#include "usb_scsi.h"
#include "variables.h"
#include "malloc.h"    // 引入动态分配

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbd_msc"
#include "usb_log.h"

#define MSD_OUT_EP_IDX 0
#define MSD_IN_EP_IDX  1

static struct usbd_endpoint mass_ep_data[CONFIG_USBDEV_MAX_BUS][2];

enum Stage {
    MSC_READ_CBW = 0,
    MSC_DATA_OUT = 1,
    MSC_DATA_IN = 2,
    MSC_SEND_CSW = 3,
    MSC_WAIT_CSW = 4,
};

USB_NOCACHE_RAM_SECTION struct usbd_msc_priv {
    enum Stage stage;
    USB_MEM_ALIGNX struct CBW cbw;
    USB_MEM_ALIGNX struct CSW csw;

    USB_MEM_ALIGNX bool readonly;
    bool popup;
    uint8_t sKey; 
    uint8_t ASC;  
    uint8_t ASQ;  
    uint8_t max_lun;
    uint32_t start_sector;
    uint32_t nsectors;
    uint32_t scsi_blk_size[CONFIG_USBDEV_MSC_MAX_LUN];
    uint32_t scsi_blk_nbr[CONFIG_USBDEV_MSC_MAX_LUN];

    // 动态内存指针
    uint8_t *block_buffer;

#if defined(CONFIG_USBDEV_MSC_THREAD)
    usb_osal_mq_t usbd_msc_mq;
    usb_osal_thread_t usbd_msc_thread;
    uint32_t nbytes;
#elif defined(CONFIG_USBDEV_MSC_POLLING)
    uint32_t event;
    uint32_t nbytes;
#endif
} g_usbd_msc[CONFIG_USBDEV_MAX_BUS];


static void usdb_msc_set_max_lun(uint8_t busid)
{
    g_usbd_msc[busid].max_lun = CONFIG_USBDEV_MSC_MAX_LUN - 1u;
}

static void usbd_msc_reset(uint8_t busid)
{
    g_usbd_msc[busid].stage = MSC_READ_CBW;
    g_usbd_msc[busid].readonly = false;
}

static int msc_storage_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    switch (setup->bRequest) {
        case MSC_REQUEST_RESET:
            usbd_msc_reset(busid);
            break;
        case MSC_REQUEST_GET_MAX_LUN:
            (*data)[0] = g_usbd_msc[busid].max_lun;
            *len = 1;
            break;
        default:
            return -1;
    }
    return 0;
}

void msc_storage_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    (void)arg;

    switch (event) {
        case USBD_EVENT_INIT:
#if defined(CONFIG_USBDEV_MSC_THREAD)
            g_usbd_msc[busid].usbd_msc_mq = usb_osal_mq_create(1);
#elif defined(CONFIG_USBDEV_MSC_POLLING)
            g_usbd_msc[busid].event = 0;
#endif
            break;
        case USBD_EVENT_DEINIT:
#if defined(CONFIG_USBDEV_MSC_THREAD)
            if (g_usbd_msc[busid].usbd_msc_mq) {
                usb_osal_mq_delete(g_usbd_msc[busid].usbd_msc_mq);
            }
#endif
            // 【重点】：任务结束时，安全释放动态分配的内存块归还给内存池
            if (g_usbd_msc[busid].block_buffer != NULL) {
                free_bsc(g_usbd_msc[busid].block_buffer);
                g_usbd_msc[busid].block_buffer = NULL;
            }
            break;
        case USBD_EVENT_RESET:
            usbd_msc_reset(busid);
            break;
        case USBD_EVENT_CONFIGURED:
            usbd_ep_start_read(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr, (uint8_t *)&g_usbd_msc[busid].cbw, USB_SIZEOF_MSC_CBW);
            break;
        default:
            break;
    }
}

static void usbd_msc_bot_abort(uint8_t busid)
{
    if ((g_usbd_msc[busid].cbw.bmFlags == 0) && (g_usbd_msc[busid].cbw.dDataLength != 0)) {
        usbd_ep_set_stall(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr);
    }
    usbd_ep_set_stall(busid, mass_ep_data[busid][MSD_IN_EP_IDX].ep_addr);
    usbd_ep_start_read(busid, mass_ep_data[busid][0].ep_addr, (uint8_t *)&g_usbd_msc[busid].cbw, USB_SIZEOF_MSC_CBW);
}

static void usbd_msc_send_csw(uint8_t busid, uint8_t CSW_Status)
{
    g_usbd_msc[busid].csw.dSignature = MSC_CSW_Signature;
    g_usbd_msc[busid].csw.bStatus = CSW_Status;
    g_usbd_msc[busid].stage = MSC_WAIT_CSW;
    usbd_ep_start_write(busid, mass_ep_data[busid][MSD_IN_EP_IDX].ep_addr, (uint8_t *)&g_usbd_msc[busid].csw, sizeof(struct CSW));
}

static void usbd_msc_send_info(uint8_t busid, uint8_t *buffer, uint8_t size)
{
    size = MIN(size, g_usbd_msc[busid].cbw.dDataLength);
    g_usbd_msc[busid].stage = MSC_SEND_CSW;
    usbd_ep_start_write(busid, mass_ep_data[busid][MSD_IN_EP_IDX].ep_addr, buffer, size);
    g_usbd_msc[busid].csw.dDataResidue -= size;
    g_usbd_msc[busid].csw.bStatus = CSW_STATUS_CMD_PASSED;
}

static bool SCSI_processWrite(uint8_t busid, uint32_t nbytes);
static bool SCSI_processRead(uint8_t busid);

static void SCSI_SetSenseData(uint8_t busid, uint32_t KCQ)
{
    g_usbd_msc[busid].sKey = (uint8_t)(KCQ >> 16);
    g_usbd_msc[busid].ASC = (uint8_t)(KCQ >> 8);
    g_usbd_msc[busid].ASQ = (uint8_t)(KCQ);
}

static bool SCSI_testUnitReady(uint8_t busid, uint8_t **data, uint32_t *len)
{
    if (g_usbd_msc[busid].cbw.dDataLength != 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_requestSense(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint8_t data_len = SCSIRESP_FIXEDSENSEDATA_SIZEOF;
    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    if (g_usbd_msc[busid].cbw.CB[4] < SCSIRESP_FIXEDSENSEDATA_SIZEOF) {
        data_len = g_usbd_msc[busid].cbw.CB[4];
    }
    uint8_t request_sense[SCSIRESP_FIXEDSENSEDATA_SIZEOF] = {
        0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SCSIRESP_FIXEDSENSEDATA_SIZEOF - 8,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    request_sense[2] = g_usbd_msc[busid].sKey;
    request_sense[12] = g_usbd_msc[busid].ASC;
    request_sense[13] = g_usbd_msc[busid].ASQ;
    memcpy(*data, (uint8_t *)request_sense, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_inquiry(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint8_t data_len = SCSIRESP_INQUIRY_SIZEOF;
    uint8_t inquiry00[6] = { 0x00, 0x00, 0x00, (0x06 - 4U), 0x00, 0x80 };
    uint8_t inquiry80[8] = { 0x00, 0x80, 0x00, 0x08, 0x20, 0x20, 0x20, 0x20 };
    uint8_t inquiry[SCSIRESP_INQUIRY_SIZEOF] = {
        0x00, 0x80, 0x02, 0x02, (SCSIRESP_INQUIRY_SIZEOF - 5), 0x00, 0x00, 0x00,
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ' 
    };

    memcpy(&inquiry[8], CONFIG_USBDEV_MSC_MANUFACTURER_STRING, strlen(CONFIG_USBDEV_MSC_MANUFACTURER_STRING));
    memcpy(&inquiry[16], CONFIG_USBDEV_MSC_PRODUCT_STRING, strlen(CONFIG_USBDEV_MSC_PRODUCT_STRING));
    memcpy(&inquiry[32], CONFIG_USBDEV_MSC_VERSION_STRING, strlen(CONFIG_USBDEV_MSC_VERSION_STRING));

    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if ((g_usbd_msc[busid].cbw.CB[1] & 0x01U) != 0U) {
        if (g_usbd_msc[busid].cbw.CB[2] == 0U) {       
            data_len = 0x06;
            memcpy(*data, (uint8_t *)inquiry00, data_len);
        } else if (g_usbd_msc[busid].cbw.CB[2] == 0x80U) { 
            data_len = 0x08;
            memcpy(*data, (uint8_t *)inquiry80, data_len);
        } else { 
            SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDFIELDINCBA);
            return false;
        }
    } else {
        if (g_usbd_msc[busid].cbw.CB[4] < SCSIRESP_INQUIRY_SIZEOF) {
            data_len = g_usbd_msc[busid].cbw.CB[4];
        }
        memcpy(*data, (uint8_t *)inquiry, data_len);
    }
    *len = data_len;
    return true;
}

static bool SCSI_startStopUnit(uint8_t busid, uint8_t **data, uint32_t *len)
{
    if (g_usbd_msc[busid].cbw.dDataLength != 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if ((g_usbd_msc[busid].cbw.CB[4] & 0x3U) == 0x1U) 
    {
    } else if ((g_usbd_msc[busid].cbw.CB[4] & 0x3U) == 0x2U)
    {
        g_usbd_msc[busid].popup = true;
        g_usb_function = 0; 
    } else if ((g_usbd_msc[busid].cbw.CB[4] & 0x3U) == 0x3U) 
    {
    } else {
    }

    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_preventAllowMediaRemoval(uint8_t busid, uint8_t **data, uint32_t *len)
{
    if (g_usbd_msc[busid].cbw.dDataLength != 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_modeSense6(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint8_t data_len = 4;
    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    if (g_usbd_msc[busid].cbw.CB[4] < SCSIRESP_MODEPARAMETERHDR6_SIZEOF) {
        data_len = g_usbd_msc[busid].cbw.CB[4];
    }
    uint8_t sense6[SCSIRESP_MODEPARAMETERHDR6_SIZEOF] = { 0x03, 0x00, 0x00, 0x00 };
    if (g_usbd_msc[busid].readonly) sense6[2] = 0x80;
    memcpy(*data, (uint8_t *)sense6, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_modeSense10(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint8_t data_len = 27;
    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    if (g_usbd_msc[busid].cbw.CB[8] < 27) {
        data_len = g_usbd_msc[busid].cbw.CB[8];
    }
    uint8_t sense10[27] = {
        0x00, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
    };
    memcpy(*data, (uint8_t *)sense10, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_readFormatCapacity(uint8_t busid, uint8_t **data, uint32_t *len)
{
    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    uint8_t format_capacity[SCSIRESP_READFORMATCAPACITIES_SIZEOF] = {
        0x00, 0x00, 0x00, 0x08, 
        (uint8_t)((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] >> 24) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] >> 16) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] >> 8) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] >> 0) & 0xff),
        0x02, 0x00,
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 8) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 0) & 0xff),
    };
    memcpy(*data, (uint8_t *)format_capacity, SCSIRESP_READFORMATCAPACITIES_SIZEOF);
    *len = SCSIRESP_READFORMATCAPACITIES_SIZEOF;
    return true;
}

static bool SCSI_readCapacity10(uint8_t busid, uint8_t **data, uint32_t *len)
{
    if (g_usbd_msc[busid].cbw.dDataLength == 0U) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    uint8_t capacity10[SCSIRESP_READCAPACITY10_SIZEOF] = {
        (uint8_t)(((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] - 1) >> 24) & 0xff),
        (uint8_t)(((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] - 1) >> 16) & 0xff),
        (uint8_t)(((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] - 1) >> 8) & 0xff),
        (uint8_t)(((g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN] - 1) >> 0) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 24) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 16) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 8) & 0xff),
        (uint8_t)((g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN] >> 0) & 0xff),
    };
    memcpy(*data, (uint8_t *)capacity10, SCSIRESP_READCAPACITY10_SIZEOF);
    *len = SCSIRESP_READCAPACITY10_SIZEOF;
    return true;
}

static bool SCSI_read10(uint8_t busid, uint8_t **data, uint32_t *len)
{
    (void)data; (void)len;
    if (((g_usbd_msc[busid].cbw.bmFlags & 0x80U) != 0x80U) || (g_usbd_msc[busid].cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    g_usbd_msc[busid].start_sector = GET_BE32(&g_usbd_msc[busid].cbw.CB[2]); 
    g_usbd_msc[busid].nsectors = GET_BE16(&g_usbd_msc[busid].cbw.CB[7]); 

    if ((g_usbd_msc[busid].start_sector + g_usbd_msc[busid].nsectors) > g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN]) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_LBAOUTOFRANGE);
        return false;
    }
    if (g_usbd_msc[busid].cbw.dDataLength != (g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN])) {
        return false;
    }
    g_usbd_msc[busid].stage = MSC_DATA_IN;
#if defined(CONFIG_USBDEV_MSC_THREAD)
    usb_osal_mq_send(g_usbd_msc[busid].usbd_msc_mq, MSC_DATA_IN);
    return true;
#elif defined(CONFIG_USBDEV_MSC_POLLING)
    g_usbd_msc[busid].event = MSC_DATA_IN;
    return true;
#else
    return SCSI_processRead(busid);
#endif
}

static bool SCSI_read12(uint8_t busid, uint8_t **data, uint32_t *len)
{
    (void)data; (void)len;
    if (((g_usbd_msc[busid].cbw.bmFlags & 0x80U) != 0x80U) || (g_usbd_msc[busid].cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    g_usbd_msc[busid].start_sector = GET_BE32(&g_usbd_msc[busid].cbw.CB[2]); 
    g_usbd_msc[busid].nsectors = GET_BE32(&g_usbd_msc[busid].cbw.CB[6]); 

    if ((g_usbd_msc[busid].start_sector + g_usbd_msc[busid].nsectors) > g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN]) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_LBAOUTOFRANGE);
        return false;
    }
    if (g_usbd_msc[busid].cbw.dDataLength != (g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN])) {
        return false;
    }
    g_usbd_msc[busid].stage = MSC_DATA_IN;
#if defined(CONFIG_USBDEV_MSC_THREAD)
    usb_osal_mq_send(g_usbd_msc[busid].usbd_msc_mq, MSC_DATA_IN);
    return true;
#elif defined(CONFIG_USBDEV_MSC_POLLING)
    g_usbd_msc[busid].event = MSC_DATA_IN;
    return true;
#else
    return SCSI_processRead(busid);
#endif
}

static bool SCSI_write10(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint32_t data_len = 0;
    (void)data; (void)len;
    if (((g_usbd_msc[busid].cbw.bmFlags & 0x80U) != 0x00U) || (g_usbd_msc[busid].cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    g_usbd_msc[busid].start_sector = GET_BE32(&g_usbd_msc[busid].cbw.CB[2]); 
    g_usbd_msc[busid].nsectors = GET_BE16(&g_usbd_msc[busid].cbw.CB[7]); 

    data_len = g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN];
    if ((g_usbd_msc[busid].start_sector + g_usbd_msc[busid].nsectors) > g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN]) {
        return false;
    }
    if (g_usbd_msc[busid].cbw.dDataLength != data_len) {
        return false;
    }
    g_usbd_msc[busid].stage = MSC_DATA_OUT;
    data_len = MIN(data_len, CONFIG_USBDEV_MSC_MAX_BUFSIZE);
    usbd_ep_start_read(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr, g_usbd_msc[busid].block_buffer, data_len);
    return true;
}

static bool SCSI_write12(uint8_t busid, uint8_t **data, uint32_t *len)
{
    uint32_t data_len = 0;
    (void)data; (void)len;
    if (((g_usbd_msc[busid].cbw.bmFlags & 0x80U) != 0x00U) || (g_usbd_msc[busid].cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    g_usbd_msc[busid].start_sector = GET_BE32(&g_usbd_msc[busid].cbw.CB[2]); 
    g_usbd_msc[busid].nsectors = GET_BE32(&g_usbd_msc[busid].cbw.CB[6]); 

    data_len = g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN];
    if ((g_usbd_msc[busid].start_sector + g_usbd_msc[busid].nsectors) > g_usbd_msc[busid].scsi_blk_nbr[g_usbd_msc[busid].cbw.bLUN]) {
        return false;
    }
    if (g_usbd_msc[busid].cbw.dDataLength != data_len) {
        return false;
    }
    g_usbd_msc[busid].stage = MSC_DATA_OUT;
    data_len = MIN(data_len, CONFIG_USBDEV_MSC_MAX_BUFSIZE);
    usbd_ep_start_read(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr, g_usbd_msc[busid].block_buffer, data_len);
    return true;
}

static bool SCSI_processRead(uint8_t busid)
{
    uint32_t transfer_len;
    transfer_len = MIN(g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN], CONFIG_USBDEV_MSC_MAX_BUFSIZE);

    if (usbd_msc_sector_read(busid, g_usbd_msc[busid].cbw.bLUN, g_usbd_msc[busid].start_sector, g_usbd_msc[busid].block_buffer, transfer_len) != 0) {
        SCSI_SetSenseData(busid, SCSI_KCQHE_UREINRESERVEDAREA);
        return false;
    }

    g_usbd_msc[busid].start_sector += (transfer_len / g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN]);
    g_usbd_msc[busid].nsectors -= (transfer_len / g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN]);
    g_usbd_msc[busid].csw.dDataResidue -= transfer_len;

    if (g_usbd_msc[busid].nsectors == 0) {
        g_usbd_msc[busid].stage = MSC_SEND_CSW;
    }

    usbd_ep_start_write(busid, mass_ep_data[busid][MSD_IN_EP_IDX].ep_addr, g_usbd_msc[busid].block_buffer, transfer_len);
    return true;
}

static bool SCSI_processWrite(uint8_t busid, uint32_t nbytes)
{
    uint32_t data_len = 0;
    if (usbd_msc_sector_write(busid, g_usbd_msc[busid].cbw.bLUN, g_usbd_msc[busid].start_sector, g_usbd_msc[busid].block_buffer, nbytes) != 0) {
        SCSI_SetSenseData(busid, SCSI_KCQHE_WRITEFAULT);
        return false;
    }

    g_usbd_msc[busid].start_sector += (nbytes / g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN]);
    g_usbd_msc[busid].nsectors -= (nbytes / g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN]);
    g_usbd_msc[busid].csw.dDataResidue -= nbytes;

    if (g_usbd_msc[busid].nsectors == 0) {
        usbd_msc_send_csw(busid, CSW_STATUS_CMD_PASSED);
    } else {
        data_len = MIN(g_usbd_msc[busid].nsectors * g_usbd_msc[busid].scsi_blk_size[g_usbd_msc[busid].cbw.bLUN], CONFIG_USBDEV_MSC_MAX_BUFSIZE);
        usbd_ep_start_read(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr, g_usbd_msc[busid].block_buffer, data_len);
    }
    return true;
}

static bool SCSI_CBWDecode(uint8_t busid, uint32_t nbytes)
{
    uint8_t *buf2send = g_usbd_msc[busid].block_buffer;
    uint32_t len2send = 0;
    bool ret = false;

    if (nbytes != sizeof(struct CBW)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    g_usbd_msc[busid].csw.dTag = g_usbd_msc[busid].cbw.dTag;
    g_usbd_msc[busid].csw.dDataResidue = g_usbd_msc[busid].cbw.dDataLength;

    if ((g_usbd_msc[busid].cbw.dSignature != MSC_CBW_Signature) || (g_usbd_msc[busid].cbw.bCBLength < 1) || (g_usbd_msc[busid].cbw.bCBLength > 16)) {
        SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    } else {
        switch (g_usbd_msc[busid].cbw.CB[0]) {
            case SCSI_CMD_TESTUNITREADY:        ret = SCSI_testUnitReady(busid, &buf2send, &len2send); break;
            case SCSI_CMD_REQUESTSENSE:         ret = SCSI_requestSense(busid, &buf2send, &len2send); break;
            case SCSI_CMD_INQUIRY:              ret = SCSI_inquiry(busid, &buf2send, &len2send); break;
            case SCSI_CMD_STARTSTOPUNIT:        ret = SCSI_startStopUnit(busid, &buf2send, &len2send); break;
            case SCSI_CMD_PREVENTMEDIAREMOVAL:  ret = SCSI_preventAllowMediaRemoval(busid, &buf2send, &len2send); break;
            case SCSI_CMD_MODESENSE6:           ret = SCSI_modeSense6(busid, &buf2send, &len2send); break;
            case SCSI_CMD_MODESENSE10:          ret = SCSI_modeSense10(busid, &buf2send, &len2send); break;
            case SCSI_CMD_READFORMATCAPACITIES: ret = SCSI_readFormatCapacity(busid, &buf2send, &len2send); break;
            case SCSI_CMD_READCAPACITY10:       ret = SCSI_readCapacity10(busid, &buf2send, &len2send); break;
            case SCSI_CMD_READ10:               ret = SCSI_read10(busid, NULL, 0); break;
            case SCSI_CMD_READ12:               ret = SCSI_read12(busid, NULL, 0); break;
            case SCSI_CMD_WRITE10:              ret = SCSI_write10(busid, NULL, 0); break;
            case SCSI_CMD_WRITE12:              ret = SCSI_write12(busid, NULL, 0); break;
            case SCSI_CMD_VERIFY10:             ret = false; break;
            case SCSI_CMD_SYNCHCACHE10:         ret = true; break;
            default:
                SCSI_SetSenseData(busid, SCSI_KCQIR_INVALIDCOMMAND);
                ret = false;
                break;
        }
    }
    if (ret) {
        if (g_usbd_msc[busid].stage == MSC_READ_CBW) {
            if (len2send) usbd_msc_send_info(busid, buf2send, len2send);
            else          usbd_msc_send_csw(busid, CSW_STATUS_CMD_PASSED);
        }
    }
    return ret;
}

void mass_storage_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    switch (g_usbd_msc[busid].stage) {
        case MSC_READ_CBW:
            if (SCSI_CBWDecode(busid, nbytes) == false) usbd_msc_bot_abort(busid);
            break;
        case MSC_DATA_OUT:
            switch (g_usbd_msc[busid].cbw.CB[0]) {
                case SCSI_CMD_WRITE10:
                case SCSI_CMD_WRITE12:
#if defined(CONFIG_USBDEV_MSC_THREAD)
                    g_usbd_msc[busid].nbytes = nbytes;
                    usb_osal_mq_send(g_usbd_msc[busid].usbd_msc_mq, MSC_DATA_OUT);
#elif defined(CONFIG_USBDEV_MSC_POLLING)
                    g_usbd_msc[busid].nbytes = nbytes;
                    g_usbd_msc[busid].event = MSC_DATA_OUT;
#else
                    if (SCSI_processWrite(busid, nbytes) == false) usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
#endif
                    break;
                default: break;
            }
            break;
        default: break;
    }
}

void mass_storage_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep; (void)nbytes;
    switch (g_usbd_msc[busid].stage) {
        case MSC_DATA_IN:
            switch (g_usbd_msc[busid].cbw.CB[0]) {
                case SCSI_CMD_READ10:
                case SCSI_CMD_READ12:
#if defined(CONFIG_USBDEV_MSC_THREAD)
                    usb_osal_mq_send(g_usbd_msc[busid].usbd_msc_mq, MSC_DATA_IN);
#elif defined(CONFIG_USBDEV_MSC_POLLING)
                    g_usbd_msc[busid].event = MSC_DATA_IN;
#else
                    if (SCSI_processRead(busid) == false) {
                        usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
                        return;
                    }
#endif
                    break;
                default: break;
            }
            break;
        case MSC_SEND_CSW:
            usbd_msc_send_csw(busid, CSW_STATUS_CMD_PASSED);
            break;
        case MSC_WAIT_CSW:
            g_usbd_msc[busid].stage = MSC_READ_CBW;
            usbd_ep_start_read(busid, mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr, (uint8_t *)&g_usbd_msc[busid].cbw, USB_SIZEOF_MSC_CBW);
            break;
        default: break;
    }
}

#if defined(CONFIG_USBDEV_MSC_THREAD)
void my_usbd_msc_thread(void)
{
    uintptr_t event;
    uint8_t busid = 0;
	
    if (usb_osal_mq_recv(g_usbd_msc[busid].usbd_msc_mq, (uintptr_t *)&event, 50) < 0) return; 

	if (event == MSC_DATA_OUT) {
		if (SCSI_processWrite(busid, g_usbd_msc[busid].nbytes) == false) usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
	} else if (event == MSC_DATA_IN) {
		if (SCSI_processRead(busid) == false) usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
	}
}
#elif defined(CONFIG_USBDEV_MSC_POLLING)
void usbd_msc_polling(uint8_t busid)
{
    uint8_t event = g_usbd_msc[busid].event;
    if (event != 0) {
        g_usbd_msc[busid].event = 0;
        if (event == MSC_DATA_OUT) {
            if (SCSI_processWrite(busid, g_usbd_msc[busid].nbytes) == false) usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
        } else if (event == MSC_DATA_IN) {
            if (SCSI_processRead(busid) == false) usbd_msc_send_csw(busid, CSW_STATUS_CMD_FAILED); 
        }
    }
}
#endif

struct usbd_interface *usbd_msc_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t out_ep, const uint8_t in_ep)
{
    intf->class_interface_handler = msc_storage_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = msc_storage_notify_handler;

    mass_ep_data[busid][MSD_OUT_EP_IDX].ep_addr = out_ep;
    mass_ep_data[busid][MSD_OUT_EP_IDX].ep_cb = mass_storage_bulk_out;
    mass_ep_data[busid][MSD_IN_EP_IDX].ep_addr = in_ep;
    mass_ep_data[busid][MSD_IN_EP_IDX].ep_cb = mass_storage_bulk_in;

    usbd_add_endpoint(busid, &mass_ep_data[busid][MSD_OUT_EP_IDX]);
    usbd_add_endpoint(busid, &mass_ep_data[busid][MSD_IN_EP_IDX]);

    // 【关键修复点】：由于 g_usbd_msc 在被放置到特定 NO_CACHE 内存段时，上电初始状态是随机垃圾数据！
    // 删除了上次错误加入的 `if (g_usbd_msc[busid].block_buffer != NULL) free(...)`。
    // 这会导致把垃圾地址传给内存池并引发 HardFault 硬件死机！

    memset((uint8_t *)&g_usbd_msc[busid], 0, sizeof(struct usbd_msc_priv));

    // 【新增】：设备开启时，动态申请大块内存
    g_usbd_msc[busid].block_buffer = (uint8_t *)malloc_bsc(CONFIG_USBDEV_MSC_MAX_BUFSIZE);

    usdb_msc_set_max_lun(busid);
    for (uint8_t i = 0u; i <= g_usbd_msc[busid].max_lun; i++) {
        usbd_msc_get_cap(busid, i, &g_usbd_msc[busid].scsi_blk_nbr[i], &g_usbd_msc[busid].scsi_blk_size[i]);
    }
    return intf;
}

void usbd_msc_set_readonly(uint8_t busid, bool readonly)
{
    g_usbd_msc[busid].readonly = readonly;
}

bool usbd_msc_get_popup(uint8_t busid)
{
    return g_usbd_msc[busid].popup;
}

__WEAK void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size) { }
__WEAK int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length) { return 0; }
__WEAK int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length) { return 0; }
