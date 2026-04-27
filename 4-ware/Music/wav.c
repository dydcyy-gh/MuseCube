#include "stm32f4xx.h"                  // Device header
#include "wav.h" 
#include "malloc.h"
#include "ff.h"
#include "i2s.h"
#include "string.h"
#include "math.h"
#include "wav.h"
#include "music.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"

//music二进制信号量
extern SemaphoreHandle_t xI2SSemaphore;// I2S 传输完成标志

// WAV控制结构体
__wavctrl wavctrl;

//wav文件识别
uint8_t wav_decode_init(uint8_t* fname, __wavctrl* wavx)
{
    FIL* ftemp = NULL;
    uint8_t* buf = NULL;
    uint32_t br = 0;
    uint8_t res = 0;
    uint32_t data_start_offset = 0;

    // 分配内存
    ftemp = (FIL*)malloc_bsc(sizeof(FIL));
    buf = (uint8_t*)malloc_bsc(512);

    // 内存分配检查
    if (!ftemp || !buf) res = 1; // 内存分配失败

    // 打开文件
    if (!res)
    {
        res = f_open(ftemp, (char*)fname, FA_READ);
        if (res != FR_OK) res = 2; // 文件打开失败
    }

    // 读取文件头
    if (!res)
    {
        res = f_read(ftemp, buf, 512, &br); 
        if (res != FR_OK || br < 512) res = 3; 
    }

    // RIFF格式验证
    if (!res)
    {
        ChunkRIFF* riff = (ChunkRIFF*)buf;
        if (riff->Format != 0x45564157) res = 4; // 非WAV格式

        uint8_t* chunk_ptr = buf + 12;
        uint32_t remaining_size = riff->ChunkSize - 4;

        ChunkFMT* fmt = NULL;
        ChunkDATA* data = NULL;
		
        // 块遍历逻辑
        while (!res && remaining_size >= 8) 
        {
            if ((chunk_ptr + 8) > (buf + 512)) // 缓冲区边界检查
            {
                res = 5; // 越界访问
                break;
            }

            uint32_t chunk_id = *(uint32_t*)chunk_ptr;
            uint32_t chunk_size = *(uint32_t*)(chunk_ptr + 4);

            // 校验块尺寸合法性
            if (chunk_size + 8 > remaining_size) 
            {
                res = 6;
                break;
            }

            uint32_t padded_size = chunk_size + (chunk_size % 2 ? 1 : 0);

            /* 修改点1: 仅对非数据块进行越界检查 */
            if (chunk_id != 0x61746164) // 非"data"块时检查
            {
                if ((chunk_ptr + 8 + padded_size) > (buf + 512))
                {
                    res = 7;
                    break;
                }
            }

            // 处理目标块
            if (chunk_id == 0x20746D66) // "fmt "
            { 
                fmt = (ChunkFMT*)chunk_ptr;
            }
            else if (chunk_id == 0x61746164) // "data"
            { 
                data = (ChunkDATA*)chunk_ptr;
                data_start_offset = (chunk_ptr - buf) + 8;
            }
            
            if (fmt && data) break; // 找到关键块后提前退出
            
            // 移动到下一个块
            chunk_ptr += 8 + padded_size;
            remaining_size -= 8 + padded_size;
        }

        if (!res)
        {
            if (!fmt || !data) res = 8; // 缺少关键块
        }
        
        if (!res)
        {
			// 填充控制结构体
			wavx->audioformat = fmt->AudioFormat;
			wavx->nchannels = fmt->NumOfChannels;
			wavx->samplerate = fmt->SampleRate;
			wavx->bitrate = fmt->ByteRate * 8;
			wavx->blockalign = fmt->BlockAlign;
			wavx->bps = fmt->BitsPerSample;
			wavx->datasize = data->ChunkSize;
			wavx->datastart = data_start_offset;
        }
    }
	
    // 清理资源
    if (res > 2 || res == 0) f_close(ftemp);
    if (ftemp) free_bsc(ftemp);
    if (buf) free_bsc(buf);
	
    return res;
}

