/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_audio.h"
#include "i2s.h"
#include "es9018k2m.h"
#include "malloc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"

extern SemaphoreHandle_t xI2SSemaphore;// I2S 传输完成标志

#define USING_FEEDBACK 1

#define USBD_VID           0xffff
#define USBD_PID           0xffff
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#ifdef CONFIG_USB_HS
#define EP_INTERVAL               0x04
#define FEEDBACK_ENDP_PACKET_SIZE 0x04
#else
#define EP_INTERVAL               0x01
#define FEEDBACK_ENDP_PACKET_SIZE 0x03
#endif

#define AUDIO_OUT_EP          0x01
#define AUDIO_OUT_FEEDBACK_EP 0x82

#define AUDIO_OUT_CLOCK_ID 0x01
#define AUDIO_OUT_FU_ID    0x03

#define AUDIO_OUT_MAX_FREQ 50000
#define HALF_WORD_BYTES    2  //2 half word (one channel)
#define SAMPLE_BITS        16 //16 bit per channel

#define BMCONTROL (AUDIO_V2_CONTROL_MUTE)

#define OUT_CHANNEL_NUM 2

#if OUT_CHANNEL_NUM == 1
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x00000000
#elif OUT_CHANNEL_NUM == 2
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x00000003
#elif OUT_CHANNEL_NUM == 3
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x00000007
#elif OUT_CHANNEL_NUM == 4
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x0000000f
#elif OUT_CHANNEL_NUM == 5
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x0000001f
#elif OUT_CHANNEL_NUM == 6
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x0000003F
#elif OUT_CHANNEL_NUM == 7
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x0000007f
#elif OUT_CHANNEL_NUM == 8
#define OUTPUT_CTRL      DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define OUTPUT_CH_ENABLE 0x000000ff
#endif

#define AUDIO_OUT_PACKET ((uint32_t)((AUDIO_OUT_MAX_FREQ * HALF_WORD_BYTES * OUT_CHANNEL_NUM) / 1000))

#if USING_FEEDBACK == 0
#define USB_CONFIG_SIZE (9 +                                                     \
                         AUDIO_V2_AC_DESCRIPTOR_LEN +                            \
                         AUDIO_V2_SIZEOF_AC_CLOCK_SOURCE_DESC +                  \
                         AUDIO_V2_SIZEOF_AC_INPUT_TERMINAL_DESC +                \
                         AUDIO_V2_SIZEOF_AC_FEATURE_UNIT_DESC(OUT_CHANNEL_NUM) + \
                         AUDIO_V2_SIZEOF_AC_OUTPUT_TERMINAL_DESC +               \
                         AUDIO_V2_AS_DESCRIPTOR_LEN)
#else
#define USB_CONFIG_SIZE (9 +                                                     \
                         AUDIO_V2_AC_DESCRIPTOR_LEN +                            \
                         AUDIO_V2_SIZEOF_AC_CLOCK_SOURCE_DESC +                  \
                         AUDIO_V2_SIZEOF_AC_INPUT_TERMINAL_DESC +                \
                         AUDIO_V2_SIZEOF_AC_FEATURE_UNIT_DESC(OUT_CHANNEL_NUM) + \
                         AUDIO_V2_SIZEOF_AC_OUTPUT_TERMINAL_DESC +               \
                         AUDIO_V2_AS_FEEDBACK_DESCRIPTOR_LEN)
#endif

