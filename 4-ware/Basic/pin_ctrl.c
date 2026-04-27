
//pin_ctrl.c 用于初始化控制输出，读取输入的基本基本引脚

#include "stm32f4xx.h"
#include "variables.h"

//充电检测 VBUS PB13 BAT IN PA8
void Battery_Ischarging_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void Is_Battery_Charging(void)//0-不在充电 1-正在充电 2-充电完成
{
	uint8_t Bat_Status = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8); //0-充电
	g_vbus_status = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);
	if(!Bat_Status) g_charge_status = 1;
	else {if(g_vbus_status) g_charge_status = 2;else g_charge_status = 0;}
}

//不断电控制 PWD_EN PA11
void Power_Maintain_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOA, GPIO_Pin_11);
}

void Power_Maintain_Ctrl(uint8_t status)//0-free 1-maintain
{
	if(status == 0) GPIO_ResetBits(GPIOA, GPIO_Pin_11);
	else GPIO_SetBits(GPIOA, GPIO_Pin_11);
	g_maintain_status = status;
}

//音响供电 SND_EN PC7
void Speaker_Power_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOC, GPIO_Pin_7);
}

void Speaker_Power_Ctrl(uint8_t status)//0-no power 1-power on
{
	if(status == 0) GPIO_ResetBits(GPIOC, GPIO_Pin_7);
	else GPIO_SetBits(GPIOC, GPIO_Pin_7);
}

//耳放供电 HDP_EN PA9
void Headphone_Power_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOA, GPIO_Pin_9);
}

void Headphone_Power_Ctrl(uint8_t status)//0-no power 1-power on
{
	if(status == 0) GPIO_ResetBits(GPIOA, GPIO_Pin_9);
	else GPIO_SetBits(GPIOA, GPIO_Pin_9);
}

//耳机插入检测 HDP_IN PC5
void Headphone_Isconnecting_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void Is_Headphone_Connecting(void)//1--耳机插入 0-无插入
{
	g_headphone_status = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5);
}

//TF卡插入检测 SDIO_CD PB8
void TFcard_Isconnecting_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void Is_TFcard_Connecting(void)
{
	g_TFcard_status = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) ? 0 : 1;//1-无插入
}

//i2s切换控制 I2S_CTRL PB11
void I2S_Exchange_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOB, GPIO_Pin_11);
}

void I2S_Exchange_Ctrl(uint8_t status) //0-耳机  1-喇叭
{
	if(status == 0) GPIO_ResetBits(GPIOB, GPIO_Pin_11);
	else GPIO_SetBits(GPIOB, GPIO_Pin_11);
	g_hdp0_or_spk1 = status;
}

//USB向外供电 USB_EN PA12
void USB_Power_Out_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOA, GPIO_Pin_12);
}

void USB_Power_Out_Ctrl(uint8_t status) //0-关  1-开
{
	if(status == 0) GPIO_ResetBits(GPIOA, GPIO_Pin_12);
	else GPIO_SetBits(GPIOA, GPIO_Pin_12);
}

//USB主从切换 USB_CTRL A10
void USB_Slave_Host_Init(void) //PA10
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOA, GPIO_Pin_10);
}

void USB_Slave_Host_Ctrl(uint8_t status) //0-从  1-主
{
	if(status == 0) GPIO_ResetBits(GPIOA, GPIO_Pin_10);
	else GPIO_SetBits(GPIOA, GPIO_Pin_10);
}

void Pin_Ctrl_Init(void)
{
	Battery_Ischarging_Init();
	Power_Maintain_Init();
	Speaker_Power_Init();
	Headphone_Power_Init();
	Headphone_Isconnecting_Init(); 
	TFcard_Isconnecting_Init(); 
	I2S_Exchange_Init(); 
	USB_Power_Out_Init(); 
	USB_Slave_Host_Init(); 
}