//wav填充dma缓冲区
uint32_t wav_buffill(uint8_t* buf, uint16_t size, uint8_t bits) 
{
    uint8_t res = 0;
    uint32_t bytes_read = 0;
    uint32_t read_size = 0;
    
    // 指针预处理
    uint32_t *p32_out = (uint32_t *)buf; 
    uint8_t *p_in_base;

    if (bits == 32) 
    {
        read_size = size;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);

        uint32_t *p32_in = (uint32_t *)music_ctrl.tbuf;
        uint32_t samples = size / 4; 
		
        if(g_hdp0_or_spk1) 
        {
            for (uint32_t i = 0; i < samples; i++) 
            {
                int32_t sample = p32_in[i]; // 这里的隐式转换是安全的
                int32_t scaled = (int32_t)(((int64_t)sample * g_spk_value) >> 8);
                p32_out[i] = __ROR(scaled, 16); 
            }
        }
        else
        {
            for (uint32_t i = 0; i < samples; i++) 
            {
                p32_out[i] = __ROR(p32_in[i], 16);
            }
        }
    }
    else if (bits == 24) 
    {
        read_size = size * 3 / 4;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);
        bytes_read = bytes_read * 4 / 3; 

        p_in_base = music_ctrl.tbuf;
        uint32_t samples = size / 4;

        if(g_hdp0_or_spk1)
        {
            for(uint32_t i = 0; i < samples; i++)
            {
                int32_t sample = (int32_t)((p_in_base[0]<<8)|(p_in_base[1]<<16)|(p_in_base[2]<<24))>>8;
                int32_t scaled = (sample * g_spk_value) >> 8;
                uint32_t val = ((scaled & 0xFF)<<24) | (((scaled>>16) & 0xFF) << 8) | ((scaled >> 8) & 0xFF);
                p32_out[i] = val;
                p_in_base += 3;
            }
        }
        else
        {
            for(uint32_t i = 0; i < samples; i++)
            {
                uint32_t val = (p_in_base[0] << 24) | (0 << 16) | (p_in_base[2] << 8) | p_in_base[1];
                p32_out[i] = val;
                p_in_base += 3;
            }
        }
    } 
    else if (bits == 16) 
    {
        read_size = size;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);
        
        int16_t *p16_in = (int16_t *)music_ctrl.tbuf;  
        int16_t *p16_out = (int16_t *)buf; // 输出指针也通常是对齐的，可以直接强转
        
        uint32_t samples = size / 2;

        if(g_hdp0_or_spk1)
        {
            for (uint32_t i = 0; i < samples; i++) 
            {
                int32_t val = ((int32_t)p16_in[i] * g_spk_value) >> 8;
                p16_out[i] = (int16_t)val;
            }
        }
        else
        {
            memcpy(buf, music_ctrl.tbuf, read_size);
        }
    } 
	
	Extract_FFT(buf);
	
    if(!res) return bytes_read;
    else return 0;
}

// 获取当前播放时间
void wav_get_curtime(FIL* fx, __wavctrl* wavx)
{
    long long fpos;
    wavx->totsec = wavx->datasize / (wavx->bitrate / 8); // 总时长（秒）
    fpos = fx->fptr - wavx->datastart;                   // 当前文件播放位置
    wavx->cursec = fpos * wavx->totsec / wavx->datasize; // 当前播放时间（秒）
}

// WAV文件快进快退函数
// pos: 需要定位到的文件位置
// 返回值: 实际定位到的文件位置
uint32_t wav_file_seek(uint32_t pos)
{
    uint8_t temp = 0;
    uint32_t file_size = f_size(music_ctrl.file);
    
    // 边界保护
    if(pos > file_size) pos = file_size;
    if(pos < wavctrl.datastart) pos = wavctrl.datastart;

    // 根据位深设置对齐步长
    if(wavctrl.bps == 16)       temp = 8;   // 16bit 2ch×2bytes=4bytes/采样 → 对齐步长8字节（2采样）
    else if(wavctrl.bps == 24)  temp = 12;  // 24bit 2ch×3bytes=6bytes/采样 → 对齐步长12字节（2采样）
    else if(wavctrl.bps == 32)  temp = 8;   // 32bit 2ch×4bytes=8bytes/采样 → 对齐步长8字节（1采样）

    // 对齐修正（确保定位到完整采样点）
    if(temp > 0) 
    {
        uint32_t data_offset = pos - wavctrl.datastart;
        if(data_offset % temp) 
        {
            pos += temp - (data_offset % temp);  // 向上对齐到最近的有效位置
            if(pos > file_size) pos = file_size; // 二次边界校验
        }
    }

    f_lseek(music_ctrl.file, pos);
    return f_tell(music_ctrl.file);  // 使用标准API获取当前偏移
}

