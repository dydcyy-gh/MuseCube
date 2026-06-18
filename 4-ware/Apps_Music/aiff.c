#include "stm32f4xx.h"                  // Device header
#include "aiff.h" 
#include "malloc.h"
#include "ff.h"
#include "i2s.h"
#include "string.h"
#include "math.h"
#include "music.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"
#include "file_unit.h"

// AIFF控制结构体
__aiffctrl aiffctrl;

// 安全的大端 32 位读取函数 (绝不会引发 HardFault)
static uint32_t get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// 安全的大端 16 位读取函数
static uint16_t get_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

// AIFF采用IEEE 754 80-bit 扩展精度浮点数存储采样率
static uint32_t aiff_read_80bit_float(uint8_t* p)
{
    uint16_t exp = get_be16(p) & 0x7FFF; // 提取15位指数
    uint32_t mantissa_hi = get_be32(p + 2); // 提取尾数高32位
    
    if (exp == 0 && mantissa_hi == 0) return 0;
    
    exp -= 16383; // 去除偏移量 Bias
    if (exp <= 31) {
        return mantissa_hi >> (31 - exp);
    }
    return 0;
}

// AIFF文件识别与头部解析 (使用 f_lseek 防越界寻址法)
uint8_t aiff_decode_init(uint8_t* fname, __aiffctrl* aiffx)
{
    FIL* ftemp = NULL;
    uint8_t header[24];
    uint32_t br = 0;
    uint8_t res = 0;

    ftemp = (FIL*)malloc_bsc(sizeof(FIL));
    if (!ftemp) return 1; 

    res = f_open(ftemp, (char*)fname, FA_READ);
    if (res != FR_OK) { free_bsc(ftemp); return 2; }

    res = f_read(ftemp, header, 12, &br); 
    if (res != FR_OK || br < 12) res = 3; 

    if (!res)
    {
        uint32_t form_id = get_be32(header);
        uint32_t form_size = get_be32(header + 4);
        uint32_t aiff_id = get_be32(header + 8);
        
        // 验证 FORM 以及 AIFF 或 AIFC
        if (form_id != AIFF_ID_FORM || (aiff_id != AIFF_ID_AIFF && aiff_id != AIFF_ID_AIFC)) {
            res = 4; // 非AIFF/AIFC格式
        } else {
            uint32_t remaining_size = form_size - 4; // 减去 'AIFF' 这4个字节
            uint8_t found_comm = 0;
            uint8_t found_ssnd = 0;
            
            // 块遍历逻辑：按块跳跃读取，不再怕区块越界
            while (remaining_size >= 8) 
            {
                res = f_read(ftemp, header, 8, &br); // 读取块头: ID 和 Size
                if (br < 8) break;

                uint32_t chunk_id = get_be32(header);
                uint32_t chunk_size = get_be32(header + 4);
                uint32_t padded_size = chunk_size + (chunk_size % 2 ? 1 : 0);
                
                // 【核心改进】：提前算好下一个块的绝对坐标，无论解析发生了什么都不怕指针跑飞
                uint32_t next_chunk_pos = ftemp->fptr + padded_size;

                if (chunk_id == AIFF_ID_COMM) 
                { 
                    // COMM 块至少 18 字节
                    res = f_read(ftemp, header, 18, &br); 
                    if (br == 18) {
                        // 【修正】：完全修复了偏移位置
                        aiffx->nchannels = get_be16(header + 0);               // 声道数
                        aiffx->bps = get_be16(header + 6);                     // 位深
                        aiffx->samplerate = aiff_read_80bit_float(header + 8); // 采样率
                        
                        aiffx->blockalign = aiffx->nchannels * (aiffx->bps / 8);
                        aiffx->bitrate = aiffx->samplerate * aiffx->nchannels * aiffx->bps;
                        
                        found_comm = 1;
                    }
                }
                else if (chunk_id == AIFF_ID_SSND) 
                { 
                    // SSND 块有 8 字节子头 (offset, blockSize)
                    res = f_read(ftemp, header, 8, &br); 
                    if (br == 8) {
                        uint32_t offset = get_be32(header);
                        
                        aiffx->datasize = chunk_size - 8 - offset; 
                        aiffx->datastart = ftemp->fptr + offset; // 数据真正的开始位置
                        
                        found_ssnd = 1;
                    }
                }
                
                // 若两个关键块都找到了就可以提前退出了
                if (found_comm && found_ssnd) break; 
                
                // 安全跳到下一个区块的开头
                f_lseek(ftemp, next_chunk_pos);
                
                if (remaining_size >= (8 + padded_size)) {
                    remaining_size -= (8 + padded_size);
                } else {
                    break;
                }
            }
            if (!found_comm || !found_ssnd) res = 8; // 缺少关键块
        }
    }
	
    f_close(ftemp);
    free_bsc(ftemp);
    return res;
}

