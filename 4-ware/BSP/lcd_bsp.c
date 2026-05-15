#include "stm32f4xx.h"
#include "systick_conf.h"
#include "FreeRTOS.h"
#include "defines.h"
#include "variables.h"

#define USE_HORIZONTAL 1

#define LCD_W 240
#define LCD_H 240

// 更新引脚定义 v9 BLK为C6 RES为B12
#define LCD_RES_Clr()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12 << 16
#define LCD_RES_Set()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12
#define LCD_DC_Clr()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4 << 16
#define LCD_DC_Set()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4
#define LCD_BLK_Clr()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6 << 16
#define LCD_BLK_Set()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6

//#define LCD_BLK_Clr()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12 << 16
//#define LCD_BLK_Set()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12
//#define LCD_DC_Clr()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4 << 16
//#define LCD_DC_Set()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4
//#define LCD_RES_Clr()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6 << 16
//#define LCD_RES_Set()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6

#define F8T16 0x00
//write data add F8T16 behind
//write reg  add F8T16 front

static uint8_t lcd_dma_user = 0;

void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    
    // 启用新的GPIO和SPI时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);  // SPI1使用DMA2
    
    // LCD控制引脚初始化
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;  // LCD_RST -> PC6
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_6);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;  // LCD_DC -> PA4
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12; // LCD_BLK -> PB12
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
    
    // SPI引脚配置 (SPI1_SCK->PA5, SPI1_MOSI->PA7)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);
    
    // SPI配置为16位模式
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;  // 42MHz @ 84MHz PCLK2
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    // DMA配置 - SPI1_TX使用DMA2 Stream3/Stream5 (这里选择Stream5)
    DMA_DeInit(DMA2_Stream5);
    DMA_InitStructure.DMA_Channel = DMA_Channel_3;  // SPI1_TX使用通道3
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI1->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = 0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream5, &DMA_InitStructure);
    
    DMA_ITConfig(DMA2_Stream5, DMA_IT_TC, ENABLE);

    SPI_Cmd(SPI1, ENABLE);
	
    if (xLcdEventGroup == NULL) {
        xLcdEventGroup = xEventGroupCreate();
    }
}

// 新增：等待 SPI 完全空闲（仅在切换 DC 信号前使用）
static inline void LCD_Wait_Idle(void) {
    while (!(SPI1->SR & SPI_I2S_FLAG_TXE)){}
    while (SPI1->SR & SPI_I2S_FLAG_BSY){}
}

// 优化：去掉死等 BSY，利用硬件 FIFO 压榨吞吐量
static inline void LCD_WR_DATA16(uint16_t data)
{
    while (!(SPI1->SR & SPI_I2S_FLAG_TXE)){} // 仅检查发送缓冲区是否可写
    SPI1->DR = data;
}

// 优化：修改入参为 uint16_t（防止原来的 uint8_t 截断 F8T16 高位），并保护 DC 切换
static inline void LCD_WR_REG16(uint16_t reg)
{
    LCD_Wait_Idle(); // 必须等前面的数据彻底发完，才能拉低 DC 切换为命令模式
    LCD_DC_Clr();
    
    SPI1->DR = reg;
    
    LCD_Wait_Idle(); // 等待命令发完，再拉高 DC 恢复数据模式
    LCD_DC_Set();
}

//设置起始和结束地址  x1,x2 设置列的起始和结束地址  y1,y2 设置行的起始和结束地址
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
	LCD_WR_REG16((F8T16<<8)|0x2a);
	LCD_WR_DATA16(x1 + ((USE_HORIZONTAL == 3) ? 80 : 0));
	LCD_WR_DATA16(x2 + ((USE_HORIZONTAL == 3) ? 80 : 0));
	LCD_WR_REG16((F8T16<<8)|0x2b);
	LCD_WR_DATA16(y1 + ((USE_HORIZONTAL == 1) ? 80 : 0));
	LCD_WR_DATA16(y2 + ((USE_HORIZONTAL == 1) ? 80 : 0));
	LCD_WR_REG16((F8T16<<8)|0x2c);
}

