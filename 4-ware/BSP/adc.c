#include "stm32f4xx.h"
#include "pin_ctrl.h"
#include "variables.h"
#include "defines.h"
#include "debug.h"

// ADC通道数量定义
#define Adc_Chennel_Count 8 

// DMA缓冲区用于存储ADC转换结果 (直接使用这个原始数据)
static volatile uint16_t adc_value[Adc_Chennel_Count];

void ADC_StartConversion(void);

void ADC1_DMA_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStruct;
    
    // 1. 开启时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    
    // 2. 配置GPIO
    // PA1-PA3 作为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // PB0-PB1 作为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // PC0-PC1 作为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    // 启用内部参考电压
    ADC_TempSensorVrefintCmd(ENABLE);
    
    // 3. 配置DMA
    DMA_DeInit(DMA2_Stream0);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)adc_value;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = Adc_Chennel_Count;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;      //单次模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_Low; //优先级
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);
    
    // dma传输完成中断
    DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);
    
    // 4. 配置ADC
    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div4; // ADC时钟分频
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;
    ADC_CommonInit(&ADC_CommonInitStruct);
    
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE; // 扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; // 单次转换模式
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = Adc_Chennel_Count;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 配置ADC通道及采样顺序
    // PA2 (ADC1_IN2) usb cc1
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_480Cycles);
    // PA1 (ADC1_IN1) usb cc2
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_480Cycles);
    // PA3 (ADC1_IN3) 2/3vbat
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_480Cycles);
    // PB0 (ADC1_IN8) KL_X
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 4, ADC_SampleTime_480Cycles);
    // PB1 (ADC1_IN9) KL_Y
    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 5, ADC_SampleTime_480Cycles);
    // PC0 (ADC1_IN10) KR_X
    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 6, ADC_SampleTime_480Cycles);
    // PC1 (ADC1_IN11) KR_Y
    ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 7, ADC_SampleTime_480Cycles);
    // 内部参考电压 (ADC1 Ch17)
    ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 8, ADC_SampleTime_480Cycles);
    
    // 允许adc dma
    ADC_DMARequestAfterLastTransferCmd(ADC1,ENABLE);
    
    // 启用ADC DMA请求
    ADC_DMACmd(ADC1, ENABLE);
    
    // 启用ADC
    ADC_Cmd(ADC1, ENABLE);
    
    // 进行一次转换
    ADC_StartConversion();
}

void ADC_StartConversion(void)
{
    g_adc_dma_finished = 0;
    DMA_Cmd(DMA2_Stream0, ENABLE);
    ADC_SoftwareStartConv(ADC1);
}

// 基于12位ADC (0-4095, 3.3V基准) 的阈值区间
#define ADC_THR_RA_MAX       250   // 判定 Ra (线缆芯片) 的上限      (< 0.2V)
#define ADC_THR_RD_DEF_MAX   820   // 判定 默认USB供电 (0.4V) 的上限 (< 0.66V)
#define ADC_THR_RD_1A5_MAX   1526  // 判定 1.5A供电 (0.9V) 的上限    (< 1.23V)
#define ADC_THR_RD_3A_MAX    2600  // 判定 3.0A供电 (最高2.04V) 的上限
#define ADC_THR_OPEN_MIN     3500  // 判定 开路/悬空状态的下限

void Get_TypeC_Status(void)
{
	static uint8_t cc_host_count = 0;
	uint8_t last_count = cc_host_count;
	
	uint16_t slav_cc_max = (g_slave_cc1_value > g_slave_cc2_value) ? g_slave_cc1_value : g_slave_cc2_value;
	uint16_t host_cc_max = (g_host_cc1_value > g_host_cc2_value) ? g_host_cc1_value : g_host_cc2_value;
	uint16_t host_cc_min = (g_host_cc1_value < g_host_cc2_value) ? g_host_cc1_value : g_host_cc2_value;
	
	if(host_cc_max < ADC_THR_RD_DEF_MAX){
		if(slav_cc_max > ADC_THR_RA_MAX) 			g_usb_status = TYPEC_CC_OKEY;
		else 								 			cc_host_count++;
	}
	else{
		if(host_cc_min < ADC_THR_RA_MAX){
			if(slav_cc_max > ADC_THR_RA_MAX) 		    g_usb_status = TYPEC_CC_OKEY;
			else 								 		g_usb_status = TYPEC_CC_IDLE;
		}
		else if(host_cc_min < ADC_THR_RD_DEF_MAX) 		g_usb_status = TYPEC_IS_HOST;
		else if(host_cc_min < ADC_THR_OPEN_MIN) 		g_usb_status = TYPEC_AC_IDLE;
		else{
			if(slav_cc_max > ADC_THR_RA_MAX) 			g_usb_status = TYPEC_AC_OKEY;
			else 								 		g_usb_status = TYPEC_NO_FIND;
		}
	}

	if(cc_host_count > 9)  								g_usb_status = TYPEC_CC_HOST;
	if(last_count == cc_host_count || cc_host_count>9)	cc_host_count = 0;
	
	if(g_usb_status == TYPEC_AC_OKEY || g_usb_status == TYPEC_CC_OKEY) USB_Slave_Host_Ctrl(0);
	if(g_usb_status == TYPEC_CC_HOST || g_usb_status == TYPEC_IS_HOST) USB_Slave_Host_Ctrl(1);
}