// AIFF 填充 DMA 缓冲区 (解决大小端与指针转换导致的异常)
uint32_t aiff_buffill(uint8_t* buf, uint16_t size, uint8_t bits) 
{
    uint8_t res = 0;
    uint32_t bytes_read = 0;
    uint32_t read_size = 0;
    
    uint32_t *p32_out = (uint32_t *)buf; 
    uint8_t *p8_in = music_ctrl.tbuf;

    if (bits == 32) 
    {
        read_size = size;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);

        uint32_t samples = size / 4; 
		
        for (uint32_t i = 0; i < samples; i++) 
        {
            // 通过字节移位保证绝不触发访问异常
            int32_t sample = (int32_t)get_be32(p8_in); 
            p8_in += 4;
            
            if(kv_hdp0_or_spk1) 
            {
                int32_t scaled = (int32_t)(((int64_t)sample * kv_spk_value) >> 8);
                p32_out[i] = __ROR(scaled, 16); 
            }
            else
            {
                p32_out[i] = __ROR(sample, 16);
            }
        }
    }
    else if (bits == 24) 
    {
        read_size = size * 3 / 4;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);
        bytes_read = bytes_read * 4 / 3; 

        uint32_t samples = size / 4;

        if(kv_hdp0_or_spk1)
        {
            for(uint32_t i = 0; i < samples; i++)
            {
                // 大端序24位转为带符号的32位整形：p[0]=MSB, p[1]=MID, p[2]=LSB
                int32_t sample = (int32_t)((p8_in[0]<<24)|(p8_in[1]<<16)|(p8_in[2]<<8))>>8;
                int32_t scaled = (sample * kv_spk_value) >> 8;
                
                uint32_t val = ((scaled & 0xFF)<<24) | (((scaled>>16) & 0xFF) << 8) | ((scaled >> 8) & 0xFF);
                p32_out[i] = val;
                p8_in += 3;
            }
        }
        else
        {
            for(uint32_t i = 0; i < samples; i++)
            {
                uint32_t val = (p8_in[2] << 24) | (0 << 16) | (p8_in[0] << 8) | p8_in[1];
                p32_out[i] = val;
                p8_in += 3;
            }
        }
    } 
    else if (bits == 16) 
    {
        read_size = size;
        res = f_read(music_ctrl.file, music_ctrl.tbuf, read_size, (uint32_t*)&bytes_read);
        if (bytes_read < read_size) memset(music_ctrl.tbuf + bytes_read, 0, read_size - bytes_read);
        
        int16_t *p16_out = (int16_t *)buf; 
        uint32_t samples = size / 2;

        for (uint32_t i = 0; i < samples; i++) 
        {
            // 通过字节移位保证绝不触发访问异常
            int16_t raw_sample = (int16_t)get_be16(p8_in);
            p8_in += 2;
            
            if(kv_hdp0_or_spk1)
            {
                int32_t val = ((int32_t)raw_sample * kv_spk_value) >> 8;
                p16_out[i] = (int16_t)val;
            }
            else
            {
                p16_out[i] = raw_sample;
            }
        }
    } 
	
	Extract_FFT(buf);
	
    if(!res) return bytes_read;
    else return 0;
}

// 获取当前播放时间
void aiff_get_curtime(FIL* fx, __aiffctrl* aiffx)
{
    long long fpos;
    uint32_t byte_rate = aiffx->bitrate / 8; // 每秒字节数
    
    if(byte_rate == 0) return; 

    if(fx->fptr < aiffx->datastart) fpos = 0;
    else fpos = fx->fptr - aiffx->datastart;                   
    
    aiffx->cursec = (uint32_t)(fpos / byte_rate); 
    aiffx->totsec = aiffx->datasize / byte_rate; 
}


// AIFF 文件快进快退函数
uint32_t aiff_file_seek(uint32_t pos)
{
    uint32_t file_size = f_size(music_ctrl.file);
    uint32_t max_pos = aiffctrl.datastart + aiffctrl.datasize;
    
    if(max_pos > file_size) max_pos = file_size;
    if(pos > max_pos) pos = max_pos;
    if(pos < aiffctrl.datastart) pos = aiffctrl.datastart;

    if(aiffctrl.blockalign > 0) 
    {
        uint32_t data_offset = pos - aiffctrl.datastart;
        if(data_offset % aiffctrl.blockalign) 
        {
            pos -= (data_offset % aiffctrl.blockalign);  
        }
    }

    f_lseek(music_ctrl.file, pos);
    return f_tell(music_ctrl.file); 
}