void LCD_Init(void)
{
	LCD_GPIO_Init();//初始化GPIO
	
	LCD_BLK_Clr();//打开背光
	
	LCD_RES_Set();
	LCD_RES_Clr();//复位
	Delay_ms(1);
	LCD_RES_Set();
	Delay_ms(120);

	LCD_WR_REG16((F8T16<<8)|0x11);//Sleep out 
	
	LCD_WR_REG16((F8T16<<8)|0x36);
	if     (USE_HORIZONTAL==0) LCD_WR_DATA16((0x00<<8)|F8T16);
	else if(USE_HORIZONTAL==1) LCD_WR_DATA16((0xC0<<8)|F8T16);
	else if(USE_HORIZONTAL==2) LCD_WR_DATA16((0x70<<8)|F8T16);
	else                       LCD_WR_DATA16((0xA0<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0x3A);     
	LCD_WR_DATA16((0x05<<8)|F8T16);  

	LCD_WR_REG16((F8T16<<8)|0xB2);
	LCD_WR_DATA16((0x0C<<8)|0x0C);
	LCD_WR_DATA16((0x00<<8)|0x33);
	LCD_WR_DATA16((0x33<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xB7);  
	LCD_WR_DATA16((0x35<<8)|F8T16); 

	LCD_WR_REG16((F8T16<<8)|0xBB);     
	LCD_WR_DATA16((0x32<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC2);     
	LCD_WR_DATA16((0x01<<8)|F8T16); 

	LCD_WR_REG16((F8T16<<8)|0xC3);     
	LCD_WR_DATA16((0x10<<8)|F8T16); //

	LCD_WR_REG16((F8T16<<8)|0xC4);     
	LCD_WR_DATA16((0x20<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC6);     
	LCD_WR_DATA16((0x0F<<8)|F8T16); //
	
	LCD_WR_REG16((F8T16<<8)|0xD0);
	LCD_WR_DATA16((0xA4<<8)|0xA1);

	LCD_WR_REG16((F8T16<<8)|0xE0);
	LCD_WR_DATA16((0xD0<<8)|0x08);
	LCD_WR_DATA16((0x0E<<8)|0x09);
	LCD_WR_DATA16((0x09<<8)|0x05);
	LCD_WR_DATA16((0x31<<8)|0x33);
	LCD_WR_DATA16((0x48<<8)|0x17);
	LCD_WR_DATA16((0x14<<8)|0x15);
	LCD_WR_DATA16((0x31<<8)|0x34);

	LCD_WR_REG16((F8T16<<8)|0xE1);
	LCD_WR_DATA16((0xD0<<8)|0x08);
	LCD_WR_DATA16((0x0E<<8)|0x09);
	LCD_WR_DATA16((0x09<<8)|0x15);
	LCD_WR_DATA16((0x31<<8)|0x33);
	LCD_WR_DATA16((0x48<<8)|0x17);
	LCD_WR_DATA16((0x14<<8)|0x15);
	LCD_WR_DATA16((0x31<<8)|0x34);

	LCD_WR_REG16((F8T16<<8)|0x21);     

	LCD_WR_REG16((F8T16<<8)|0x29);
}

// 纯粹的底层 DMA 发送函数（不设地址）
void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count)
{
    lcd_dma_user = g_lcd_user; //获取本次传输的真正发起者
    DMA2_Stream5->M0AR = (uint32_t)data;
    DMA2_Stream5->NDTR = pixel_count;
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA2_Stream5, ENABLE);
}

// 供 LVGL 使用的颜色填充
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t *color)
{
    LCD_Address_Set(xsta, ysta, xend, yend);
    uint32_t pixel_count = (xend - xsta + 1) * (yend - ysta + 1);
    LCD_Write_DMA(color, pixel_count);
}

extern void LCD_DMA_TransferComplete(void);

//中断
void DMA2_Stream5_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream5, DMA_IT_TCIF5))
    {
        DMA_ClearITPendingBit(DMA2_Stream5, DMA_IT_TCIF5);
        DMA_Cmd(DMA2_Stream5, DISABLE);
        SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);
        while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);

        if(lcd_dma_user == LCD_USER_LVGL) LCD_DMA_TransferComplete();
		else
		{
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xEventGroupSetBitsFromISR(xLcdEventGroup, lcd_dma_user, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
    }
}
