#include "stm32f4xx.h"
#include "ogg.h" 
#include "malloc.h"
#include "ff.h"
#include "i2s.h"
#include "string.h"
#include "music.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"
#include "file_unit.h"

#include "stb_vorbis.h"

// 控制参数
__oggctrl oggctrl;

// stb_vorbis 的静态安全内存池 (85KB, FH=6 优化 + blocksize=2048 够用)
#define OGG_MEMORY_SIZE (70 * 1024)
uint8_t* ogg_mem_pool = NULL;
stb_vorbis_alloc ogg_alloc;

static volatile uint32_t read_bytes = 0;

// ==================== 解码与缓冲填充 ====================

uint32_t ogg_buffill(uint8_t* buf, uint16_t size) 
{
	int16_t *p16_out = (int16_t *)buf; 
	uint32_t shorts_to_read = size / 2; // shorts 数量
	
	// 调用 stb_vorbis 解码, 强制输出双声道以匹配 I2S 硬件配置
	// 无论原文件是单声道还是双声道，始终输出 2 声道交错 PCM
	int samples = stb_vorbis_get_samples_short_interleaved(oggctrl.stb_vf, 2, p16_out, shorts_to_read);

	// 转换为总读取字节数 (固定按 2 声道计算)
	uint32_t bytes_read = samples * 2 * 2;
	
	// 若数据不足(通常是文件尾)，补零以防杂音
	if (bytes_read < size) {
		memset(buf + bytes_read, 0, size - bytes_read);
	}
	
	// 处理音量缩放 (原地处理)
	if(kv_hdp0_or_spk1)
	{
		uint32_t total_samples = size / 2;
		for (uint32_t i = 0; i < total_samples; i++) 
		{
			int32_t val = ((int32_t)p16_out[i] * kv_spk_value) >> 8;
			p16_out[i] = (int16_t)val;
		}
	}
	
	Extract_FFT(buf); // 频谱提取
	return bytes_read;
}

// ==================== 播放控制 ====================

// OGG 文件快进快退函数
void ogg_file_seek(uint32_t target_sec)
{
	if(!oggctrl.stb_vf) return;
	if(target_sec >= oggctrl.totsec) target_sec = oggctrl.totsec - 1;
	
	// stb_vorbis 接受绝对采样帧数进行快进
	uint32_t target_sample = target_sec * oggctrl.samplerate;
	stb_vorbis_seek(oggctrl.stb_vf, target_sample); 
}

// 播放准备阶段
uint8_t ogg_play_song_prepare(uint8_t* fname)
{
	uint8_t res = 0;
	int error = 0;
	
	// 1. 分配空间 (砍掉了旧版 tbuf)
	music_ctrl.file    = (FIL*)malloc_bsc(sizeof(FIL));
	music_ctrl.i2sbuf1 = malloc_bsc(OGG_I2S_TX_DMA_BUFSIZE);
	music_ctrl.i2sbuf2 = malloc_bsc(OGG_I2S_TX_DMA_BUFSIZE);
	ogg_mem_pool       = malloc_bsc(OGG_MEMORY_SIZE); 
	music_ctrl.tbuf    = NULL; 
	
	if (!music_ctrl.file || !music_ctrl.i2sbuf1 || !music_ctrl.i2sbuf2 || !ogg_mem_pool) {
		return 1;
	}
	
	memset(music_ctrl.file, 0, sizeof(FIL));
	res = f_open(music_ctrl.file, (char*)fname, FA_READ);
	if (res != FR_OK) return 2;
	
	// 2. 配置专属内存储备给解码器
	ogg_alloc.alloc_buffer = (char *)ogg_mem_pool;
	ogg_alloc.alloc_buffer_length_in_bytes = OGG_MEMORY_SIZE;
	
	// 3. 打开文件开始解码
	oggctrl.stb_vf = stb_vorbis_open_file(music_ctrl.file, 0, &error, &ogg_alloc);
	if (!oggctrl.stb_vf) {
		return 3;
	}
	
	stb_vorbis_info info = stb_vorbis_get_info(oggctrl.stb_vf);
	oggctrl.nchannels = info.channels;
	oggctrl.samplerate = info.sample_rate;
	oggctrl.bps = 16;
	oggctrl.totsec = stb_vorbis_stream_length_in_seconds(oggctrl.stb_vf);
	
	// 同步到 UI 与总控制
	music_info.total_sec = oggctrl.totsec;
	music_info.bitrate = 112000; // VBR参考值，按需修改
	music_info.samplerate = oggctrl.samplerate;
	music_info.bit_depth = oggctrl.bps;
	music_info.current_sec = 0;
	
	// 4. 开启 DMA 传输
	I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
	music_bitdepth = 16;
	if(I2S2_SampleRate_Set(oggctrl.samplerate)) {
		return 4;
	}
	I2S2_TX_DMA_Init(music_ctrl.i2sbuf1, music_ctrl.i2sbuf2, OGG_I2S_TX_DMA_BUFSIZE / 2);
	I2S_Play_Stop();
	
	return 0;
}