// 播放某个 AIFF 文件准备阶段
uint8_t aiff_play_song_prepare(uint8_t* fname) 
{
    uint8_t res = 0;
    // 内存分配
    music_ctrl.file    = (FIL*)malloc_bsc(sizeof(FIL));
    music_ctrl.i2sbuf1 = malloc_bsc(AIFF_I2S_TX_DMA_BUFSIZE);
    music_ctrl.i2sbuf2 = malloc_bsc(AIFF_I2S_TX_DMA_BUFSIZE);
    music_ctrl.tbuf    = malloc_bsc(AIFF_I2S_TX_DMA_BUFSIZE);
	
    if (!music_ctrl.file || !music_ctrl.i2sbuf1 || !music_ctrl.i2sbuf2 || !music_ctrl.tbuf) res = 1;
	else memset(music_ctrl.file, 0, sizeof(FIL)); 
    
	if (!res) res = aiff_decode_init(fname, &aiffctrl); // 解析文件信息
    
	if (!res) // 解析成功
	{
        if (aiffctrl.bitrate > 0)
        {
            aiffctrl.totsec = aiffctrl.datasize / (aiffctrl.bitrate / 8);
        }
        else
        {
            aiffctrl.totsec = 0;
        }
        
        music_info.total_sec = aiffctrl.totsec;    // 总秒数
        music_info.bitrate = aiffctrl.bitrate;     // 比特率
        music_info.samplerate = aiffctrl.samplerate; // 采样率
        music_info.bit_depth = aiffctrl.bps;       // 位深
        music_info.current_sec = 0;                // 当前秒数清零

		// 配置 I2S
		if (aiffctrl.bps == 16) 
		{
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_16b);
			music_bitdepth = 16;
		}
		else if (aiffctrl.bps == 24) 
		{
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_24b);
			music_bitdepth = 32;
		}
		else if(aiffctrl.bps == 32)
		{	
			I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_32b);
			music_bitdepth = 32;
		}
		I2S2_SampleRate_Set(aiffctrl.samplerate); // 设置采样率
		
		I2S2_TX_DMA_Init(music_ctrl.i2sbuf1, music_ctrl.i2sbuf2, AIFF_I2S_TX_DMA_BUFSIZE/2); 

		I2S_Play_Stop();
		
		res = f_open(music_ctrl.file, (char*)fname, FA_READ); 
	}
	if (!res) 
	{
		f_lseek(music_ctrl.file, aiffctrl.datastart); // 跳过文件头
	}
	return res;
}

//播放任务逻辑
void aiff_play_song_task(uint8_t* fname)
{
	uint32_t read_bytes = 0;

	if(Music_Status == Song_Prepare)
	{
		if(aiff_play_song_prepare(fname)) Music_Status = Song_Next;
		else 
		{
			if(Music_Suspend_Flag)
			{
				memset(music_ctrl.i2sbuf1, 0, AIFF_I2S_TX_DMA_BUFSIZE);
				memset(music_ctrl.i2sbuf2, 0, AIFF_I2S_TX_DMA_BUFSIZE);
				Music_Status = Song_Playing;
			}
			else
			{
				read_bytes = aiff_buffill(music_ctrl.i2sbuf1, AIFF_I2S_TX_DMA_BUFSIZE, aiffctrl.bps);
				read_bytes = aiff_buffill(music_ctrl.i2sbuf2, AIFF_I2S_TX_DMA_BUFSIZE, aiffctrl.bps);
				if(read_bytes != AIFF_I2S_TX_DMA_BUFSIZE) {Music_Status = Song_End;}
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
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf2, 0, AIFF_I2S_TX_DMA_BUFSIZE);
			else read_bytes = aiff_buffill(music_ctrl.i2sbuf2, AIFF_I2S_TX_DMA_BUFSIZE, aiffctrl.bps);
		}
		else
		{
			if(Music_Suspend_Flag) memset(music_ctrl.i2sbuf1, 0, AIFF_I2S_TX_DMA_BUFSIZE);
			else read_bytes = aiff_buffill(music_ctrl.i2sbuf1, AIFF_I2S_TX_DMA_BUFSIZE, aiffctrl.bps);
		}
		if(read_bytes != AIFF_I2S_TX_DMA_BUFSIZE && !Music_Suspend_Flag) {Music_Status = Song_End;}
		
		aiff_get_curtime(music_ctrl.file, &aiffctrl);
        music_info.current_sec = aiffctrl.cursec;
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
