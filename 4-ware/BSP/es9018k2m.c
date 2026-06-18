#include "stm32f4xx.h"                  
#include "es9018k2m.h"                  
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "systick_conf.h"
#include "pin_ctrl.h"

#define ESS9018_ADDR    0x90    // 设备地址 (写)
#define ESS9018_ADDR_R  0x91    // 设备地址 (读)

#define ES9018K2M_SYSTEM_SETTING                0x00
#define ES9018K2M_INPUT_CONFIG                  0x01
#define ES9018K2M_AUTOMUTE_TIME                 0x04
#define ES9018K2M_AUTOMUTE_LEVEL                0x05
#define ES9018K2M_DEEMPHASIS                    0x06
#define ES9018K2M_GENERAL_SET                   0x07
#define ES9018K2M_GPIO_CONFIG                   0x08
#define ES9018K2M_M_MODE_CONTROL                0x0A
#define ES9018K2M_CHANNEL_MAP                   0x0B
#define ES9018K2M_DPLL                          0x0C
#define ES9018K2M_THD_COMPENSATION              0x0D
#define ES9018K2M_SOFT_START                    0x0E
#define ES9018K2M_VOLUME1                       0x0F
#define ES9018K2M_VOLUME2                       0x10
#define ES9018K2M_MASTERTRIM0                   0x11
#define ES9018K2M_MASTERTRIM1                   0x12
#define ES9018K2M_MASTERTRIM2                   0x13
#define ES9018K2M_MASTERTRIM3                   0x14
#define ES9018K2M_INPUT_SELECT                  0x15
#define ES9018K2M_2_HARMONIC_COMPENSATION_0     0x16
#define ES9018K2M_2_HARMONIC_COMPENSATION_1     0x17
#define ES9018K2M_3_HARMONIC_COMPENSATION_0     0x18
#define ES9018K2M_3_HARMONIC_COMPENSATION_1     0x19

//read only
#define ES9018K2M_CHIP_STATUS                   0x40
#define ES9018K2M_DPLL_RATIO0                   0x42
#define ES9018K2M_DPLL_RATIO1                   0x43
#define ES9018K2M_DPLL_RATIO2                   0x44
#define ES9018K2M_DPLL_RATIO3                   0x45

/* ================== 软件IIC引脚操作宏 ================== */
#define IIC_SCL_H()  GPIO_SetBits(GPIOB, GPIO_Pin_6)
#define IIC_SCL_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_6)
#define IIC_SDA_H()  GPIO_SetBits(GPIOB, GPIO_Pin_7)
#define IIC_SDA_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_7)
#define IIC_SDA_READ() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)

/* ================== 私有全局变量 ================== */
static ES9018_State_t  s_es9018_state = {0};
static ES9018_Config_t s_es9018_config_new = {0,0,104,2,0,0,0,0,0,5,0,1,5,1,0,0,0,0};
static ES9018_Config_t s_es9018_config_old = {0,0,104,2,0,0,0,0,0,5,0,1,5,1,0,0,0,0};
static volatile uint8_t s_settings_changed = 0;

/* ================== 内部底层封装(Static) ================== */

static void IIC_lock(void) 
{
    while (xSemaphoreTake(xIICMutex, portMAX_DELAY) != pdTRUE);
}

static void IIC_unlock(void) 
{
    xSemaphoreGive(xIICMutex);
}

/* 软件IIC微秒级延时 (适配STM32F4 168MHz主频) */
static void IIC_Delay(void)
{
    __IO uint32_t i = 30; // 适当调整，确保I2C速率在400KHz以内
    while(i--);
}

static void I2C1_Init(void) 
{
    if(xIICMutex == NULL) {
        xIICMutex = xSemaphoreCreateMutex();
    }
    
    GPIO_InitTypeDef GPIO_InitStruct;

    // 1. 使能时钟 (只需要GPIO时钟)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    // 2. 配置GPIO引脚（PB6: SCL, PB7: SDA）为开漏输出
    // 开漏输出模式下，输出1时引脚由外部/内部上拉电阻拉高，此时也可直接读取引脚电平
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;         // 普通输出模式
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;        // 必须为开漏输出
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;          // 开启内部上拉
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 释放总线
    IIC_SCL_H();
    IIC_SDA_H();
}

static void IIC_Start(void)
{
    IIC_SDA_H();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SDA_L();  // SCL为高时，SDA由高变低产生起始信号
    IIC_Delay();
    IIC_SCL_L();  // 钳住I2C总线，准备发送或接收数据
}

