#include "stm32f4xx.h"
#include "flac.h"
#include "i2s.h"
#include "malloc.h"
#include "systick_conf.h"
#include "music.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "math.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"
#include "file_unit.h"

__flacctrl * flacctrl;	//flac解码控制结构体

//分析FLAC文件 - 使用foxen-flac库
uint8_t flac_init(FIL* fx, __flacctrl* fctrl)
{
    uint8_t res = 0;
    uint8_t *buf;
    uint32_t br;
    
    buf = malloc_bsc(1024);
    if(!buf) return 1;

    f_lseek(fx, 0);
    
    // 创建foxen-flac解码器
    fctrl->decoder = FX_FLAC_ALLOC_SUBSET_FORMAT_DAT();
    if(!fctrl->decoder) 
	{
        free_bsc(buf);
        return 3;
    }
    
    // 读取并处理元数据
    uint32_t total_read = 0;
    uint8_t metadata_done = 0;
    uint32_t in_buf_wr_cur = 0;
	
    while(!metadata_done && total_read < 65536) 
	{
        uint32_t to_read = 1024 - in_buf_wr_cur;  // 只读剩余空间
        f_read(fx, buf + in_buf_wr_cur, to_read, &br);
        if(br == 0) break;
        
        uint32_t in_len = in_buf_wr_cur + br;
        
        fx_flac_state_t state = fx_flac_process(fctrl->decoder, buf, &in_len, NULL, NULL);
        
        if(state > FLAC_END_OF_METADATA) metadata_done = 1;
		if(state == FLAC_ERR) {res = 4;break;}
        
        uint32_t remaining = (in_buf_wr_cur + br) - in_len;
        memmove(buf, buf + in_len, remaining);
        in_buf_wr_cur = remaining;
        
        total_read += br;
    }
    
    if(metadata_done) 
	{
        // 获取流信息
        fctrl->samplerate = fx_flac_get_streaminfo(fctrl->decoder, FLAC_KEY_SAMPLE_RATE);
        fctrl->nchannels = fx_flac_get_streaminfo(fctrl->decoder, FLAC_KEY_N_CHANNELS);
        fctrl->bps = fx_flac_get_streaminfo(fctrl->decoder, FLAC_KEY_SAMPLE_SIZE);
        uint64_t total_samples = fx_flac_get_streaminfo(fctrl->decoder, FLAC_KEY_N_SAMPLES);
        uint32_t max_block_size = fx_flac_get_streaminfo(fctrl->decoder, FLAC_KEY_MAX_BLOCK_SIZE);
        
        fctrl->totsec = total_samples / fctrl->samplerate;
        fctrl->datastart = fx->fptr - in_buf_wr_cur;
        
        // 计算比特率
        uint32_t file_size = f_size(fx);
        fctrl->bitrate = ((file_size - fctrl->datastart) * 8) / fctrl->totsec;
		
		f_lseek(fx,fctrl->datastart);
    } 
	else res = 5;
	
    free_bsc(buf);
    return res;
}

#define IN_BUF_SIZE 8*1024
#define OUT_BUF_SIZE 16*1024

uint8_t* flac_in_buffer=0;  

uint32_t in_buf_byte_left;
uint32_t in_buf_offset = 0;
uint32_t out_buf_byte_left;
uint32_t br=0; 
  
uint32_t flac_fptr=0; 

//得到当前播放时间
//fx:文件指针
void flac_get_curtime(FIL* fx, __flacctrl *flacctrl)
{
    if(fx->fptr > flacctrl->datastart) 
	{
        uint64_t fpos = fx->fptr - flacctrl->datastart;
        uint64_t total_size = f_size(fx) - flacctrl->datastart;
        flacctrl->cursec = (fpos * flacctrl->totsec) / total_size;
    } 
	else 
        flacctrl->cursec = 0;
}

//flac文件快进快退函数
uint32_t flac_file_seek(uint32_t pos)
{
    uint32_t file_size = f_size(music_ctrl.file);
    if(pos > file_size) pos = file_size;
    if(pos < flacctrl->datastart) pos = flacctrl->datastart;

    f_lseek(music_ctrl.file, pos);

    // 强制清空 foxen-flac 内部的 bitstream 缓存
    fx_flac_flush(flacctrl->decoder);

    in_buf_byte_left = 0;
    in_buf_offset = 0;    // 强制复位缓冲游标
    // 强制触发 Task 内的文件全量重读机制
    flac_fptr = 0xFFFFFFFF;
    
    return music_ctrl.file->fptr;
}

