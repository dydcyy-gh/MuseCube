#include "stm32f4xx.h"
#include "w25q128.h"
#include <string.h>
#include "systick_conf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define W25X_WriteEnable        0x06 
#define W25X_WriteDisable       0x04 
#define W25X_ReadStatusReg      0x05 
#define W25X_WriteStatusReg     0x01 
#define W25X_ReadData           0x03 
#define W25X_FastReadData       0x0B 
#define W25X_FastReadDual       0x3B 
#define W25X_PageProgram        0x02 
#define W25X_BlockErase         0xD8 
#define W25X_SectorErase        0x20 
#define W25X_ChipErase          0xC7 
#define W25X_PowerDown          0xB9 
#define W25X_ReleasePowerDown   0xAB 
#define W25X_DeviceID           0xAB 
#define W25X_ManufactDeviceID   0x90 
#define W25X_JedecDeviceID      0x9F 

//NSS ctrl
#define SPI3_NSS_Clr() GPIOA->BSRR = (uint32_t)GPIO_Pin_15 << 16;
#define SPI3_NSS_Set() GPIOA->BSRR = (uint32_t)GPIO_Pin_15;

// 缓冲区大小调整：保持最小RAM占用为512字节
#define W25QXX_R_BUFF_SIZE  512
#define W25QXX_T_BUFF_SIZE  512     // TX buffer (512B)
#define W25QXX_SECTOR_BUFF_SIZE 512 // 用于分块处理扇区数据 (512B)

// CCM RAM 区域定义 (STM32F405/407 CCM start: 0x10000000, size: 64KB)
#define IS_CCM_RAM(addr) ((uint32_t)(addr) >= 0x10000000 && (uint32_t)(addr) < 0x10010000)

// 对齐
__attribute__((aligned(4))) uint8_t SPI3_RX_Buffer[W25QXX_R_BUFF_SIZE];
__attribute__((aligned(4))) uint8_t SPI3_TX_Buffer[W25QXX_T_BUFF_SIZE];
__attribute__((aligned(4))) uint8_t W25QXX_BUFFER[W25QXX_SECTOR_BUFF_SIZE];

SemaphoreHandle_t xFlashMutex = NULL;//w25q128互斥锁
SemaphoreHandle_t xFlashSemaphore = NULL;//w25q128计数型信号量

#define MIN_USE_DMA_SIZE 16

/* ==============================================================
   磨损均衡保留区配置 (最后1MB)
   总大小: 16MB (0x000000 - 0xFFFFFF)
   保留区: 0xF00000 - 0xFFFFFF (1MB)
   结构:
   - 0xF00000: 索引扇区 (Index Sector, 4KB), 记录当前 Scratch Index
   - 0xF01000: Scratch Block 1
   - ...
   - 0xFFF000: Scratch Block 255
============================================================== */
   
#define W25QXX_FLASH_SIZE       (16 * 1024 * 1024)
#define W25QXX_WEAR_LEVEL_SIZE  (1 * 1024 * 1024)
#define W25QXX_WEAR_LEVEL_BASE  (W25QXX_FLASH_SIZE - W25QXX_WEAR_LEVEL_SIZE) // 0xF00000

#define INDEX_SECTOR_ADDR       (W25QXX_WEAR_LEVEL_BASE)
#define SCRATCH_POOL_BASE       (W25QXX_WEAR_LEVEL_BASE + 4096)
#define MAX_SCRATCH_INDEX       255 // 1~255

void flash_lock(void) 
{
	while (xSemaphoreTake(xFlashMutex, portMAX_DELAY) != pdTRUE);
}

void flash_unlock(void) 
{
    xSemaphoreGive(xFlashMutex);
}

//SPI3 读写一个字节
uint8_t SPI3_ReadWriteByte(uint8_t TxData)
{                  
    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET){} //等待发送
    SPI_I2S_SendData(SPI3, TxData);   //通过外设SPIx发送一个数据
    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_RXNE) == RESET){}//等待接收
    return SPI_I2S_ReceiveData(SPI3); //返回通过SPIx最近接收的数据        
}