static void IIC_Stop(void)
{
    IIC_SDA_L();
    IIC_SCL_L();
    IIC_Delay();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SDA_H();  // SCL为高时，SDA由低变高产生停止信号
    IIC_Delay();
}

static uint8_t IIC_Wait_Ack(void)
{
    uint8_t errTime = 0;
    
    IIC_SDA_H();  // 主机释放SDA线
    IIC_Delay();
    IIC_SCL_H();
    IIC_Delay();
    
    while(IIC_SDA_READ())
    {
        errTime++;
        if(errTime > 250)
        {
            IIC_Stop();
            return 1; // 接收应答失败
        }
    }
    IIC_SCL_L(); // 时钟输出0
    return 0;    // 接收应答成功
}

static void IIC_NAck(void)
{
    IIC_SCL_L();
    IIC_SDA_H(); // SDA拉高，发出NACK (这是解决硬件I2C读单字节BUG的关键)
    IIC_Delay();
    IIC_SCL_H();
    IIC_Delay();
    IIC_SCL_L();
}

static void IIC_Send_Byte(uint8_t txData)
{                        
    uint8_t t;   
    IIC_SCL_L(); // 拉低时钟开始数据传输
    for(t = 0; t < 8; t++)
    {              
        if((txData & 0x80) >> 7)
            IIC_SDA_H();
        else
            IIC_SDA_L();
        txData <<= 1; 	  
        IIC_Delay();
        IIC_SCL_H();
        IIC_Delay();
        IIC_SCL_L();
        IIC_Delay();
    }
}

static uint8_t IIC_Read_Byte(void)
{
    uint8_t i, receive = 0;
    IIC_SDA_H(); // 切换为输入前释放数据线
    for(i = 0; i < 8; i++)
    {
        IIC_SCL_L();
        IIC_Delay();
        IIC_SCL_H();
        receive <<= 1;
        if(IIC_SDA_READ()) receive++;   
        IIC_Delay(); 
    }
    return receive;
}

static uint8_t I2C_Write_ES9018(uint8_t regAddr, uint8_t data) 
{
    IIC_lock();
    
    IIC_Start();
    IIC_Send_Byte(ESS9018_ADDR); // 发送设备写地址
    if(IIC_Wait_Ack()) {
        IIC_unlock();
        return ES9018_ERR_I2C_ADDR;
    }
    
    IIC_Send_Byte(regAddr);      // 发送寄存器地址
    if(IIC_Wait_Ack()) {
        IIC_Stop();
        IIC_unlock();
        return ES9018_ERR_I2C_TX_REG;
    }
    
    IIC_Send_Byte(data);         // 发送数据
    if(IIC_Wait_Ack()) {
        IIC_Stop();
        IIC_unlock();
        return ES9018_ERR_I2C_TX_DATA;
    }
    
    IIC_Stop();
    IIC_unlock();
    return ES9018_OK;
}

static uint8_t I2C_Read_ES9018(uint8_t regAddr, uint8_t *pData) 
{
    IIC_lock(); 
    
    IIC_Start();
    IIC_Send_Byte(ESS9018_ADDR); // 发送设备写地址
    if(IIC_Wait_Ack()) {
        IIC_unlock();
        return ES9018_ERR_I2C_ADDR;
    }
    
    IIC_Send_Byte(regAddr);      // 发送寄存器地址
    if(IIC_Wait_Ack()) {
        IIC_Stop();
        IIC_unlock();
        return ES9018_ERR_I2C_TX_REG;
    }
    
    IIC_Start();                 // Restart 信号
    IIC_Send_Byte(ESS9018_ADDR_R); // 发送设备读地址 (0x91)
    if(IIC_Wait_Ack()) {
        IIC_Stop();
        IIC_unlock();
        return ES9018_ERR_I2C_RX_MODE;
    }
    
    *pData = IIC_Read_Byte();    // 接收1个字节
    IIC_NAck();                  // 读取完成后发送 NACK 信号
    IIC_Stop();                  // 产生停止信号
    
    IIC_unlock();
    return ES9018_OK;
}

static void ES9018_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

static uint8_t ES9018_Reg4_Set_AutoMute_Time(uint8_t Automute_Time)
{
    return I2C_Write_ES9018(ES9018K2M_AUTOMUTE_TIME, Automute_Time);
}

