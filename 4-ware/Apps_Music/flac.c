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
//pos:需要定位到的文件位置
//返回值:当前文件位置(即定位后的结果)
uint32_t flac_file_seek(uint32_t pos)
{
	if(pos>f_size(music_ctrl.file))
	{
		pos=f_size(music_ctrl.file);
	}
	f_lseek(music_ctrl.file,pos);
	return music_ctrl.file->fptr;
}

#define IN_BUF_SIZE 2*1024
#define OUT_BUF_SIZE 12*1024

uint8_t* flac_in_buffer=0;  

uint32_t in_buf_byte_left;
uint32_t out_buf_byte_left;
uint32_t br=0; 
  
uint32_t flac_fptr=0; 

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

        // [新增] 填充 music_info 结构体，参考 wav.c
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
				flac_fptr = music_ctrl.file->fptr;
			}
			
			// 使用foxen-flac解码
			if(flacctrl->nchannels == 1) out_buf_byte_left = OUT_BUF_SIZE/2;
			else out_buf_byte_left = OUT_BUF_SIZE;
		
			uint8_t *out_ptr = target_buf;  // 添加指针来跟踪当前输出位置

			while(out_buf_byte_left)
			{
				// 先看看要不要读数据
				if(in_buf_byte_left < IN_BUF_SIZE/2) 
				{
					f_read(music_ctrl.file, flac_in_buffer + in_buf_byte_left, IN_BUF_SIZE/2, &br);
					in_buf_byte_left += br;
				}
				// 没有数据了
				if(!in_buf_byte_left) {Music_Status = Song_End; break;}
				
				// 解码
				uint32_t in_len = in_buf_byte_left;
				uint32_t out_len = out_buf_byte_left / 4;
				int32_t* temp_buf = (int32_t*)out_ptr;
				
				fx_flac_state_t state = fx_flac_process(flacctrl->decoder, flac_in_buffer, &in_len, temp_buf, &out_len);
				
				// 出错
				if(state == FLAC_ERR) {Music_Status = Song_End; break;}
				if(state == FLAC_SEARCH_FRAME) {continue;}

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
                            
                            // 软件音量缩放 (参考wav.c)
                            if(g_hdp0_or_spk1) {
                                sample = (int32_t)(((int64_t)sample * g_spk_value) >> 8);
                            }
                            
                            // 数据位整理 (32bit ROR 16, 优化性能)
							int32_t ex_sample = __ROR(sample, 16);
                            
							*dst-- = ex_sample;
							*dst-- = ex_sample;
							src--;
						}
						out_ptr += out_len * 8;
					}
					else
					{
                        // Stereo 优化处理
						uint32_t* p = (uint32_t*)temp_buf;
						for(uint32_t i = 0; i < out_len; i++)
						{
                            int32_t sample = p[i];
                            
                            // 软件音量缩放
                            if(g_hdp0_or_spk1) {
                                sample = (int32_t)(((int64_t)sample * g_spk_value) >> 8);
                            }
                            
                            // 数据位整理 (32bit ROR 16)
                            // 替代原有的16bit指针交换操作，提高性能
                            p[i] = __ROR(sample, 16);
						}
						out_ptr += out_len * 4;
					}
					out_buf_byte_left -= out_len * 4;
				}
				
				// 移动输入缓冲区
				if(in_len < in_buf_byte_left)
				{
					memmove(flac_in_buffer, flac_in_buffer+in_len, in_buf_byte_left-in_len);
					in_buf_byte_left -= in_len;
				}
				else in_buf_byte_left = 0;
			}

			flac_get_curtime(music_ctrl.file, flacctrl);
			flac_fptr = music_ctrl.file->fptr;
            music_info.current_sec = flacctrl->cursec; // [新增] 更新全局播放时间
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
            if(Music_Switch_Method == Play_In_Order) play_next_song();
            if(Music_Switch_Method == Play_Randomly) play_random_song();
            if(Music_Switch_Method == Play_Repeatly) play_same_song();
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