//读取芯片ID               
//0XEF17--W25Q128      
uint16_t W25QXX_ReadID(void)
{
    uint16_t Temp = 0;      
    SPI3_NSS_Clr();                 
    SPI3_ReadWriteByte(0x90); // 发送读取ID命令        
    SPI3_ReadWriteByte(0x00); // 地址字节1     
    SPI3_ReadWriteByte(0x00); // 地址字节2     
    SPI3_ReadWriteByte(0x00); // 地址字节3              
    Temp |= SPI3_ReadWriteByte(0xFF) << 8; // 制造商ID (应该是0xEF)  
    Temp |= SPI3_ReadWriteByte(0xFF);     // 设备ID (应该是0x17)     
    SPI3_NSS_Set();                 
    return Temp;
}

// 内部函数声明
void W25QXX_Write_Enable(void);
void W25QXX_Wait_Busy(uint8_t query_interval);
void W25QXX_Erase_Sector_Internal(uint32_t Sector_Addr);
void W25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite);
void W25QXX_Read_Internal(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);

//SPI3初始化 0-ok 1-err
uint8_t W25QXX_Init(void)
{
	xFlashMutex = xSemaphoreCreateMutex();
	xFlashSemaphore = xSemaphoreCreateCounting(2,0);
	
    memset(SPI3_RX_Buffer, 0xFF, W25QXX_R_BUFF_SIZE);
    memset(SPI3_TX_Buffer, 0xFF, W25QXX_T_BUFF_SIZE);
    
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);  // SPI3在APB1上
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);  // SPI3使用DMA1
    
    // SPI3_SCK(PB3), SPI3_MISO(PB4), SPI3_MOSI(PB5)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;          //复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;        //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;    //100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;          //上拉
    GPIO_Init(GPIOB, &GPIO_InitStructure);                //初始化GPIOB
    
    // SPI3_NSS (PA15)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;          //复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;        //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;    //100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;          //上拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
	GPIO_SetBits(GPIOA, GPIO_Pin_15);
	
    // 配置引脚复用功能
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_SPI3);  // PB3复用为 SPI3_SCK
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_SPI3);  // PB4复用为 SPI3_MISO
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI3);  // PB5复用为 SPI3_MOSI
    
    // 复位SPI3
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI3, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI3, DISABLE);
    
    // 配置SPI3
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //双线双向全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                       //主模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                   //8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                         //时钟极性高
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                        //第二个边沿采样
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                           //软件NSS
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;  //2分频
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                  //MSB先行
    SPI_InitStructure.SPI_CRCPolynomial = 7;                            //CRC多项式
    SPI_Init(SPI3, &SPI_InitStructure);
    
    // 初始化DMA1 Stream0 (SPI3_RX) - Channel 0
    DMA_DeInit(DMA1_Stream0);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;  // SPI3_RX使用Channel 0
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI3->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = 0; // 在具体函数中设置
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = 0; // 在具体函数中设置
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // 单次模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream0, &DMA_InitStructure);
    
    // 初始化DMA1 Stream5 (SPI3_TX) - Channel 0
    DMA_DeInit(DMA1_Stream5);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;  // SPI3_TX使用Channel 0
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI3->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = 0; // 在具体函数中设置
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = 0; // 在具体函数中设置
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // 单次模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream5, &DMA_InitStructure);
    
    // 使能DMA传输完成中断
    DMA_ITConfig(DMA1_Stream0, DMA_IT_TC, ENABLE);
    DMA_ITConfig(DMA1_Stream5, DMA_IT_TC, ENABLE);

    SPI_Cmd(SPI3, ENABLE); //使能SPI3外设
    
    SPI3_ReadWriteByte(0xff);//启动传输
    
    if(W25QXX_ReadID() != 0XEF17) return 1;//id error
    else return 0;
}

