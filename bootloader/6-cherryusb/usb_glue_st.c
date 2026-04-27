/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_dwc2_param.h"
#include "stm32f4xx.h"
#include "systick_conf.h"
#include "pin_ctrl.h"

#define USB_DEVICE_MODE

#ifndef CONFIG_USB_DWC2_CUSTOM_PARAM

const struct dwc2_user_params param_pa11_pa12 = {
    .phy_type = DWC2_PHY_TYPE_PARAM_FS,
    .device_dma_enable = false,
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (320 - 16 - 16 - 16 - 16),
    .device_tx_fifo_size = {
        [0] = 16, // 64 byte
        [1] = 16, // 64 byte
        [2] = 16, // 64 byte
        [3] = 16, // 64 byte
        [4] = 0,
        [5] = 0,
        [6] = 0,
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },
    .device_gccfg = ((1 << 16) | (1 << 21)), // fs: USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_NOVBUSSENS
    .total_fifo_size = 320 // 1280 byte
};

const struct dwc2_user_params param_pb14_pb15 = {
#ifdef CONFIG_USB_HS
    .phy_type = DWC2_PHY_TYPE_PARAM_UTMI,
#else
    .phy_type = DWC2_PHY_TYPE_PARAM_FS,
#endif
#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    .device_dma_enable = true,
#else
    .device_dma_enable = false,
#endif
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (1006 - 16 - 256 - 128 - 128 - 128 - 128), // 1006/1012
    .device_tx_fifo_size = {
        [0] = 16,  // 64 byte
        [1] = 256, // 1024 byte
        [2] = 128, // 512 byte
        [3] = 128, // 512 byte
        [4] = 128, // 512 byte
        [5] = 128, // 512 byte
        [6] = 0,
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },

    .host_dma_desc_enable = false,
    .host_rx_fifo_size = 622,
    .host_nperio_tx_fifo_size = 128, // 512 byte
    .host_perio_tx_fifo_size = 256,  // 1024 byte
    .device_gccfg = ((1 << 16) | (1 << 21)), // fs: USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_NOVBUSSENS hs:0
    .host_gccfg = ((1 << 16) | (1 << 21))    // fs: USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_NOVBUSSENS hs:0
};

#endif // CONFIG_USB_DWC2_CUSTOM_PARAM

typedef void (*usb_dwc2_irq)(uint8_t busid);

static usb_dwc2_irq g_usb_dwc2_irq[2];
static uint8_t g_usb_dwc2_busid[2] = { 0, 0 };

void usb_dc_low_level_init(uint8_t busid)
{
	g_usb_dwc2_busid[1] = busid;
    g_usb_dwc2_irq[1] = USBD_IRQHandler;
	
	/* 1. 开 USBOTG_HS 和 GPIOB 时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_OTG_HS, ENABLE);

    /* 2. 【核心修复 1】硬件强制复位（清空 DWC2 底层 FIFO 内存泄露） */
    RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_OTG_HS, ENABLE);
    for(volatile int i=0; i<50000; i++); 
    RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_OTG_HS, DISABLE);
    for(volatile int i=0; i<50000; i++);

    /* 3. 配置 PB14/PB15 为 OTG_HS FS DP/DM */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_OTG_HS_FS);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_OTG_HS_FS);
	
    NVIC_EnableIRQ(OTG_HS_IRQn);
}

void usb_dc_low_level_deinit(uint8_t busid)
{
    NVIC_DisableIRQ(OTG_HS_IRQn);
    NVIC_ClearPendingIRQ(OTG_HS_IRQn);

    /* 2. 清空指针 */
	g_usb_dwc2_busid[1] = 0;
    g_usb_dwc2_irq[1] = NULL;

    /* 3. 关时钟和配置 GPIO */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_OTG_HS, DISABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN; // 下拉
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 强行输出低电平
    GPIO_ResetBits(GPIOB, GPIO_Pin_14 | GPIO_Pin_15);
}

#ifndef CONFIG_USB_DWC2_CUSTOM_PARAM
void dwc2_get_user_params(uint32_t reg_base, struct dwc2_user_params *params)
{
    if (reg_base == 0x40040000UL) { // USB_OTG_HS_PERIPH_BASE
        memcpy(params, &param_pb14_pb15, sizeof(struct dwc2_user_params));
    } else {
        memcpy(params, &param_pa11_pa12, sizeof(struct dwc2_user_params));
    }
#ifdef CONFIG_USB_DWC2_CUSTOM_FIFO
    struct usb_dwc2_user_fifo_config s_dwc2_fifo_config;

    dwc2_get_user_fifo_config(reg_base, &s_dwc2_fifo_config);

    params->device_rx_fifo_size = s_dwc2_fifo_config.device_rx_fifo_size;
    for (uint8_t i = 0; i < MAX_EPS_CHANNELS; i++)
    {
        params->device_tx_fifo_size[i] = s_dwc2_fifo_config.device_tx_fifo_size[i];
    }
#endif
}
#endif

extern uint32_t SystemCoreClock;

void usbd_dwc2_delay_ms(uint8_t ms)
{
    uint32_t count = SystemCoreClock / 1000 * ms;
    while (count--) {
        __asm volatile("nop");
    }
}

uint32_t usbd_dwc2_get_system_clock(void)
{
    return SystemCoreClock;
}

void OTG_FS_IRQHandler(void)
{
    if (g_usb_dwc2_irq[0] != NULL) {
        g_usb_dwc2_irq[0](g_usb_dwc2_busid[0]);
    }
}

void OTG_HS_IRQHandler(void)
{
    if (g_usb_dwc2_irq[1] != NULL) {
        g_usb_dwc2_irq[1](g_usb_dwc2_busid[1]);
    }
}