static uint8_t ES9018_Reg5_Set_AutoMute_Level(uint8_t Enable_Loopback, uint8_t Automute_Level)
{
    if(Automute_Level > 127) Automute_Level = 127;
    if(Enable_Loopback)
        return I2C_Write_ES9018(ES9018K2M_AUTOMUTE_LEVEL, 128 + Automute_Level);
    else 
        return I2C_Write_ES9018(ES9018K2M_AUTOMUTE_LEVEL, Automute_Level);
}

static uint8_t ES9018_Reg6_DeEmphasis(uint8_t Vol_Rate)
{
    if(Vol_Rate > 7) Vol_Rate = 7;
    return I2C_Write_ES9018(ES9018K2M_DEEMPHASIS, Vol_Rate + 72);
}

static uint8_t ES9018_Reg7_General_Set(uint8_t Filter_Shape, uint8_t IIR_BW, uint8_t Mute_L, uint8_t Mute_R)
{
    uint8_t value = 128;
    
    if(Filter_Shape == 1) value += 32;
    else if(Filter_Shape == 2) value += 64;
    
    if(IIR_BW == 1) value += 4;
    else if(IIR_BW == 2) value += 8;
    else if(IIR_BW == 3) value += 12;
    
    if(Mute_L == 1) value += 1;
    if(Mute_R == 1) value += 2;
    
    return I2C_Write_ES9018(ES9018K2M_GENERAL_SET, value);
}

static uint8_t ES9018_Reg10_Master_Mode_Control(uint8_t stop_div)
{
    return I2C_Write_ES9018(ES9018K2M_M_MODE_CONTROL, stop_div);
}

static uint8_t ES9018_Reg11_Channel_Mapping(uint8_t Sel_Ch1, uint8_t Sel_Ch2)
{
    uint8_t value = 0;
    if(Sel_Ch1) value += 1;
    if(Sel_Ch2) value += 2;
    return I2C_Write_ES9018(ES9018K2M_CHANNEL_MAP, value);
}

static uint8_t ES9018_Reg12_DPLL_Setting(uint8_t DPLL_BW)
{
    if(DPLL_BW > 15) DPLL_BW = 15;
    uint8_t value = 16 * DPLL_BW + 10;
    return I2C_Write_ES9018(ES9018K2M_DPLL, value);
}

static uint8_t ES9018_Reg13_THD_Compensation(uint8_t Bypass_THD)
{
    if(Bypass_THD) 
        return I2C_Write_ES9018(ES9018K2M_THD_COMPENSATION, 0x40);
    else 
        return I2C_Write_ES9018(ES9018K2M_THD_COMPENSATION, 0x00);
}

static uint8_t ES9018_Reg14_Soft_Start_Setting(uint8_t Mute_Lock, uint8_t OutL_Lock)
{
    uint8_t value = 134;
    if(Mute_Lock) value += 32;
    if(OutL_Lock) value += 64;
    return I2C_Write_ES9018(ES9018K2M_SOFT_START, value);
}

static uint8_t ES9018_Reg21_Bypass_OSF_IIR(uint8_t Bypass_IIR)
{
    uint8_t value = 0;
    if(Bypass_IIR) value += 4;
    return I2C_Write_ES9018(ES9018K2M_INPUT_SELECT, value); 
}

static uint8_t ES9018_Reg22_23_THD_Comp_C2(int16_t Compensation)
{
    uint8_t res = ES9018_OK;
    uint8_t high_byte = (Compensation >> 8) & 0xFF;
    uint8_t low_byte = Compensation & 0xFF;
    
    res = I2C_Write_ES9018(ES9018K2M_2_HARMONIC_COMPENSATION_0, low_byte); 
    if(!res) res = I2C_Write_ES9018(ES9018K2M_2_HARMONIC_COMPENSATION_1, high_byte);
    return res;
}

static uint8_t ES9018_Reg24_25_THD_Comp_C3(int16_t Compensation)
{
    uint8_t res = ES9018_OK;
    uint8_t high_byte = (Compensation >> 8) & 0xFF;
    uint8_t low_byte = Compensation & 0xFF;
    
    res = I2C_Write_ES9018(ES9018K2M_3_HARMONIC_COMPENSATION_0, low_byte); 
    if(!res) res = I2C_Write_ES9018(ES9018K2M_3_HARMONIC_COMPENSATION_1, high_byte);
    return res;
}

