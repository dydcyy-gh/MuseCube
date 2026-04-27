#include "stm32f4xx.h"
#include "variables.h"
#include "systick_conf.h"
#include "pin_ctrl.h"

void MAX98357_Init(void)
{
	Speaker_Power_Ctrl(1);
	Delay_ms(5);
	g_max98357_ststus = 1;
}

void MAX98357_Deinit(void)
{
	Speaker_Power_Ctrl(0);
	Delay_ms(5);
	g_max98357_ststus = 0;
}