// 计算adc (彻底移除滤波逻辑，直接取 adc_value)
void ADC_Calculate_Voltage(void)
{
    // 读取内部参考电压校准值 (出厂校准值)
    float Vrefint = *(__IO uint16_t *)(0x1FFF7A2A);
    
    // 提取公共系数，减少浮点乘除法次数
    // 注意：这里全部改为直接使用原始的 adc_value
    float common_voltage_factor = (Vrefint * 0.0008056640625f) / adc_value[7];
    float vdda_real_factor = Vrefint / adc_value[7]; // 摇杆使用的系数

    float temp_bat_voltage;
    int16_t temp_LX, temp_LY, temp_RX, temp_RY;
	
	//cc计算逻辑
	static uint8_t state = 0;
	state = (state+1)%5;
	
	if(g_usb_status < 3)
	{
		switch(state)//0 - 4
		{
			case 0: 
				USB_Slave_Host_Ctrl(0); 
				break;
			case 2: 
				g_slave_cc1_value = adc_value[0]; 
				g_slave_cc2_value = adc_value[1]; 
				USB_Slave_Host_Ctrl(1);
				break;
			case 4: 
				g_host_cc1_value = adc_value[0]; 
				g_host_cc2_value = adc_value[1];
				Get_TypeC_Status(); 
				break;
			default: 
				break;
		}
	}
	else
	{
		static uint8_t disconnect_cnt = 0; 
		uint8_t is_disconnect = 0;
		
		if(g_usb_status > 4) //host
		{
			g_host_cc1_value = adc_value[0]; g_host_cc2_value = adc_value[1];
			uint16_t max = (adc_value[0] > adc_value[1]) ? adc_value[0] : adc_value[1];
			uint16_t min = (adc_value[0] < adc_value[1]) ? adc_value[0] : adc_value[1];
			if(g_usb_status == TYPEC_CC_HOST && max > ADC_THR_OPEN_MIN)  is_disconnect = 1;
			if(g_usb_status == TYPEC_IS_HOST && min > ADC_THR_OPEN_MIN)  is_disconnect = 1;
		}
		else                 //device
		{
			g_slave_cc1_value = adc_value[0]; g_slave_cc2_value = adc_value[1];
			uint16_t max = (adc_value[0] > adc_value[1]) ? adc_value[0] : adc_value[1];
			if(max < ADC_THR_RA_MAX)        							 is_disconnect = 1;
		}
		
		if(is_disconnect)
		{
			disconnect_cnt++;
			if(disconnect_cnt > 9)
			{
				g_usb_status = TYPEC_NO_FIND; 
				USB_Slave_Host_Ctrl(0);
				disconnect_cnt = 0;
				state = 4;
			}
		}
		else disconnect_cnt = 0; 
	}
	
    // 计算电池电压
    temp_bat_voltage = adc_value[2] * (common_voltage_factor * 1.5f);

    // 电池电压微小波动过滤 (迟滞)
    if (temp_bat_voltage > (g_battery_voltage + 0.01f) || temp_bat_voltage < (g_battery_voltage - 0.01f))
    {
        g_battery_voltage = temp_bat_voltage;
    }
    
    // 摇杆计算
    temp_LX = (int16_t)((adc_value[3] * vdda_real_factor - 2048) / 8.0f);
    temp_LY = (int16_t)((adc_value[4] * vdda_real_factor - 2048) / 8.0f);
    temp_RX = (int16_t)((adc_value[5] * vdda_real_factor - 2048) / 8.0f);
    temp_RY = (int16_t)((2048 - adc_value[6] * vdda_real_factor) / 8.0f);
    
    // 摇杆限幅及死区优化 (利用连续的 if-else 减少判断)
    #define LIMIT_AND_DEADZONE(val) \
        do { \
            if (val > 127) val = 127; \
            else if (val < -128) val = -128; \
            if (val < 20 && val > -20) val = 0; \
        } while(0)

    LIMIT_AND_DEADZONE(temp_LX);
    LIMIT_AND_DEADZONE(temp_LY);
    LIMIT_AND_DEADZONE(temp_RX);
    LIMIT_AND_DEADZONE(temp_RY);
    
    // 更新全局摇杆值 (减少写入操作)
    if (temp_LX != g_key_L_X) g_key_L_X = temp_LX;
    if (temp_LY != g_key_L_Y) g_key_L_Y = temp_LY;
    if (temp_RX != g_key_R_X) g_key_R_X = temp_RX;
    if (temp_RY != g_key_R_Y) g_key_R_Y = temp_RY;
}

// DMA2 Stream0中断服务函数
void DMA2_Stream0_IRQHandler(void) 
{
    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0)) 
    {
        g_adc_dma_finished = 1;
        DMA_Cmd(DMA2_Stream0, DISABLE);
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
    }
}