static uint8_t ES9018_Read_Chip_Status(void) 
{
    uint8_t res = ES9018_OK;
    uint8_t value = 0;
    res = I2C_Read_ES9018(ES9018K2M_CHIP_STATUS, &value);
    
    if(!res) {
        s_es9018_state.revision = (value >> 5) & 0x01;
        s_es9018_state.chip_id = (value >> 2) & 0x07;
    }
    return res;
}

static uint8_t ES9018_Read_SampleRate(void) 
{
    uint8_t reg66 = 0, reg67 = 0, reg68 = 0, reg69 = 0;
    uint8_t res = ES9018_OK;
    
    res = I2C_Read_ES9018(ES9018K2M_DPLL_RATIO0, &reg66);
    if(res) return res;
    res = I2C_Read_ES9018(ES9018K2M_DPLL_RATIO1, &reg67);
    if(res) return res;
    res = I2C_Read_ES9018(ES9018K2M_DPLL_RATIO2, &reg68);
    if(res) return res;
    res = I2C_Read_ES9018(ES9018K2M_DPLL_RATIO3, &reg69);
    if(res) return res;
    
    uint32_t dpll_num = ((uint32_t)reg69 << 24) |
                        ((uint32_t)reg68 << 16) |
                        ((uint32_t)reg67 << 8)  |
                        ((uint32_t)reg66 << 0);
    
    uint64_t numerator = (uint64_t)dpll_num * 38400000;
    s_es9018_state.sample_rate = (uint32_t)(numerator >> 32); 
    
    return res;
}


/* ================== 外部公共接口 API ================== */

const ES9018_State_t* ES9018_Get_State(void) 
{
    return &s_es9018_state;
}

void ES9018_Get_Config(ES9018_Config_t *config) 
{
    if (config) {
        *config = s_es9018_config_new;
    }
}

void ES9018_Set_Config(const ES9018_Config_t *config) 
{
    if (config) {
        s_es9018_config_new = *config;
        s_settings_changed = 1;
    }
}

uint8_t ES9018_Set_BitDepth(uint8_t BitDepth)
{
    if(!g_es9018_inited) return ES9018_ERR_NOT_WORKING;

    if(BitDepth == 16)
        return I2C_Write_ES9018(ES9018K2M_INPUT_CONFIG, 0x0C);
    else if(BitDepth == 24)
        return I2C_Write_ES9018(ES9018K2M_INPUT_CONFIG, 0x4C);
    else
        return I2C_Write_ES9018(ES9018K2M_INPUT_CONFIG, 0x8C);
}

uint8_t ES9018_Set_Volume(uint8_t ch1, uint8_t ch2)
{
    if(!g_es9018_inited) return ES9018_ERR_NOT_WORKING;

    uint8_t res = ES9018_OK;
    res = I2C_Write_ES9018(ES9018K2M_VOLUME1, 255 - ch1);
    if(!res) res = I2C_Write_ES9018(ES9018K2M_VOLUME2, 255 - ch2);
    return res;
}

uint8_t ES9018_Poll_Status(void)
{
    if(!g_es9018_inited) return ES9018_ERR_NOT_WORKING;

    s_es9018_state.automute_status = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5);
    s_es9018_state.dpll_lock_status = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4);
    return ES9018_Read_SampleRate();
}

uint8_t ES9018_Init(void)
{
    uint8_t res = ES9018_OK;
    
    // 0. 上电
    Headphone_Power_Ctrl(1);
    Delay_ms(10);
    
    // 1. 初始化底层硬件接口
    ES9018_GPIO_Init();
    I2C1_Init();
    
    // 2. 检查芯片是否在线并读取状态
    res = ES9018_Read_Chip_Status();
    if(s_es9018_state.chip_id != 4 && !res) {
        res = ES9018_ERR_CHIP_ID;
    }
    
    // 3. 如果通信正常，强制全量恢复所有寄存器配置（应对芯片区域断电后恢复）
    if(res == ES9018_OK)
    {	
        g_es9018_inited = 1;
		
		ES9018_Set_BitDepth(music_bitdepth);
		ES9018_Set_Volume(kv_hdp_value,kv_hdp_value);
        ES9018_Reg4_Set_AutoMute_Time(s_es9018_config_new.Automute_Time);
        ES9018_Reg5_Set_AutoMute_Level(s_es9018_config_new.Enable_Loopback, s_es9018_config_new.Automute_Level);
        ES9018_Reg6_DeEmphasis(s_es9018_config_new.Vol_Rate);
        ES9018_Reg7_General_Set(s_es9018_config_new.Filter_Shape, s_es9018_config_new.IIR_BW, s_es9018_config_new.Mute_Ch1, s_es9018_config_new.Mute_Ch2);
        ES9018_Reg10_Master_Mode_Control(s_es9018_config_new.Stop_Div);
        ES9018_Reg11_Channel_Mapping(s_es9018_config_new.Sel_Ch1, s_es9018_config_new.Sel_Ch2);
        ES9018_Reg12_DPLL_Setting(s_es9018_config_new.DPLL_BW);
        ES9018_Reg13_THD_Compensation(s_es9018_config_new.Bypass_THD);
        ES9018_Reg14_Soft_Start_Setting(s_es9018_config_new.Mute_Lock, s_es9018_config_new.OutL_Lock);
        ES9018_Reg21_Bypass_OSF_IIR(s_es9018_config_new.Bypass_IIR);
        ES9018_Reg22_23_THD_Comp_C2(s_es9018_config_new.C2);
        ES9018_Reg24_25_THD_Comp_C3(s_es9018_config_new.C3);
        
        s_es9018_config_old = s_es9018_config_new;
        s_settings_changed = 0;
    }
	else
	{
		g_es9018_inited = 0;
		Headphone_Power_Ctrl(0);
    }
    
    return res;
}

