#include "stm32f4xx.h" 
#include "adc.h"
#include "variables.h"

typedef struct {
    float voltage;   // 单体电压 (V)
    int8_t soc;      // 剩余电量 (%)
} VoltageSocPoint;

static const VoltageSocPoint vTable[] = {
    {4.20f, 100},{4.12f,  95},{4.08f,  90},{4.03f,  85},
    {3.97f,  80},{3.93f,  75},{3.90f,  70},{3.87f,  65},
    {3.84f,  60},{3.81f,  55},{3.79f,  50},{3.77f,  45},
    {3.76f,  40},{3.74f,  35},{3.73f,  30},{3.72f,  25},
    {3.71f,  20},{3.69f,  15},{3.65f,  10},{3.63f,   5},
    {3.58f,   0},{3.50f,  -5},{3.30f, -10},
};

static const uint16_t Size = (uint16_t)(sizeof(vTable) / sizeof(vTable[0]));

//3.7V 单体锂电池 电压 -> 剩余电量(%)
void VoltageToPercent(void)
{
	float v = g_battery_voltage;
	
    // 边界
    if (v >= vTable[0].voltage) 
	{
        g_battery_percent = vTable[0].soc;
		return;
    }
    if (v <= vTable[Size - 1].voltage) 
	{
		g_battery_percent = vTable[Size - 1].soc;
		return;
    }

    // 二分查找（降序数组）
    int left = 0;
    int right = Size - 1;

    while (left <= right) 
	{
        int mid = (left + right) >> 1;
        float vmid = vTable[mid].voltage;

        if (v == vmid) 
		{
            g_battery_percent = vTable[mid].soc;
			return;
        } 
		else if (v < vmid) left = mid + 1;
        else right = mid - 1;
    }

    if (left < 0) left = 0;
    if (left >= Size) left = Size - 1;

    g_battery_percent = vTable[left].soc;
}