uint8_t flac_play_song_prepare(uint8_t* fname)
{ 
    uint8_t res = 0; 
    
    flacctrl = malloc_bsc(sizeof(__flacctrl));
    music_ctrl.file = (FIL*)malloc_bsc(sizeof(FIL));
    
    if(!music_ctrl.file || !flacctrl) res = 1; // 内存申请错误

	if(!res) {
        memset(flacctrl, 0, sizeof(__flacctrl));
        res = f_open(music_ctrl.file, (char*)fname, FA_READ);
	}
	
	if(!res) {
		res = flac_init(music_ctrl.file, flacctrl);//初始化
	}
	
	if(!res) {
		music_ctrl.i2sbuf1 = malloc_bsc(OUT_BUF_SIZE);
		music_ctrl.i2sbuf2 = malloc_bsc(OUT_BUF_SIZE);
		flac_in_buffer = malloc_bsc(IN_BUF_SIZE);
		
		I2S2_Init(I2S_Standard_Phillips, I2S_Mode_MasterTx, I2S_CPOL_Low, I2S_DataFormat_32b);

        music_info.total_sec = flacctrl->totsec;    // 总秒数
        music_info.bitrate = flacctrl->bitrate;     // 比特率
        music_info.samplerate = flacctrl->samplerate; // 采样率
        music_info.bit_depth = flacctrl->bps;       // 位深
        music_info.current_sec = 0;                 // 当前秒数清零

		if(music_ctrl.i2sbuf1 && music_ctrl.i2sbuf2 && flac_in_buffer) 
		{
			// 初始化缓冲区
			memset(music_ctrl.i2sbuf1, 0, OUT_BUF_SIZE);
			memset(music_ctrl.i2sbuf2, 0, OUT_BUF_SIZE);
			
			// 配置DMA和I2S
			I2S2_TX_DMA_Init(music_ctrl.i2sbuf1, music_ctrl.i2sbuf2, OUT_BUF_SIZE/2);//dma是2byte(16bit)
			
			I2S2_SampleRate_Set(flacctrl->samplerate);

			// 读取初始数据
			f_read(music_ctrl.file, flac_in_buffer, IN_BUF_SIZE, &br);
			in_buf_byte_left = br;
            in_buf_offset = 0;  // 复位缓冲游标
			flac_fptr = music_ctrl.file->fptr;
			
			I2S_Play_Start();
		} 
		else res = 2; // 缓冲区分配失败
    }
    return res;
}