void ES9018_Deinit(void)
{
    g_es9018_inited = 0;
	Headphone_Power_Ctrl(0);
}

uint8_t ES9018_Update_Register(void)
{
    if(!g_es9018_inited) return ES9018_ERR_NOT_WORKING;

    uint8_t res = ES9018_OK;
    
    if(s_settings_changed)
    {
        if(s_es9018_config_old.Automute_Time != s_es9018_config_new.Automute_Time) {
            res = ES9018_Reg4_Set_AutoMute_Time(s_es9018_config_new.Automute_Time);
            s_es9018_config_old.Automute_Time = s_es9018_config_new.Automute_Time;
        }
        
        if(s_es9018_config_old.Enable_Loopback != s_es9018_config_new.Enable_Loopback || 
           s_es9018_config_old.Automute_Level != s_es9018_config_new.Automute_Level) {
            res = ES9018_Reg5_Set_AutoMute_Level(s_es9018_config_new.Enable_Loopback, s_es9018_config_new.Automute_Level);
            s_es9018_config_old.Enable_Loopback = s_es9018_config_new.Enable_Loopback;
            s_es9018_config_old.Automute_Level = s_es9018_config_new.Automute_Level;
        }
        
        if(s_es9018_config_old.Vol_Rate != s_es9018_config_new.Vol_Rate) {
            res = ES9018_Reg6_DeEmphasis(s_es9018_config_new.Vol_Rate);
            s_es9018_config_old.Vol_Rate = s_es9018_config_new.Vol_Rate;
        }
        
        if(s_es9018_config_old.Filter_Shape != s_es9018_config_new.Filter_Shape ||
           s_es9018_config_old.IIR_BW != s_es9018_config_new.IIR_BW ||
           s_es9018_config_old.Mute_Ch1 != s_es9018_config_new.Mute_Ch1 ||
           s_es9018_config_old.Mute_Ch2 != s_es9018_config_new.Mute_Ch2) {
            res = ES9018_Reg7_General_Set(s_es9018_config_new.Filter_Shape, s_es9018_config_new.IIR_BW, s_es9018_config_new.Mute_Ch1, s_es9018_config_new.Mute_Ch2);
            s_es9018_config_old.Filter_Shape = s_es9018_config_new.Filter_Shape;
            s_es9018_config_old.IIR_BW = s_es9018_config_new.IIR_BW;
            s_es9018_config_old.Mute_Ch1 = s_es9018_config_new.Mute_Ch1;
            s_es9018_config_old.Mute_Ch2 = s_es9018_config_new.Mute_Ch2;
        }
        
        if(s_es9018_config_old.Stop_Div != s_es9018_config_new.Stop_Div) {
            res = ES9018_Reg10_Master_Mode_Control(s_es9018_config_new.Stop_Div);
            s_es9018_config_old.Stop_Div = s_es9018_config_new.Stop_Div;
        }
        
        if(s_es9018_config_old.Sel_Ch1 != s_es9018_config_new.Sel_Ch1 || 
           s_es9018_config_old.Sel_Ch2 != s_es9018_config_new.Sel_Ch2) {
            res = ES9018_Reg11_Channel_Mapping(s_es9018_config_new.Sel_Ch1, s_es9018_config_new.Sel_Ch2);
            s_es9018_config_old.Sel_Ch1 = s_es9018_config_new.Sel_Ch1;
            s_es9018_config_old.Sel_Ch2 = s_es9018_config_new.Sel_Ch2;
        }
        
        if(s_es9018_config_old.DPLL_BW != s_es9018_config_new.DPLL_BW) {
            res = ES9018_Reg12_DPLL_Setting(s_es9018_config_new.DPLL_BW);
            s_es9018_config_old.DPLL_BW = s_es9018_config_new.DPLL_BW;
        }
        
        if(s_es9018_config_old.Bypass_THD != s_es9018_config_new.Bypass_THD) {
            res = ES9018_Reg13_THD_Compensation(s_es9018_config_new.Bypass_THD);
            s_es9018_config_old.Bypass_THD = s_es9018_config_new.Bypass_THD;
        }
        
        if(s_es9018_config_old.Mute_Lock != s_es9018_config_new.Mute_Lock || 
           s_es9018_config_old.OutL_Lock != s_es9018_config_new.OutL_Lock) {
            res = ES9018_Reg14_Soft_Start_Setting(s_es9018_config_new.Mute_Lock, s_es9018_config_new.OutL_Lock);
            s_es9018_config_old.Mute_Lock = s_es9018_config_new.Mute_Lock;
            s_es9018_config_old.OutL_Lock = s_es9018_config_new.OutL_Lock;
        }
        
        if(s_es9018_config_old.Bypass_IIR != s_es9018_config_new.Bypass_IIR) {
            res = ES9018_Reg21_Bypass_OSF_IIR(s_es9018_config_new.Bypass_IIR);
            s_es9018_config_old.Bypass_IIR = s_es9018_config_new.Bypass_IIR;
        }
        
        if(s_es9018_config_old.C2 != s_es9018_config_new.C2) {
            res = ES9018_Reg22_23_THD_Comp_C2(s_es9018_config_new.C2);
            s_es9018_config_old.C2 = s_es9018_config_new.C2;
        }
        
        if(s_es9018_config_old.C3 != s_es9018_config_new.C3) {
            res = ES9018_Reg24_25_THD_Comp_C3(s_es9018_config_new.C3);
            s_es9018_config_old.C3 = s_es9018_config_new.C3;
        }
        
        s_settings_changed = 0;
    }
    return res;
}

