#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__

#include "stm32f4xx.h" 

void Start_Task(void *pvParameters);
void Task_Manager(void *pvParameters);
void Basic_Task(void *pvParameters);
void Lvgl_Task(void *pvParameters);
void USB_Task(void *pvParameters);
void Music_Task(void *pvParameters);
void Video_Task(void *pvParameters);
void Game_Task(void *pvParameters);

//task_num:0-Basic_Task  1-LVGL_Task  2-USB_Task  3-Music_Task
//task_action:1-Creat  3-Suspend  5-Resume  6-Delete
void Taskmanager_Ctrl(uint8_t task_num,uint8_t task_action,uint8_t IsFromISR);
	
#endif