// 播放任务逻辑
void ogg_play_song_task(uint8_t* fname)
{
	if(Music_Status == Song_Prepare)
	{
		if(ogg_play_song_prepare(fname)) 
		{
			Music_Status = Song_Next; 
		}
		else 
		{
			if(Music_Suspend_Flag)
			{
				memset(music_ctrl.i2sbuf1, 0, OGG_I2S_TX_DMA_BUFSIZE);
				memset(music_ctrl.i2sbuf2, 0, OGG_I2S_TX_DMA_BUFSIZE);
				Music_Status = Song_Playing;
			}
			else
			{
				read_bytes = ogg_buffill(music_ctrl.i2sbuf1, OGG_I2S_TX_DMA_BUFSIZE);
				read_bytes = ogg_buffill(music_ctrl.i2sbuf2, OGG_I2S_TX_DMA_BUFSIZE);
				if(read_bytes != OGG_I2S_TX_DMA_BUFSIZE) {Music_Status = Song_End;}
				else {Music_Status = Song_Playing;}
			}
			if(Music_Status == Song_Playing) I2S_Play_Start();
		}
	}
	else if(Music_Status == Song_Playing)
	{
		xSemaphoreTake(xI2SSemaphore, portMAX_DELAY); // 等待一帧传输完成
		
		if (I2SdmaBuff)
		{
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf2, 0, OGG_I2S_TX_DMA_BUFSIZE);
			else read_bytes = ogg_buffill(music_ctrl.i2sbuf2, OGG_I2S_TX_DMA_BUFSIZE);
		}
		else
		{
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf1, 0, OGG_I2S_TX_DMA_BUFSIZE);
			else read_bytes = ogg_buffill(music_ctrl.i2sbuf1, OGG_I2S_TX_DMA_BUFSIZE);
		}
		
		if(read_bytes != OGG_I2S_TX_DMA_BUFSIZE && !Music_Suspend_Flag) {Music_Status = Song_End;}
		
		// 更新当前时间
		if (oggctrl.stb_vf) {
			oggctrl.cursec = stb_vorbis_get_sample_offset(oggctrl.stb_vf) / oggctrl.samplerate;
			music_info.current_sec = oggctrl.cursec;
		}
	}
	else
	{
		I2S_Play_Stop();
		
		// 释放解码器内部信息
		if(oggctrl.stb_vf) {
			stb_vorbis_close(oggctrl.stb_vf); 
			oggctrl.stb_vf = NULL;
		}
		
		// 释放 FatFs 句柄
		if (music_ctrl.file) {
			f_close(music_ctrl.file); 
			free_bsc(music_ctrl.file);
			music_ctrl.file = NULL;
		}
		
		// 释放我们自己配给 stb_vorbis 的静态池
		if (ogg_mem_pool) {
			free_bsc(ogg_mem_pool);
			ogg_mem_pool = NULL;
		}
		
		if (music_ctrl.i2sbuf1) {
			free_bsc(music_ctrl.i2sbuf1);
			music_ctrl.i2sbuf1 = NULL;
		}
		
		if (music_ctrl.i2sbuf2) {
			free_bsc(music_ctrl.i2sbuf2);
			music_ctrl.i2sbuf2 = NULL;
		}
		
		if(Music_Status == Song_Error) Music_Status = Music_Exit;
		if(Music_Status == Song_End) 
		{
			if(kv_music_switch_method == Play_In_Order) play_next_song();
			if(kv_music_switch_method == Play_Randomly) play_random_song();
			if(kv_music_switch_method == Play_Repeatly) play_same_song();
			Music_Status = Song_Prepare;
		}
		if(Music_Status == Song_Next)  
		{
			play_next_song();
			Music_Status = Song_Prepare;
		}
		if(Music_Status == Song_Previous)  
		{
			play_previous_song();
			Music_Status = Song_Prepare;
		}
		if(Music_Status == Song_File)
		{
			play_specific_song(chosen_file_path); 
			chosen_file_path_free();
			Music_Status = Song_Prepare;
		}
	}
}