#define AUDIO_AC_SIZ (AUDIO_V2_SIZEOF_AC_HEADER_DESC +                        \
                      AUDIO_V2_SIZEOF_AC_CLOCK_SOURCE_DESC +                  \
                      AUDIO_V2_SIZEOF_AC_INPUT_TERMINAL_DESC +                \
                      AUDIO_V2_SIZEOF_AC_FEATURE_UNIT_DESC(OUT_CHANNEL_NUM) + \
                      AUDIO_V2_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0001, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    AUDIO_V2_AC_DESCRIPTOR_INIT(0x00, 0x02, AUDIO_AC_SIZ, AUDIO_CATEGORY_SPEAKER, 0x00, 0x00),
    AUDIO_V2_AC_CLOCK_SOURCE_DESCRIPTOR_INIT(AUDIO_OUT_CLOCK_ID, 0x03, 0x03),
    AUDIO_V2_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(0x02, AUDIO_TERMINAL_STREAMING, 0x01, OUT_CHANNEL_NUM, OUTPUT_CH_ENABLE, 0x0000),
    AUDIO_V2_AC_FEATURE_UNIT_DESCRIPTOR_INIT(AUDIO_OUT_FU_ID, 0x02, OUTPUT_CTRL),
    AUDIO_V2_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(0x04, AUDIO_OUTTERM_SPEAKER, 0x03, 0x01, 0x0000),
#if USING_FEEDBACK == 0
    AUDIO_V2_AS_DESCRIPTOR_INIT(0x01, 0x02, OUT_CHANNEL_NUM, OUTPUT_CH_ENABLE, HALF_WORD_BYTES, SAMPLE_BITS, AUDIO_OUT_EP, 0x09, AUDIO_OUT_PACKET, EP_INTERVAL),
#else
    AUDIO_V2_AS_FEEDBACK_DESCRIPTOR_INIT(0x01, 0x02, OUT_CHANNEL_NUM, OUTPUT_CH_ENABLE, HALF_WORD_BYTES, SAMPLE_BITS, AUDIO_OUT_EP, AUDIO_OUT_PACKET, EP_INTERVAL, AUDIO_OUT_FEEDBACK_EP),
#endif
};

