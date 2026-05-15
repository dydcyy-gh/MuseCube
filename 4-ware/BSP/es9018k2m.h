//1.
//是否启用平滑自动静音 1-启动 0-不启动
//设置自动静音时间 time_ms = 2096896/(automute_time/1000)/SampleRate/64; 0-255
//设置自动静音级别 0-127 对应（静音后降低到0 -- -127db）

//2.
//音量改变的速率 （0-7） 0.0078125 x fs / 2(vol_rate-5) dB/s 

//3.
//静音ch1/ch2 // 1:Muted  0:unMuted

//4.
//数字滤波器模式选择 Filter_Shape: 0 fast rolloff  1 slow rolloff  2 minimum phase 

//5.
//IIR滤波器启用 默认为0(开启) 
//带宽设置 IIR_BW : 0 1.0757 x fs   1 1.1338 x fs  2 1.3605 x fs  3 1.5873 x fs

//6.
//锁定时钟信号时钟数 多少个时钟边沿后锁定信号(>96khz得设置为0)
//0-18364 1-8192 2-5461 3-4096 4-3276 5-2730 6-2340 7-2048

//7.
//改变左右声道 0-left 1-right

//8.
//DPLL矫正力度/带宽 选更大的获取更大带宽,选更小的矫正i2s ck的抖动
//DPLL_BW  0-15   0:close   15:max   default:5  

//9.
//开启THD补偿 默认关闭(0)
//开启后,使用  输出 = 输入 + (输入²) × thd_comp_c2 + (输入³) × thd_comp_c3
//补偿模拟输出级可能产生的二次,三次谐波失真,未经过测试不建议打开

//10.
//时钟环失效时强制静音、输出低

#ifndef __ES9018K2M_H__
#define __ES9018K2M_H__

#include "stm32f4xx.h"

typedef enum {
    ES9018_OK = 0,
    ES9018_ERR_I2C_START,
    ES9018_ERR_I2C_ADDR,
    ES9018_ERR_I2C_TX_REG,
    ES9018_ERR_I2C_TX_DATA,
    ES9018_ERR_I2C_RESTART,
    ES9018_ERR_I2C_RX_MODE,
    ES9018_ERR_I2C_RX_DATA,
    ES9018_ERR_INPUT,
    ES9018_ERR_CHIP_ID,
    ES9018_ERR_NOT_WORKING,
} ES9018_Error_e;

// ES9018 配置参数结构体 (原 ES9018_Status)
typedef __packed struct {
    uint8_t Enable_Loopback;   // 自动静音
    uint8_t Automute_Time;
    uint8_t Automute_Level;
    uint8_t Vol_Rate;          // 音量变化速率
    uint8_t Filter_Shape;      // 数字滤波器(Digital Filters)
    uint8_t Mute_Ch1;          // 手动静音
    uint8_t Mute_Ch2;
    uint8_t Bypass_IIR;        // 无限脉冲响应滤波器(IIR)
    uint8_t IIR_BW;
    uint8_t Stop_Div;          // 锁定信号时钟数
    uint8_t Sel_Ch1;           // 配置通道输出
    uint8_t Sel_Ch2;
    uint8_t DPLL_BW;           // DPLL带宽
    uint8_t Bypass_THD;        // 谐波补偿
    int16_t C2;
    int16_t C3;
    uint8_t Mute_Lock;         // 失锁处理
    uint8_t OutL_Lock;
} ES9018_Config_t;

// ES9018 实时运行状态结构体 (整合散落的全局变量)
typedef struct {
    uint8_t  revision;         // 版本 (0=Rev W, 1=Rev V)
    uint8_t  chip_id;          // 芯片id (4=es9018k2m)
    uint8_t  automute_status;  // 0-未静音 1-静音
    uint8_t  dpll_lock_status; // 1-正常锁定 0-未锁定
    uint32_t sample_rate;      // 采样率 (44100, 48000...)
} ES9018_State_t;

// 获取芯片状态信息指针 (只读使用)
const ES9018_State_t* ES9018_Get_State(void);

// 获取和设置配置 (替代直接访问全局变量NEW)
void ES9018_Get_Config(ES9018_Config_t *config);
void ES9018_Set_Config(const ES9018_Config_t *config);

// 核心控制接口
uint8_t ES9018_Init(void);
void ES9018_Deinit(void);
uint8_t ES9018_Soft_Reset(void);
uint8_t ES9018_Update_Register(void);  // 轮询调用：将Set_Config的变化写入硬件

// 常用功能快捷接口
uint8_t ES9018_Set_BitDepth(uint8_t BitDepth);    // 设置位深 16、24、32
uint8_t ES9018_Set_Volume(uint8_t ch1, uint8_t ch2); // 设置音量
uint8_t ES9018_Poll_Status(void);                 // 轮询更新GPIO和采样率状态

#endif