// DMA1 Stream0中断处理函数
void DMA1_Stream0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(DMA_GetITStatus(DMA1_Stream0, DMA_IT_TCIF0))
    {
        DMA_ClearITPendingBit(DMA1_Stream0, DMA_IT_TCIF0);
        DMA_Cmd(DMA1_Stream0, DISABLE);
        SPI_I2S_DMACmd(SPI3, SPI_I2S_DMAReq_Rx, DISABLE);

        xSemaphoreGiveFromISR(xFlashSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// DMA1 Stream5中断处理函数
void DMA1_Stream5_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(DMA_GetITStatus(DMA1_Stream5, DMA_IT_TCIF5))
    {
        DMA_ClearITPendingBit(DMA1_Stream5, DMA_IT_TCIF5);
        DMA_Cmd(DMA1_Stream5, DISABLE);
        SPI_I2S_DMACmd(SPI3, SPI_I2S_DMAReq_Tx, DISABLE);
        
        xSemaphoreGiveFromISR(xFlashSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// 等待DMA传输完成
void SPI3_DMA_Wait_Complete(void)
{
    xSemaphoreTake(xFlashSemaphore,portMAX_DELAY);
    xSemaphoreTake(xFlashSemaphore,portMAX_DELAY);
    
    // 等待SPI传输完成
    while(SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_BSY) == SET){}
    
    // 清除DMA中断标志
    DMA_ClearITPendingBit(DMA1_Stream0,DMA_IT_HTIF0|DMA_IT_TEIF0|DMA_IT_DMEIF0|DMA_IT_FEIF0);
    DMA_ClearITPendingBit(DMA1_Stream5,DMA_IT_HTIF5|DMA_IT_TEIF5|DMA_IT_DMEIF5|DMA_IT_FEIF5);
}

//等待空闲
void W25QXX_Wait_Busy(uint8_t query_interval)
{
    uint8_t status;
    do 
    {
        SPI3_NSS_Clr();
        SPI3_ReadWriteByte(W25X_ReadStatusReg);
        status = SPI3_ReadWriteByte(0xFF);
        SPI3_NSS_Set();
        
        if((status&0x01) && query_interval) Delay_ms(query_interval);
    } while(status & 0x01);
}

//W25QXX写使能 
void W25QXX_Write_Enable(void)   
{
    SPI3_NSS_Clr();                            //使能器件   
    SPI3_ReadWriteByte(W25X_WriteEnable);      //发送写使能  
    SPI3_NSS_Set();                            //取消片选                   
} 

//读取SPI FLASH 
void W25QXX_Read_Internal(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)   
{
    uint16_t i, bytes_to_read;
    uint32_t current_addr = ReadAddr;
    uint32_t bytes_remaining = NumByteToRead;
    
    while (bytes_remaining > 0)
    {
        bytes_to_read = (bytes_remaining > W25QXX_T_BUFF_SIZE) ? W25QXX_T_BUFF_SIZE : bytes_remaining;
        
        SPI3_NSS_Clr();
        SPI3_ReadWriteByte(W25X_ReadData);
        SPI3_ReadWriteByte((uint8_t)((current_addr)>>16));
        SPI3_ReadWriteByte((uint8_t)((current_addr)>>8));
        SPI3_ReadWriteByte((uint8_t)current_addr);
        
        // 增加对CCM RAM的检查
        if (bytes_to_read > MIN_USE_DMA_SIZE && !IS_CCM_RAM(pBuffer + (NumByteToRead - bytes_remaining)))
        {
            // 配置DMA - 修改为DMA1
            DMA1_Stream0->M0AR = (uint32_t)(pBuffer + (NumByteToRead - bytes_remaining));
            DMA1_Stream0->NDTR = bytes_to_read;
            DMA1_Stream5->M0AR = (uint32_t)SPI3_TX_Buffer;
            DMA1_Stream5->NDTR = bytes_to_read;
            
            // 启动DMA传输
            DMA_Cmd(DMA1_Stream5, ENABLE);
            DMA_Cmd(DMA1_Stream0, ENABLE);
            
            // 使能SPI DMA - 修改为SPI3
            SPI_I2S_DMACmd(SPI3, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);
            
            // 等待传输完成
            SPI3_DMA_Wait_Complete();
        }
        else
        {
            // 小数据量或目标地址在CCM区域，使用普通方式
            for (i = 0; i < bytes_to_read; i++)
            { 
                pBuffer[NumByteToRead - bytes_remaining + i] = SPI3_ReadWriteByte(0xFF); //循环读数  
            }
        }
        
        SPI3_NSS_Set(); //取消片选
        bytes_remaining -= bytes_to_read;
        current_addr += bytes_to_read;
    }
}  

//SPI在一页(0~65535)内写入少于256个字节的数据 
void W25QXX_Write_Page(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
    uint16_t i;  
    W25QXX_Write_Enable();                     //SET WEL 
    SPI3_NSS_Clr();                            //使能器件   
    SPI3_ReadWriteByte(W25X_PageProgram);      //发送写页命令   
    SPI3_ReadWriteByte((uint8_t)((WriteAddr)>>16)); //发送24bit地址    
    SPI3_ReadWriteByte((uint8_t)((WriteAddr)>>8));   
    SPI3_ReadWriteByte((uint8_t)WriteAddr);   
    
    // 使用DMA进行数据传输，增加对CCM RAM的检查
    if(NumByteToWrite > MIN_USE_DMA_SIZE && !IS_CCM_RAM(pBuffer)) 
    {
        // 配置DMA
        DMA1_Stream0->M0AR = (uint32_t)SPI3_RX_Buffer;
        DMA1_Stream0->NDTR = NumByteToWrite;
        
        DMA1_Stream5->M0AR = (uint32_t)pBuffer;
        DMA1_Stream5->NDTR = NumByteToWrite;
        
        // 启动DMA传输
        DMA_Cmd(DMA1_Stream5, ENABLE);
        DMA_Cmd(DMA1_Stream0, ENABLE);
        
        // 使能SPI DMA
        SPI_I2S_DMACmd(SPI3, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);
        
        // 等待传输完成
        SPI3_DMA_Wait_Complete();
    }
    else
    {
        // 小数据量或源地址在CCM区域，使用普通方式
        for(i=0;i<NumByteToWrite;i++)
            SPI3_ReadWriteByte(pBuffer[i]);//循环写数  
    }
    
    SPI3_NSS_Set();                           //取消片选 
    W25QXX_Wait_Busy(1);
}

// 无检验写（假定目标区域已擦除），自动跨页
void W25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)   
{
    uint16_t pageremain;      
    pageremain=256-WriteAddr%256; //单页剩余的字节数                  
    if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//不大于256个字节
    while(1)
    {      
        W25QXX_Write_Page(pBuffer,WriteAddr,pageremain);
        if(NumByteToWrite==pageremain)break;//写入结束了
        else //NumByteToWrite>pageremain
        {
            pBuffer+=pageremain;
            WriteAddr+=pageremain;    
            
            NumByteToWrite-=pageremain;              //减去已经写入了的字节数
            if(NumByteToWrite>256)pageremain=256; //一次可以写入256个字节
            else pageremain=NumByteToWrite;       //不够256个字节了
        }
    };
} 

// 擦除一个扇区
void W25QXX_Erase_Sector_Internal(uint32_t Sector_Addr)   
{
    W25QXX_Write_Enable();                  			//SET WEL    
    SPI3_NSS_Clr();                         			//使能器件   
    SPI3_ReadWriteByte(W25X_SectorErase);   			//发送扇区擦除指令 
    SPI3_ReadWriteByte((uint8_t)((Sector_Addr)>>16));	//发送24bit地址    
    SPI3_ReadWriteByte((uint8_t)((Sector_Addr)>>8));   
    SPI3_ReadWriteByte((uint8_t)Sector_Addr);  
    SPI3_NSS_Set();                            			//取消片选                  
    W25QXX_Wait_Busy(5);                       			//等待擦除完成
}

// 擦除整个芯片 
void W25QXX_Erase_Chip_Internal(void)   
{
    W25QXX_Write_Enable();                 //SET WEL 
    SPI3_NSS_Clr();                        //使能器件   
    SPI3_ReadWriteByte(W25X_ChipErase);    //发送片擦除命令  
    SPI3_NSS_Set();                        //取消片选                  
    W25QXX_Wait_Busy(200);                 //等待芯片擦除结束
}   

//磨损均衡核心逻辑
uint32_t W25QXX_Get_Next_Scratch_Addr(void)
{
    uint32_t search_offset = 0;
    uint8_t current_index = 0;
    uint8_t next_index = 0;
    uint32_t write_offset = 0;
    uint8_t found = 0;

    // 1. 扫描 Index Sector
    for (search_offset = 0; search_offset < 4096; search_offset += W25QXX_SECTOR_BUFF_SIZE)
    {
        W25QXX_Read_Internal(W25QXX_BUFFER, INDEX_SECTOR_ADDR + search_offset, W25QXX_SECTOR_BUFF_SIZE);
        
        // 在缓冲区中检查 0xFF
        for (uint16_t i = 0; i < W25QXX_SECTOR_BUFF_SIZE; i++)
        {
            if (W25QXX_BUFFER[i] == 0xFF)
            {
                // 找到第一个空闲位
                write_offset = search_offset + i;
                
                // 获取前一个字节作为 current_index
                if (write_offset > 0)
                {
                    // 如果不是 buffer 的第 0 个，直接取 buffer 前一个
                    if (i > 0) {
                        current_index = W25QXX_BUFFER[i-1];
                    } else {
                        // 跨 buffer 了，需要���新读取前一个字节
                        W25QXX_Read_Internal(&current_index, INDEX_SECTOR_ADDR + write_offset - 1, 1);
                    }
                }
                else
                {
                    // write_offset == 0, 说明整个扇区是空的
                    current_index = 0; 
                }
                found = 1;
                break;
            }
        }
        if (found) break;
    }

    // 2. 如果扫描完4096字节都没找到0xFF，说明满了
    if (!found)
    {
        // 读取最后一个字节作为 current_index
        W25QXX_Read_Internal(&current_index, INDEX_SECTOR_ADDR + 4095, 1);
        write_offset = 0; // 下次写到开头，触发擦除逻辑
    }

    // 3. 计算下一个 Index (1~255)
    if (current_index == 0 || current_index > MAX_SCRATCH_INDEX)
    {
        // 异常或初次使用
        next_index = 1;
    }
    else
    {
        next_index = (current_index % MAX_SCRATCH_INDEX) + 1;
    }

    // 4. 如果需要，擦除 Index Sector
    if (write_offset == 0)
	{
		uint8_t first_byte;
		W25QXX_Read_Internal(&first_byte, INDEX_SECTOR_ADDR, 1);
		if (first_byte != 0xFF)   // 只有非空才需要擦除
		{
			W25QXX_Erase_Sector_Internal(INDEX_SECTOR_ADDR);
		}
	}

    // 5. 写入新的 Index
    W25QXX_Write_NoCheck(&next_index, INDEX_SECTOR_ADDR + write_offset, 1);

    // 6. 返回对应的 Scratch 物理地址
    // Base + (Index - 1) * 4096
    return SCRATCH_POOL_BASE + (uint32_t)(next_index - 1) * 4096;
}

// 拷贝目标扇区到 指定的 scratch 地址
void copy_src_sector_to_scratch(uint32_t sector_base, uint32_t scratch_addr)
{
    // 防止自身拷贝
    if (sector_base == scratch_addr) return;

    W25QXX_Erase_Sector_Internal(scratch_addr);

    for(uint32_t off=0; off<4096; off+=W25QXX_SECTOR_BUFF_SIZE)
	{
        W25QXX_Read_Internal(W25QXX_BUFFER, sector_base + off, W25QXX_SECTOR_BUFF_SIZE);
        W25QXX_Write_NoCheck(W25QXX_BUFFER, scratch_addr + off, W25QXX_SECTOR_BUFF_SIZE);
    }
}

// 从 scratch 恢复，并覆盖 patch 数据
void restore_from_scratch_with_patch(uint32_t sector_base, uint32_t scratch_addr, uint16_t secoff, uint16_t secremain, uint8_t *p)
{
    for(uint32_t off=0; off<4096; off+=W25QXX_SECTOR_BUFF_SIZE){
        uint16_t chunk = W25QXX_SECTOR_BUFF_SIZE;
        W25QXX_Read_Internal(W25QXX_BUFFER, scratch_addr + off, chunk);

        uint32_t block_s = off, block_e = off + chunk;
        uint32_t write_s = secoff, write_e = secoff + secremain;

        uint32_t os = (block_s>write_s?block_s:write_s);
        uint32_t oe = (block_e<write_e?block_e:write_e);

        if(os<oe){
            uint32_t len = oe - os;
            memcpy(&W25QXX_BUFFER[os-block_s], &p[os-write_s], len);
        }

        W25QXX_Write_NoCheck(W25QXX_BUFFER, sector_base + off, chunk);
    }
}

// 写SPI FLASH
void W25QXX_Write_Internal(uint8_t* pBuffer,uint32_t WriteAddr,uint32_t NumByteToWrite)   
{ 
    uint32_t secpos;
    uint16_t secoff;
    uint16_t secremain;      
    uint16_t i;    
    uint8_t need_erase = 0;
    uint32_t current_addr = WriteAddr;
    uint32_t bytes_remaining = NumByteToWrite;
    
    // 保护保留区域不被常规 Write 写入 (可选，视需求而定)
    // if (WriteAddr >= W25QXX_WEAR_LEVEL_BASE) return; 

    // 处理循环，每次处理跨扇区的写
    while (bytes_remaining > 0)
    {
        secpos = current_addr / 4096UL; // 扇区索引
        secoff = current_addr % 4096UL; // 在扇区内偏移
        secremain = 4096 - secoff;
        if (bytes_remaining <= secremain) secremain = bytes_remaining;
        
        // 检查要写入区域在原扇区中是否全部为 0xFF（如果全为0xFF，则不需擦除）
        need_erase = 0;
        uint32_t check_addr = secpos * 4096UL + secoff;
        uint32_t checked = 0;
        while (checked < secremain)
        {
            uint16_t this_check = (secremain - checked > W25QXX_SECTOR_BUFF_SIZE) ? W25QXX_SECTOR_BUFF_SIZE : (secremain - checked);
            // 读取现有数据到缓冲
            W25QXX_Read_Internal(W25QXX_BUFFER, check_addr + checked, this_check);
            for (i = 0; i < this_check; i++)
            {
                if (W25QXX_BUFFER[i] != 0xFF)
                {
                    need_erase = 1;
                    break;
                }
            }
            if (need_erase) break;
            checked += this_check;
        }
        
        if (!need_erase)
        {
            W25QXX_Write_NoCheck(pBuffer + (NumByteToWrite - bytes_remaining), current_addr, secremain);
        }
        else
        {
            // 1. 获取一个动态的 Scratch 扇区地址 (带磨损均衡)
            uint32_t scratch_addr = W25QXX_Get_Next_Scratch_Addr();

            // 2. 拷贝原扇区到 Scratch
            copy_src_sector_to_scratch(secpos*4096, scratch_addr);
            
            // 3. 擦除原扇区
            W25QXX_Erase_Sector_Internal(secpos*4096);
            
            // 4. 从 Scratch 恢复数据，并打入 Patch (新数据)
            restore_from_scratch_with_patch(secpos*4096, scratch_addr, secoff, secremain, pBuffer+(NumByteToWrite-bytes_remaining));
        }
        
        // 更新指针
        current_addr += secremain;
        bytes_remaining -= secremain;
    }
}

void W25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint32_t NumByteToWrite)   
{
	flash_lock();
	W25QXX_Write_Internal(pBuffer,WriteAddr,NumByteToWrite);
	flash_unlock();
}

// use for FlashDB
void W25QXX_Write_Raw(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite)
{
    flash_lock();
    W25QXX_Write_NoCheck(pBuffer, WriteAddr, NumByteToWrite);
    flash_unlock();
}

void W25QXX_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)   
{
    flash_lock();
	W25QXX_Read_Internal(pBuffer,ReadAddr,NumByteToRead);
	flash_unlock();
}

void W25QXX_Erase_Sector(uint32_t Sector_Addr)   
{
    flash_lock();
	W25QXX_Erase_Sector_Internal(Sector_Addr);   
    flash_unlock();
}
      
void W25QXX_Erase_Chip(void)   
{
    flash_lock();    
	W25QXX_Erase_Chip_Internal();  
    flash_unlock();
}