static const uint8_t device_quality_descriptor[] = {
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB UAC DEMO",         /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor audio_v2_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};


static const uint8_t default_sampling_freq_table[] = {
    AUDIO_SAMPLE_FREQ_NUM(1),

	AUDIO_SAMPLE_FREQ_4B(48000),
    AUDIO_SAMPLE_FREQ_4B(48000),
    AUDIO_SAMPLE_FREQ_4B(0x00),
};

USB_MEM_ALIGNX uint8_t s_speakerv2_feedback_buffer[4];

volatile static bool rx_flag = 0;
volatile static uint32_t s_speaker_sample_rate;

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

#define AUDIO_BUF_NUM 20

USB_MEM_ALIGNX uint8_t *uac2_audio_ring_buf;
USB_MEM_ALIGNX uint8_t *uac2_dma_buf0;
USB_MEM_ALIGNX uint8_t *uac2_dma_buf1;
USB_MEM_ALIGNX uint8_t *uac2_usb_buf ;

volatile static uint32_t ring_buf_wr = 0;  // 环形缓冲区写指针
volatile static uint32_t ring_buf_rd = 0;  // 环形缓冲区读指针
volatile static bool dma_started = false;  // DMA是否已启动

volatile static uint32_t free_read_data = 0;
volatile static uint32_t free_write_data = 0;

void uac2_play_song_task(void) 
{
	if (xSemaphoreTake(xI2SSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
        return; 
    }
	
	// 计算环形缓冲区中可用数据量
    if (ring_buf_wr >= ring_buf_rd)
        free_read_data = ring_buf_wr - ring_buf_rd;
    else
        free_read_data = (AUDIO_BUF_NUM * AUDIO_OUT_PACKET - ring_buf_rd) + ring_buf_wr;
	
	if (free_read_data > AUDIO_OUT_PACKET) 
	{
		if(I2SdmaBuff) 
		{
            if (ring_buf_rd + AUDIO_OUT_PACKET <= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) 
			{
                memcpy(uac2_dma_buf1, &uac2_audio_ring_buf[ring_buf_rd], AUDIO_OUT_PACKET);
                ring_buf_rd += AUDIO_OUT_PACKET;
                if (ring_buf_rd >= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) {
                    ring_buf_rd = 0;
                }
            } 
			else 
			{
                // 需要回绕拷贝
                uint32_t first_part = AUDIO_BUF_NUM * AUDIO_OUT_PACKET - ring_buf_rd;
                memcpy(uac2_dma_buf1, &uac2_audio_ring_buf[ring_buf_rd], first_part);
                memcpy(&uac2_dma_buf1[first_part], uac2_audio_ring_buf, AUDIO_OUT_PACKET - first_part);
                ring_buf_rd = AUDIO_OUT_PACKET - first_part;
            }
        } 
		else 
		{
            if (ring_buf_rd + AUDIO_OUT_PACKET <= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) 
			{
                memcpy(uac2_dma_buf0, &uac2_audio_ring_buf[ring_buf_rd], AUDIO_OUT_PACKET);
                ring_buf_rd += AUDIO_OUT_PACKET;
                if (ring_buf_rd >= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) {
                    ring_buf_rd = 0;
                }
            } 
			else 
			{
                // 需要回绕拷贝
                uint32_t first_part = AUDIO_BUF_NUM * AUDIO_OUT_PACKET - ring_buf_rd;
                memcpy(uac2_dma_buf0, &uac2_audio_ring_buf[ring_buf_rd], first_part);
                memcpy(&uac2_dma_buf0[first_part], uac2_audio_ring_buf, AUDIO_OUT_PACKET - first_part);
                ring_buf_rd = AUDIO_OUT_PACKET - first_part;
            }
        }
    }
	else 
	{
        if(I2SdmaBuff)
            memset(uac2_dma_buf1, 0, AUDIO_OUT_PACKET);
        else
            memset(uac2_dma_buf0, 0, AUDIO_OUT_PACKET);
    }
}  

void usbd_audiov2_open(uint8_t busid, uint8_t intf)
{
    rx_flag = 1;
	
	ring_buf_wr = 0;
    ring_buf_rd = 0;
    dma_started = false;

	uac2_audio_ring_buf = malloc_bsc(AUDIO_BUF_NUM * AUDIO_OUT_PACKET);
	uac2_dma_buf0 = malloc_bsc(AUDIO_OUT_PACKET);
	uac2_dma_buf1 = malloc_bsc(AUDIO_OUT_PACKET);
	uac2_usb_buf = malloc_bsc(AUDIO_OUT_PACKET);

    memset(uac2_audio_ring_buf, 0, AUDIO_BUF_NUM * AUDIO_OUT_PACKET);
    memset(uac2_dma_buf0, 0, AUDIO_OUT_PACKET);
    memset(uac2_dma_buf1, 0, AUDIO_OUT_PACKET);
	memset(uac2_usb_buf,  0, AUDIO_OUT_PACKET);
	
    /* setup first out ep read transfer */
	if(SAMPLE_BITS == 16)
	{
		I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
		music_bitdepth = 16;
	}
	else if (SAMPLE_BITS == 24) 
	{
		I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_24b);
		music_bitdepth = 32;
	}
	else if(SAMPLE_BITS == 32)
	{	
		I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_32b);
		music_bitdepth = 32;
	}
	I2S2_SampleRate_Set(48000);
	
	I2S2_TX_DMA_Init(uac2_dma_buf0, uac2_dma_buf1, AUDIO_OUT_PACKET/2); 

	usbd_ep_start_read(busid, AUDIO_OUT_EP, uac2_usb_buf, AUDIO_OUT_PACKET);

#if USING_FEEDBACK == 1
#ifdef CONFIG_USB_HS
    uint32_t feedback_value = AUDIO_FREQ_TO_FEEDBACK_HS(s_speaker_sample_rate);
    AUDIO_FEEDBACK_TO_BUF_HS(s_speakerv2_feedback_buffer, feedback_value);
#else
    uint32_t feedback_value = AUDIO_FREQ_TO_FEEDBACK_FS(s_speaker_sample_rate);
    AUDIO_FEEDBACK_TO_BUF_FS(s_speakerv2_feedback_buffer, feedback_value);
#endif
    usbd_ep_start_write(busid, AUDIO_OUT_FEEDBACK_EP, s_speakerv2_feedback_buffer, FEEDBACK_ENDP_PACKET_SIZE);
#endif
    USB_LOG_RAW("OPEN\r\n");
}