// 播放某个 WAV 文件
uint8_t wav_play_song_prepare(uint8_t* fname) //切歌后执行一次
{
    uint8_t res = 0;
    // 内存分配
    music_ctrl.file    = (FIL*)malloc_bsc(sizeof(FIL));
    music_ctrl.i2sbuf1 = malloc_bsc(WAV_I2S_TX_DMA_BUFSIZE);
    music_ctrl.i2sbuf2 = malloc_bsc(WAV_I2S_TX_DMA_BUFSIZE);
    music_ctrl.tbuf    = malloc_bsc(WAV_I2S_TX_DMA_BUFSIZE);
	
    if (!music_ctrl.file || !music_ctrl.i2sbuf1 || !music_ctrl.i2sbuf2 || !music_ctrl.tbuf) res = 1;
	else memset(music_ctrl.file, 0, sizeof(FIL)); //初始化文件结构体为0，防止 cleanup 时 f_close 访问垃圾指针
	if (!res) res = wav_decode_init(fname, &wavctrl); // 解析文件信息
	if (!res) // 解析成功
	{
        if (wavctrl.bitrate > 0)
        {
            wavctrl.totsec = wavctrl.datasize / (wavctrl.bitrate / 8);
        }
        else
        {
            wavctrl.totsec = 0;
        }
        
        music_info.total_sec = wavctrl.totsec;    // 总秒数
        music_info.bitrate = wavctrl.bitrate;     // 比特率
        music_info.samplerate = wavctrl.samplerate; // 采样率
        music_info.bit_depth = wavctrl.bps;       // 位深
        music_info.current_sec = 0;               // 当前秒数清零

		// 配置 I2S
		if (wavctrl.bps == 16) 
		{
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
			music_bitdepth = 16;
		}
		else if (wavctrl.bps == 24) 
		{
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_24b);
			music_bitdepth = 32;
		}
		else if(wavctrl.bps == 32)
		{	
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_32b);
			music_bitdepth = 32;
		}
		I2S2_SampleRate_Set(wavctrl.samplerate); // 设置采样率
		
		I2S2_TX_DMA_Init(music_ctrl.i2sbuf1, music_ctrl.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE/2); 
		//因为dma是16bit的，虽然MUSIC数据是32bit，num数量还是4096个

		I2S_Play_Stop();
		
		res = f_open(music_ctrl.file, (char*)fname, FA_READ); // 打开文件
	}
	if (!res) 
	{
		f_lseek(music_ctrl.file, wavctrl.datastart); // 跳过文件头
	}
	return res;
}

//播放某个文件
void wav_play_song_task(uint8_t* fname)
{
	uint32_t read_bytes = 0;

	if(Music_Status == Song_Prepare)
	{
		if(wav_play_song_prepare(fname)) Music_Status = Song_Next;
		else 
		{
			if(Music_Suspend_Flag)
			{
				memset(music_ctrl.i2sbuf1, 0, WAV_I2S_TX_DMA_BUFSIZE);
				memset(music_ctrl.i2sbuf2, 0, WAV_I2S_TX_DMA_BUFSIZE);
				Music_Status = Song_Playing;
			}
			else
			{
				read_bytes = wav_buffill(music_ctrl.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
				read_bytes = wav_buffill(music_ctrl.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
				if(read_bytes != WAV_I2S_TX_DMA_BUFSIZE) {Music_Status = Song_End;}
				else {Music_Status = Song_Playing;}
			}
			if(Music_Status == Song_Playing) I2S_Play_Start();
		}
	}
	else if(Music_Status == Song_Playing)
	{
		xSemaphoreTake(xI2SSemaphore, portMAX_DELAY);//传输完成
		if (I2SdmaBuff)
		{
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf2, 0, WAV_I2S_TX_DMA_BUFSIZE);
			else read_bytes = wav_buffill(music_ctrl.i2sbuf2, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
		}
		else
		{
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf1, 0, WAV_I2S_TX_DMA_BUFSIZE);
			else read_bytes = wav_buffill(music_ctrl.i2sbuf1, WAV_I2S_TX_DMA_BUFSIZE, wavctrl.bps);
		}
		if(read_bytes != WAV_I2S_TX_DMA_BUFSIZE && !Music_Suspend_Flag) {Music_Status = Song_End;}
		
		wav_get_curtime(music_ctrl.file,&wavctrl);
        music_info.current_sec = wavctrl.cursec;
	}
	else
	{
		I2S_Play_Stop();
		
		if (music_ctrl.file) {
            f_close(music_ctrl.file); 
            free_bsc(music_ctrl.file);
            music_ctrl.file = NULL;
        }

        if (music_ctrl.tbuf) {
            free_bsc(music_ctrl.tbuf);
            music_ctrl.tbuf = NULL;
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
			if(Music_Switch_Method == Play_In_Order) play_next_song();
			if(Music_Switch_Method == Play_Randomly) play_random_song();
			if(Music_Switch_Method == Play_Repeatly) play_same_song();
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
			Music_Status = Song_Prepare;
		}
	}
}