uint8_t ES9018_Soft_Reset(void)
{
    if(!g_es9018_inited) return ES9018_ERR_NOT_WORKING;

    uint8_t res = ES9018_OK;
    res = I2C_Write_ES9018(ES9018K2M_SYSTEM_SETTING, 0x01);
    if(!res)
    {
        s_es9018_config_new.Enable_Loopback = s_es9018_config_old.Enable_Loopback = 0;
        s_es9018_config_new.Automute_Time = s_es9018_config_old.Automute_Time = 0;
        s_es9018_config_new.Automute_Level = s_es9018_config_old.Automute_Level = 104;
        s_es9018_config_new.Vol_Rate = s_es9018_config_old.Vol_Rate = 2;
        s_es9018_config_new.Filter_Shape = s_es9018_config_old.Filter_Shape = 0;
        s_es9018_config_new.Mute_Ch1 = s_es9018_config_old.Mute_Ch1 = 0;
        s_es9018_config_new.Mute_Ch2 = s_es9018_config_old.Mute_Ch2 = 0;
        s_es9018_config_new.Bypass_IIR = s_es9018_config_old.Bypass_IIR = 0;
        s_es9018_config_new.IIR_BW = s_es9018_config_old.IIR_BW = 0;
        s_es9018_config_new.Stop_Div = s_es9018_config_old.Stop_Div = 5;
        s_es9018_config_new.Sel_Ch1 = s_es9018_config_old.Sel_Ch1 = 0;
        s_es9018_config_new.Sel_Ch2 = s_es9018_config_old.Sel_Ch2 = 1;
        s_es9018_config_new.DPLL_BW = s_es9018_config_old.DPLL_BW = 5;
        s_es9018_config_new.Bypass_THD = s_es9018_config_old.Bypass_THD = 1;
        s_es9018_config_new.C2 = s_es9018_config_old.C2 = 0;
        s_es9018_config_new.C3 = s_es9018_config_old.C3 = 0;
        s_es9018_config_new.Mute_Lock = s_es9018_config_old.Mute_Lock = 0;
        s_es9018_config_new.OutL_Lock = s_es9018_config_old.OutL_Lock = 0;
    }
    return res;
}