void usbd_audiov2_close(uint8_t busid, uint8_t intf)
{
    USB_LOG_RAW("CLOSE\r\n");
	I2S_Play_Stop();
	if(uac2_audio_ring_buf) free_bsc(uac2_audio_ring_buf);
	if(uac2_dma_buf0) free_bsc(uac2_dma_buf0);
	if(uac2_dma_buf1) free_bsc(uac2_dma_buf1);
	if(uac2_usb_buf)  free_bsc(uac2_usb_buf);
    rx_flag = 0;
}

void usbd_uac2_deinit(void)
{
    I2S_Play_Stop();
    dma_started = false;
    rx_flag = 0;
    
    if(uac2_audio_ring_buf) { free_bsc(uac2_audio_ring_buf); uac2_audio_ring_buf = NULL; }
    if(uac2_dma_buf0) { free_bsc(uac2_dma_buf0); uac2_dma_buf0 = NULL; }
    if(uac2_dma_buf1) { free_bsc(uac2_dma_buf1); uac2_dma_buf1 = NULL; }
    if(uac2_usb_buf)  { free_bsc(uac2_usb_buf); uac2_usb_buf = NULL; }
	
	usbd_ep_close(0, AUDIO_OUT_EP);
    usbd_ep_close(0, AUDIO_OUT_FEEDBACK_EP);
	usbd_deinitialize(0);
}

void usbd_audiov2_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq)
{
    if (ep == AUDIO_OUT_EP) {
        s_speaker_sample_rate = sampling_freq;
    }
}

uint32_t usbd_audiov2_get_sampling_freq(uint8_t busid, uint8_t ep)
{
    (void)busid;

    uint32_t freq = 0;

    if (ep == AUDIO_OUT_EP) {
        freq = s_speaker_sample_rate;
    }

    return freq;
}

void usbd_audiov2_get_sampling_freq_table(uint8_t busid, uint8_t ep, uint8_t **sampling_freq_table)
{
    if (ep == AUDIO_OUT_EP) {
        *sampling_freq_table = (uint8_t *)default_sampling_freq_table;
    }
}

void usbd_audio_iso_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    USB_LOG_RAW("actual out len:%d\r\n", (unsigned int)nbytes);
	
    if (ring_buf_wr >= ring_buf_rd)
        free_write_data = (AUDIO_BUF_NUM * AUDIO_OUT_PACKET) - (ring_buf_wr - ring_buf_rd);
    else
        free_write_data = ring_buf_rd - ring_buf_wr;
	
	if (free_write_data > nbytes)
	{
		if (ring_buf_wr + nbytes <= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) 
		{
            memcpy(&uac2_audio_ring_buf[ring_buf_wr], uac2_usb_buf, nbytes);
            ring_buf_wr += nbytes;
            if (ring_buf_wr >= AUDIO_BUF_NUM * AUDIO_OUT_PACKET) ring_buf_wr = 0;
        } 
		else // 需要回绕复制
		{
            uint32_t first_part = AUDIO_BUF_NUM * AUDIO_OUT_PACKET - ring_buf_wr;
            memcpy(&uac2_audio_ring_buf[ring_buf_wr], uac2_usb_buf, first_part);
            memcpy(uac2_audio_ring_buf, &uac2_usb_buf[first_part], nbytes - first_part);
            ring_buf_wr = nbytes - first_part;
        }
	}
	else
	{
		USB_LOG_RAW("WARNING: Ring buffer full");
	}

	// 检查是否需要启动DMA
	if (!dma_started) 
	{
		// 当缓冲区填充到一半时开始播放
		if (ring_buf_wr >= (AUDIO_BUF_NUM * AUDIO_OUT_PACKET) / 2) 
		{
			I2S_Play_Start();
			dma_started = 1;
		}
	}
    
    // 继续读取USB数据
    usbd_ep_start_read(busid, AUDIO_OUT_EP, uac2_usb_buf, AUDIO_OUT_PACKET);
}

