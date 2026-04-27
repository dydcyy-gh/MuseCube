#include "stm32f4xx.h"
#include "systick_conf.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define USE_HORIZONTAL 1

#define LCD_W 240
#define LCD_H 240

// 更新引脚定义
#define LCD_RES_Clr()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12 << 16
#define LCD_RES_Set()  GPIOB->BSRR = (uint32_t)GPIO_Pin_12
#define LCD_DC_Clr()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4 << 16
#define LCD_DC_Set()   GPIOA->BSRR = (uint32_t)GPIO_Pin_4
#define LCD_BLK_Clr()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6 << 16
#define LCD_BLK_Set()  GPIOC->BSRR = (uint32_t)GPIO_Pin_6

#define F8T16 0x00
//write data add F8T16 behind
//write reg  add F8T16 front

SemaphoreHandle_t xLcdDmaSemaphore = NULL;

#define FOREGROUND_COLOR   0xFFFF   // 白色
#define BACKGROUND_COLOR   0x0000   // 黑色

uint16_t muse_cube_color_buf[240 * 20];
uint16_t background_color_buf[240 * 20];
uint16_t Progress_bar_buf[240 * 10];

static void LCD_PrepareMuseCubeIcon(void);

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
	
	if (xLcdDmaSemaphore == NULL) {
        xLcdDmaSemaphore = xSemaphoreCreateBinary();
    }
	
	LCD_PrepareMuseCubeIcon();
	
	for (uint32_t i = 0; i < sizeof(background_color_buf) / sizeof(background_color_buf[0]); i++) 
		background_color_buf[i] = BACKGROUND_COLOR;
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
	LCD_GPIO_Init();
	
	LCD_BLK_Clr();
	
	LCD_RES_Set();
	LCD_RES_Clr();
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
	LCD_WR_DATA16((0x1F<<8)|0x1F);
	LCD_WR_DATA16((0x00<<8)|0x33);
	LCD_WR_DATA16((0x33<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xB7);  
	LCD_WR_DATA16((0x35<<8)|F8T16); 

	LCD_WR_REG16((F8T16<<8)|0xBB);     
	LCD_WR_DATA16((0x2B<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC0);     
	LCD_WR_DATA16((0x2C<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC2);     
	LCD_WR_DATA16((0x01<<8)|F8T16); 

	LCD_WR_REG16((F8T16<<8)|0xC3);     
	LCD_WR_DATA16((0x0F<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC4);     
	LCD_WR_DATA16((0x20<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xC6);     
	LCD_WR_DATA16((0x15<<8)|F8T16);
	
	LCD_WR_REG16((F8T16<<8)|0xD0);
	LCD_WR_DATA16((0xA4<<8)|0xA1);

	LCD_WR_REG16((F8T16<<8)|0xD6);  
	LCD_WR_DATA16((0xA1<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0xE0);
	LCD_WR_DATA16((0xF0<<8)|0x04);
	LCD_WR_DATA16((0x07<<8)|0x04);
	LCD_WR_DATA16((0x04<<8)|0x04);
	LCD_WR_DATA16((0x25<<8)|0x33);
	LCD_WR_DATA16((0x3C<<8)|0x36);
	LCD_WR_DATA16((0x14<<8)|0x12);
	LCD_WR_DATA16((0x29<<8)|0x30);

	LCD_WR_REG16((F8T16<<8)|0xE1);
	LCD_WR_DATA16((0xF0<<8)|0x02);
	LCD_WR_DATA16((0x04<<8)|0x05);
	LCD_WR_DATA16((0x05<<8)|0x21);
	LCD_WR_DATA16((0x25<<8)|0x32);
	LCD_WR_DATA16((0x3B<<8)|0x38);
	LCD_WR_DATA16((0x12<<8)|0x14);
	LCD_WR_DATA16((0x27<<8)|0x31);

	LCD_WR_REG16((F8T16<<8)|0xE4);
	LCD_WR_DATA16((0x1D<<8)|0x00);
	LCD_WR_DATA16((0x00<<8)|F8T16);

	LCD_WR_REG16((F8T16<<8)|0x21);     

	LCD_WR_REG16((F8T16<<8)|0x29);
}

// 纯粹的底层 DMA 发送函数（不设地址）
void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count)
{
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


//中断
void DMA2_Stream5_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream5, DMA_IT_TCIF5))
    {
        DMA_ClearITPendingBit(DMA2_Stream5, DMA_IT_TCIF5);
        DMA_Cmd(DMA2_Stream5, DISABLE);
        SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);
        while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);

		if (xLcdDmaSemaphore != NULL) {
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(xLcdDmaSemaphore, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

uint8_t muse_cube_map[] = {
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xff, 0x0f, 0xf0, 0xf0, 0x0f, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0xf0, 
  0xff, 0x0f, 0xf0, 0xf0, 0x0f, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0xf0, 
  0xff, 0x0f, 0xf0, 0xf0, 0x0f, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0xf0, 
  0xff, 0x0f, 0xf0, 0xf0, 0x0f, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0xf0, 
  0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0x00, 0xff, 0xff, 0xf0, 
  0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0x00, 0xff, 0xff, 0xf0, 
  0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0x00, 0xff, 0xff, 0xf0, 
  0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0x00, 0xff, 0xff, 0xf0, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0x00, 
  0xf0, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0xf0, 0xf0, 0x00, 0x00, 
  0xf0, 0x00, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
  0xf0, 0x00, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0x00, 0x0f, 0xff, 0x00, 0x00, 0xff, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0xf0, 0x0f, 0xff, 0x00, 
};

static void LCD_PrepareMuseCubeIcon(void)
{
    uint8_t width = 172;          // 图标实际宽度
    uint8_t height = 20;           // 图标高度
    uint16_t buf_width = 240;       // 缓冲区宽度
    uint16_t offset_x = (buf_width - width) / 2;  // 水平居中偏移 = 34

    // 1. 整个缓冲区填充背景色
    for (uint32_t i = 0; i < buf_width * height; i++) {
        muse_cube_color_buf[i] = BACKGROUND_COLOR;
    }

    // 2. 将图标数据转换并放入缓冲区居中位置
    uint16_t bytes_per_line = (width + 7) / 8;    // 每行占用的字节数
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint32_t byte_index = y * bytes_per_line + (x / 8);
            uint8_t bit_mask = 0x80 >> (x % 8);
            uint8_t byte = muse_cube_map[byte_index];
            uint16_t color = (byte & bit_mask) ? FOREGROUND_COLOR : BACKGROUND_COLOR;
            muse_cube_color_buf[y * buf_width + offset_x + x] = color;
        }
    }
}

void Display_Icon(uint8_t y)
{
	LCD_Color_Fill(0,y,239,y+19,muse_cube_color_buf);
	xSemaphoreTake(xLcdDmaSemaphore,portMAX_DELAY);
}

void Display_None(uint8_t y)
{
	LCD_Color_Fill(0,y,239,y+19,background_color_buf);
	xSemaphoreTake(xLcdDmaSemaphore,portMAX_DELAY);
}

void Progress_Bar(uint8_t y, uint8_t num)
{
    uint16_t offset_x = (240 - 172) / 2;      // 水平居中偏移 = 34
    uint16_t left = offset_x;                  // 进度条左边界列号
    uint16_t right = offset_x + 172 - 1;       // 进度条右边界列号 (205)
    uint16_t top = 0;                           // 缓冲区顶部行索引
    uint16_t bottom = 9;                        // 缓冲区底部行索引 (高度10, 索引0~9)

    // 填充缓冲区 (高度10)
    for (uint16_t row = 0; row < 10; row++) {
        for (uint16_t col = 0; col < 240; col++) {
            uint16_t color = BACKGROUND_COLOR;
            if (col >= left && col <= right) 
            {
                // 边框：顶部/底部行 或 左/右边界列
                if (row == top || row == bottom || col == left || col == right) 
                {
                    color = FOREGROUND_COLOR;
                } 
                else 
                {
                    // 内部填充区域 (宽度170, 高度8)
                    uint16_t inner_left = left + 1;       // 内部左边界 (35)
                    uint16_t inner_right = right - 1;      // 内部右边界 (204)
                    uint16_t fill_end = inner_left + num - 1; // 填充部分的最后一列

                    if (col >= inner_left && col <= inner_right) 
                    {
                        if (col <= fill_end) 
                            color = FOREGROUND_COLOR;      // 已填充部分
                        else 
                            color = BACKGROUND_COLOR;      // 未填充部分
                    }
                }
            }
            Progress_bar_buf[row * 240 + col] = color;
        }
    }

    // 显示到屏幕，高度为10，因此 yend = y + 9
    LCD_Color_Fill(0, y, 239, y + 9, Progress_bar_buf);
    xSemaphoreTake(xLcdDmaSemaphore, portMAX_DELAY);
}
