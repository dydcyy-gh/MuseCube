//注: IROM1 Start：0x08010000 Size：0xF0000 (1MB-64KB)

#include "main.h"

uint8_t init_res = 0;

TaskHandle_t    Start_Task_handler;
void Start_Task( void * pvParameters );

TaskHandle_t    Task_Manager_handler;
void Task_Manager( void * pvParameters );

//flashdb
struct fdb_kvdb kvdb = { 0 };
struct fdb_tsdb tsdb = { 0 };

int main(void)
{
	SCB->VTOR = 0x08010000; // 重定向中断向量表到 APP 起始地址
	__enable_irq();// 重新开启全局中断 (因为 Bootloader 跳转前把它关了)
	
	Nvic_Init();    //NVIC
	Systick_init(); //systick
	init_res = tlsf_init(); //mem
	
	if(init_res) while(1);
		
	//creat start task
    xTaskCreate((TaskFunction_t         )   Start_Task,
                (char *                 )   "Start_Task",
                (configSTACK_DEPTH_TYPE )   START_TASK_STACK_SIZE,
                (void *                 )   NULL,
                (UBaseType_t            )   START_TASK_PRIO,
                (TaskHandle_t *         )   &Start_Task_handler );
    vTaskStartScheduler();
}

void Start_Task( void * pvParameters )
{
	//creat Mutex,Semaphore
	xI2SSemaphore = xSemaphoreCreateBinary();
	xTaskManagerSemaphore = xSemaphoreCreateBinary();
	xFDBSemaphore = xSemaphoreCreateMutex();

	//key init
	Wakeup_Key_Init();
	Stick_Middle_Init();
	
	//pin
	Pin_Ctrl_Init();

	//rng
	RNG_Init();

	//adc
	ADC1_DMA_Init();
	
	//rtc time
	init_res = RTC_Clock_Init();

	//sdcard
	init_res = SD_Init();

	//w25q128
	init_res = W25QXX_Init();

	//fatfs
	init_res = fatfs_mount(DEV_SD);

	// FlashDB KVDB Init
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, (void *)fdb_lock);
    fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, (void *)fdb_unlock);
    init_res = fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", NULL, NULL);

    // FlashDB TSDB Init
    fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_LOCK, (void *)fdb_lock);
    fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_UNLOCK, (void *)fdb_unlock);
    init_res = fdb_tsdb_init(&tsdb, "log", "fdb_tsdb1", get_fdb_time, 128, NULL); 

	//font8
	if(font_init()) init_res = update_font((uint8_t*)"0:");
	
	if(init_res) while(1){} //error loop
		
	//creat task manager
    xTaskCreate(Task_Manager,"Task_Manager",TASK_MANAGER_STACK_SIZE,NULL,TASK_MANAGER_PRIO,&Task_Manager_handler );
				
	Taskmanager_Ctrl(Task_N_Basic, Task_T_Creat, 0);    //basic task
	Taskmanager_Ctrl(Task_N_LVGL, Task_T_Creat, 0);     //LVGL task
	Taskmanager_Ctrl(Task_N_Music, Task_T_Creat, 0);     //MUSIC task
	Taskmanager_Ctrl(Task_N_USB, Task_T_Creat, 0);      //usb task
				
	vTaskDelete(NULL);   //参数为NULL时，表示删除任务自身
}