#if USING_FEEDBACK == 1
void usbd_audiov2_iso_out_feedback_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    USB_LOG_RAW("actual feedback len:%d\r\n", (unsigned int)nbytes);
#ifdef CONFIG_USB_HS
    uint32_t feedback_value = AUDIO_FREQ_TO_FEEDBACK_HS(s_speaker_sample_rate);
    AUDIO_FEEDBACK_TO_BUF_HS(s_speakerv2_feedback_buffer, feedback_value);
#else
	uint32_t nominal_rate = AUDIO_FREQ_TO_FEEDBACK_FS(48000); ;
	
    static int8_t adjustment = 0;
    if (free_read_data > (AUDIO_BUF_NUM * AUDIO_OUT_PACKET * 3 / 4)) adjustment = -100;
    else if (free_read_data < (AUDIO_BUF_NUM * AUDIO_OUT_PACKET / 4)) adjustment = 100;

    uint32_t feedback_value = nominal_rate + adjustment;

    AUDIO_FEEDBACK_TO_BUF_FS(s_speakerv2_feedback_buffer, feedback_value);
#endif
    usbd_ep_start_write(busid, AUDIO_OUT_FEEDBACK_EP, s_speakerv2_feedback_buffer, FEEDBACK_ENDP_PACKET_SIZE);
}
#endif

static struct usbd_endpoint audio_out_ep = {
    .ep_cb = usbd_audio_iso_out_callback,
    .ep_addr = AUDIO_OUT_EP
};

#if USING_FEEDBACK == 1
static struct usbd_endpoint audio_out_feedback_ep = {
    .ep_cb = usbd_audiov2_iso_out_feedback_callback,
    .ep_addr = AUDIO_OUT_FEEDBACK_EP
};
#endif

static struct usbd_interface intf0;
static struct usbd_interface intf1;

struct audio_entity_info audiov2_entity_table[] = {
    { .bEntityId = AUDIO_OUT_CLOCK_ID,
      .bDescriptorSubtype = AUDIO_CONTROL_CLOCK_SOURCE,
      .ep = AUDIO_OUT_EP },
    { .bEntityId = AUDIO_OUT_FU_ID,
      .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
      .ep = AUDIO_OUT_EP },
};

void audio_v2_init(uint8_t busid, uintptr_t reg_base)
{
#ifdef CONFIG_USBDEV_ADVANCE_DESC
    usbd_desc_register(busid, &audio_v2_descriptor);
#else
    usbd_desc_register(busid, audio_v2_descriptor);
#endif
    usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf0, 0x0200, audiov2_entity_table, 2));
    usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf1, 0x0200, audiov2_entity_table, 2));
    usbd_add_endpoint(busid, &audio_out_ep);
#if USING_FEEDBACK == 1
    usbd_add_endpoint(busid, &audio_out_feedback_ep);
#endif
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void audio_v2_test(uint8_t busid)
{
    if (rx_flag) {
    }
}

void usbd_audiov2_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume_db)
{
    (void)busid;
    (void)ep;
    (void)ch;
    (void)volume_db;
}

int usbd_audiov2_get_volume(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;

    return 0;
}

void usbd_audiov2_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute)
{
    (void)busid;
    (void)ep;
    (void)ch;
    (void)mute;
}

bool usbd_audiov2_get_mute(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;

    return 0;
}