void flac_play_song_task(uint8_t* fname)
{
    uint8_t* target_buf = NULL;
    
    if(Music_Status == Song_Prepare) 
	{
        uint8_t res = flac_play_song_prepare(fname);
        if(res) Music_Status = Song_End;
        else Music_Status = Song_Playing;
    }
    else if(Music_Status == Song_Playing) 
	{
        xSemaphoreTake(xI2SSemaphore, portMAX_DELAY);
        
        if(I2SdmaBuff == 0) target_buf = music_ctrl.i2sbuf1;
        else target_buf = music_ctrl.i2sbuf2;
        
		if(Music_Suspend_Flag) 
		{
			memset(target_buf, 0, OUT_BUF_SIZE);
		}
		else 
		{
			// 处理文件定位变化
			if(flac_fptr != music_ctrl.file->fptr) 
			{
				if(music_ctrl.file->fptr < flacctrl->datastart) {
					f_lseek(music_ctrl.file, flacctrl->datastart);
				}
				f_read(music_ctrl.file, flac_in_buffer, IN_BUF_SIZE, &br);
				in_buf_byte_left = br;
                in_buf_offset = 0;  // 复位缓冲游标
				flac_fptr = music_ctrl.file->fptr;
			}
			
			// 使用foxen-flac解码
			if(flacctrl->nchannels == 1) out_buf_byte_left = OUT_BUF_SIZE/2;
			else out_buf_byte_left = OUT_BUF_SIZE;
		
			uint8_t *out_ptr = target_buf;  // 添加指针来跟踪当前输出位置

			while(out_buf_byte_left)
			{
				// 当数据余量不到一半时，集中大块填满，减少碎片化耗时
				if(in_buf_byte_left < IN_BUF_SIZE/2) 
				{
                    // 先把剩下未处理的有效数据挪到缓冲区头部
					if(in_buf_byte_left > 0 && in_buf_offset > 0) {
						memmove(flac_in_buffer, flac_in_buffer + in_buf_offset, in_buf_byte_left);
					}
					in_buf_offset = 0; // 重置游标

                    // 一次性把缓存给全部塞满，减少SD卡的极低效碎片调用
					f_read(music_ctrl.file, flac_in_buffer + in_buf_byte_left, IN_BUF_SIZE - in_buf_byte_left, &br);
					in_buf_byte_left += br;
				}
				// 没有数据了
				if(!in_buf_byte_left) {Music_Status = Song_End; break;}
				
				// 传给解码器的是 flac_in_buffer + in_buf_offset
				uint32_t in_len = in_buf_byte_left;
				uint32_t out_len = out_buf_byte_left / 4;
				int32_t* temp_buf = (int32_t*)out_ptr;
				
				fx_flac_state_t state = fx_flac_process(flacctrl->decoder, flac_in_buffer + in_buf_offset, &in_len, temp_buf, &out_len);
				
				// 出错
				if(state == FLAC_ERR) {Music_Status = Song_End; break;}
                
				// 读取到了数据
				if(out_len > 0) 
				{
					if(flacctrl->nchannels == 1)
					{
						int32_t* src = temp_buf + out_len - 1;
						int32_t* dst = temp_buf + out_len * 2 - 1;
						
						for(uint32_t i = 0; i < out_len; i++)
						{
							int32_t sample = *src;
                            
                            if(kv_hdp0_or_spk1) {
                                sample = (int32_t)(((int64_t)sample * kv_spk_value) >> 8);
                            }
                            
							int32_t ex_sample = __ROR(sample, 16);
                            
							*dst-- = ex_sample;
							*dst-- = ex_sample;
							src--;
						}
						out_ptr += out_len * 8;
					}
					else
					{
						uint32_t* p = (uint32_t*)temp_buf;
						for(uint32_t i = 0; i < out_len; i++)
						{
                            int32_t sample = p[i];
                            
                            if(kv_hdp0_or_spk1) {
                                sample = (int32_t)(((int64_t)sample * kv_spk_value) >> 8);
                            }
                            
                            p[i] = __ROR(sample, 16);
						}
						out_ptr += out_len * 4;
					}
					out_buf_byte_left -= out_len * 4;
				}
				
				// 更新游标指针和剩余量，绝对不在此处进行 memmove！
				if(in_len <= in_buf_byte_left)
				{
                    in_buf_offset += in_len;
					in_buf_byte_left -= in_len;
				}
				else in_buf_byte_left = 0;
			}

			flac_get_curtime(music_ctrl.file, flacctrl);
			flac_fptr = music_ctrl.file->fptr;
            music_info.current_sec = flacctrl->cursec; 
        }
		Extract_FFT(target_buf);
    } 
	else 
	{
        // 清理资源
        I2S_Play_Stop();
        
        if (flacctrl) {
            if (flacctrl->decoder) {
                free_bsc(flacctrl->decoder);
                flacctrl->decoder = NULL; 
            }
            free_bsc(flacctrl);
            flacctrl = NULL;
        }

        if (music_ctrl.file) {
            f_close(music_ctrl.file);
            free_bsc(music_ctrl.file);
            music_ctrl.file = NULL;
        }

        if (music_ctrl.i2sbuf1) { free_bsc(music_ctrl.i2sbuf1); music_ctrl.i2sbuf1 = NULL; }
        if (music_ctrl.i2sbuf2) { free_bsc(music_ctrl.i2sbuf2); music_ctrl.i2sbuf2 = NULL; }
        if (flac_in_buffer) { free_bsc(flac_in_buffer); flac_in_buffer = NULL; }
        
        // 处理播放状态转换
        if(Music_Status == Song_Error) Music_Status = Music_Exit;
        if(Music_Status == Song_End) {
            if(kv_music_switch_method == Play_In_Order) play_next_song();
            if(kv_music_switch_method == Play_Randomly) play_random_song();
            if(kv_music_switch_method == Play_Repeatly) play_same_song();
            Music_Status = Song_Prepare;
        }
        if(Music_Status == Song_Next) {
            play_next_song();
            Music_Status = Song_Prepare;
        }
        if(Music_Status == Song_Previous) {
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
