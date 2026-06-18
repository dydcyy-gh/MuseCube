#include "ape.h"
#include "ff.h"
#include "systick_conf.h"
#include "string.h"
#include "malloc.h"
#include "key.h"
#include "i2s.h"
#include "music.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"
#include "file_unit.h"

__apectrl * apectrl;	// APE播放控制结构体

#define AUDIO_MIN(x,y)	((x)<(y)? (x):(y))

// APE 解码状态变量 (提取为全局，供 seek 和主循环共享)
static int firstbyte = 0;
static int bytesconsumed = 0;
static int currentframe = 0;
static int nblocks = 0;
static int blockstodecode = 0;

// Seek 控制标志
static uint8_t ape_seek_flag = 0;
static uint32_t ape_target_sec = 0;

// apedecoder.c里面需要的数组 
extern filter_int *filterbuf64;		//需要2816字节 

//填充PCM数据到DAC
void ape_fill_buffer(uint16_t* buf, uint16_t size)
{
    uint16_t *p = (I2SdmaBuff == 0) ? 
                 (uint16_t*)music_ctrl.i2sbuf1: 
                 (uint16_t*)music_ctrl.i2sbuf2;

    memcpy(p, buf, size * sizeof(uint16_t));
}

// 原地音量处理
void ape_apply_volume_in_place(uint16_t* buf, uint16_t size)
{
    if (!kv_hdp0_or_spk1 || kv_spk_value == 0xFF) return;

    uint32_t *p32 = (uint32_t *)buf;
    uint32_t samples_pairs = size / 2; 

    for (uint32_t i = 0; i < samples_pairs; i++) 
    {
        uint32_t raw = p32[i]; 
        int16_t low = (int16_t)(raw & 0xFFFF);
        int16_t high = (int16_t)(raw >> 16);

        int32_t val_low = ((int32_t)low * kv_spk_value) >> 8;
        int32_t val_high = ((int32_t)high * kv_spk_value) >> 8;

        p32[i] = (uint32_t)((uint16_t)val_low) | ((uint32_t)((uint16_t)val_high) << 16);
    }
}

struct ape_ctx_t *apex; 

int bytesinbuffer;

uint8_t *ape_readptr;
uint8_t *ape_buffer;
int *decoded0;
int *decoded1; 

// 得到当前播放时间 (使用精确计算代替文件比例，防止时间抖动)
void ape_get_curtime(FIL*fx,__apectrl *apectrl)
{
	if (apex && apex->samplerate) {
		uint32_t samples_played;
		if (currentframe > 0) {
			samples_played = (currentframe - 1) * apex->blocksperframe
			               + (apex->currentframeblocks - nblocks);
			if (samples_played > apex->totalsamples)
				samples_played = apex->totalsamples;
		} else {
			samples_played = 0;
		}
		apectrl->cursec = samples_played / apex->samplerate;
	} else {
		long long fpos=0;
		if(fx->fptr>apectrl->datastart)fpos=fx->fptr-apectrl->datastart;
		apectrl->cursec=fpos*apectrl->totsec/(f_size(fx)-apectrl->datastart);
	}
}

uint8_t ape_play_song_prepare(uint8_t* fname)
{
    uint8_t res = 0;
	uint32_t totalsamples;
	
	filterbuf64=malloc_bsc(2816);  
	apectrl=malloc_bsc(sizeof(__apectrl));
	apex=malloc_bsc(sizeof(struct ape_ctx_t));
	decoded0=malloc_bsc(APE_BLOCKS_PER_LOOP*4);
	decoded1=malloc_bsc(APE_BLOCKS_PER_LOOP*4);
	
	music_ctrl.file=(FIL*)malloc_bsc(sizeof(FIL));
	music_ctrl.i2sbuf1=malloc_bsc(APE_BLOCKS_PER_LOOP*4);
	music_ctrl.i2sbuf2=malloc_bsc(APE_BLOCKS_PER_LOOP*4);  
	ape_buffer=malloc_bsc(APE_FILE_BUF_SZ);
	
	if(filterbuf64&&apectrl&&apex&&decoded0&&decoded1&&music_ctrl.file&&music_ctrl.i2sbuf1&&music_ctrl.i2sbuf2&&ape_buffer)
	{ 
		memset(apex,0,sizeof(struct ape_ctx_t));
		memset(apectrl,0,sizeof(__apectrl));
		memset(music_ctrl.i2sbuf1,0,APE_BLOCKS_PER_LOOP*4);
		memset(music_ctrl.i2sbuf2,0,APE_BLOCKS_PER_LOOP*4);		
		f_open(music_ctrl.file,(char*)fname,FA_READ);
		res=ape_parseheader(music_ctrl.file,apex);
		if(res==0)
		{  
			if((apex->compressiontype>3000)||(apex->fileversion<APE_MIN_VERSION)||(apex->fileversion>APE_MAX_VERSION||apex->bps!=16))
			{
				res = 1;
			}
			else
			{
				apectrl->bps=apex->bps;
				apectrl->samplerate=apex->samplerate;
				if(apex->totalframes>1)totalsamples=apex->finalframeblocks+apex->blocksperframe*(apex->totalframes-1);
				else totalsamples=apex->finalframeblocks;
				apectrl->totsec=totalsamples/apectrl->samplerate;
				apectrl->bitrate=(f_size(music_ctrl.file)-apex->firstframe)*8/apectrl->totsec;
				apectrl->outsamples=APE_BLOCKS_PER_LOOP*2;
				apectrl->datastart=apex->firstframe;
                
                // ===== [极重要修复] 应对 parser.c 因长度验证严格导致 seektable 未分配的问题 =====
                if (apex->seektable == NULL && apex->totalframes > 0) {
                    apex->seektable = (uint32_t*)malloc_bsc(apex->totalframes * sizeof(uint32_t));
                    if (apex->seektable) {
                        uint32_t st_pos = apex->junklength + apex->descriptorlength + apex->headerlength;
                        f_lseek(music_ctrl.file, st_pos);
                        uint32_t br;
                        f_read(music_ctrl.file, apex->seektable, apex->totalframes * sizeof(uint32_t), &br);
                    }
                }
                // ====================================================================

                music_info.total_sec = apectrl->totsec;
                music_info.bitrate = apectrl->bitrate;
                music_info.samplerate = apectrl->samplerate;
                music_info.bit_depth = apectrl->bps;
                music_info.current_sec = 0;
			}
		}
	}
	if(res==0)
	{   
		I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_16b);
		I2S2_SampleRate_Set(apex->samplerate);
		I2S2_TX_DMA_Init(music_ctrl.i2sbuf1,music_ctrl.i2sbuf2,APE_BLOCKS_PER_LOOP*2);		
		f_lseek(music_ctrl.file,apex->firstframe); 
		res = f_read(music_ctrl.file,ape_buffer,APE_FILE_BUF_SZ,(uint32_t*)&bytesinbuffer);	
		ape_readptr = ape_buffer;
	}
	return res;
}

void ape_play_song_task(uint8_t* fname)
{
	int n;
	uint8_t res = 0;

	if(Music_Status == Song_Prepare)
	{
		uint8_t res = ape_play_song_prepare(fname);
		if(!res) 
		{
			currentframe = 0; 
			firstbyte = 3;
			bytesconsumed = 0;
			nblocks = 0;
			blockstodecode = 0;
			ape_seek_flag = 0;
			I2S_Play_Start();
			Music_Status = Song_Playing;
		}
		else Music_Status = Song_End;
	}
	else if(Music_Status == Song_Playing)
	{
		xSemaphoreTake(xI2SSemaphore, portMAX_DELAY);//传输完成
		
		uint16_t* target_buf = (I2SdmaBuff == 0) ? (uint16_t*)music_ctrl.i2sbuf1 : (uint16_t*)music_ctrl.i2sbuf2;

		if(Music_Suspend_Flag) 
		{
			memset(target_buf, 0, APE_BLOCKS_PER_LOOP * 4); 
		}
		else if (ape_seek_flag)
		{
			ape_seek_flag = 0;

			if (apex && apex->seektable && apex->totalframes > 0) {
				// 1. 换算目标帧索引，防止越界
				uint32_t target_sample = ape_target_sec * apex->samplerate;
				currentframe = target_sample / apex->blocksperframe;
				if (currentframe >= apex->totalframes)
					currentframe = apex->totalframes > 0 ? apex->totalframes - 1 : 0;

				// 2. 从 seektable 取出对应文件偏移
				uint32_t fpos = apex->seektable[currentframe];
                
                // ===== [自适应兼容] 如果存储的是相对第一帧的偏移量，则加上首帧绝对位置 =====
                if (apex->seektable[0] < apex->firstframe) {
                    fpos += apex->firstframe;
                }

				// 3. 按照 APE 小端 32 位字节对齐规则换算起始字节
				firstbyte = 3 - (fpos & 3);
				fpos &= ~3;

				// 4. 定位并读取新的数据块
				f_lseek(music_ctrl.file, fpos);
				int n_read;
				f_read(music_ctrl.file, ape_buffer, APE_FILE_BUF_SZ, (uint32_t*)&n_read);
				bytesinbuffer = n_read;
				ape_readptr = ape_buffer;

				// 5. 归零计数状态，迫使下一循环进入 init_frame_decoder 重置解码树
				nblocks = 0;
				bytesconsumed = 0;
				blockstodecode = 0;
			}

			// 6. 将当轮发往 DAC 的 DMA 缓冲区清空，防止断层杂音
			memset(target_buf, 0, APE_BLOCKS_PER_LOOP * 4);

			// 7. 更新时间
			apectrl->cursec = ape_target_sec;
			music_info.current_sec = apectrl->cursec;
		}
		else 
		{
			if(nblocks <= 0)	
			{
				if(currentframe < apex->totalframes) 
				{
					if(currentframe==(apex->totalframes-1))nblocks=apex->finalframeblocks;
					else nblocks=apex->blocksperframe; 
					apex->currentframeblocks=nblocks; 
					init_frame_decoder(apex,ape_readptr,&firstbyte,&bytesconsumed);
					ape_readptr+=bytesconsumed;
					bytesinbuffer-=bytesconsumed;
					currentframe++;
				}
				else Music_Status = Song_End;
			}
			if(nblocks>0)
			{
				blockstodecode=AUDIO_MIN(APE_BLOCKS_PER_LOOP,nblocks);
                
				res =decode_chunk(apex,ape_readptr,&firstbyte,&bytesconsumed,decoded0,decoded1,blockstodecode);
				if(res!=0)
				{
					Music_Status = Song_End;
				}

				// 只拷贝有效数据并原位处理音量，末尾清零
				ape_fill_buffer((uint16_t*)decoded1, blockstodecode * 2);
				if(blockstodecode < APE_BLOCKS_PER_LOOP)
				{
					memset(target_buf + blockstodecode * 2, 0, (APE_BLOCKS_PER_LOOP - blockstodecode) * 4);
				}
                ape_apply_volume_in_place(target_buf, blockstodecode * 2);
				
                ape_readptr+=bytesconsumed;
				bytesinbuffer-=bytesconsumed; 	
				if(bytesconsumed>4*APE_BLOCKS_PER_LOOP)
				{
					nblocks=0;
					Music_Status = Song_End;
				}
				if(bytesinbuffer<4*APE_BLOCKS_PER_LOOP)
				{ 
					memmove(ape_buffer,ape_readptr,bytesinbuffer);
					res=f_read(music_ctrl.file,ape_buffer+bytesinbuffer,APE_FILE_BUF_SZ-bytesinbuffer,(uint32_t*)&n);
					if(res) Music_Status = Song_End;
					bytesinbuffer+=n;  
					ape_readptr=ape_buffer;
				} 
				nblocks-=blockstodecode;
			}
            
            ape_get_curtime(music_ctrl.file, apectrl);
            music_info.current_sec = apectrl->cursec;
		}
		Extract_FFT((uint8_t*)target_buf);
	}
	else
	{
		I2S_Play_Stop();
        if (music_ctrl.file) {
            f_close(music_ctrl.file);
            free_bsc(music_ctrl.file);
            music_ctrl.file = NULL;
        }

		if (filterbuf64) { free_bsc(filterbuf64); filterbuf64 = NULL; }
		if (apectrl) { free_bsc(apectrl); apectrl = NULL; }
        
        if (apex) {
            if (apex->seektable) { free_bsc(apex->seektable); apex->seektable = NULL; }
            free_bsc(apex); 
            apex = NULL;
        }
        
		if (decoded0) { free_bsc(decoded0); decoded0 = NULL; }
		if (decoded1) { free_bsc(decoded1); decoded1 = NULL; }
		if (music_ctrl.i2sbuf1) { free_bsc(music_ctrl.i2sbuf1); music_ctrl.i2sbuf1 = NULL; }
		if (music_ctrl.i2sbuf2) { free_bsc(music_ctrl.i2sbuf2); music_ctrl.i2sbuf2 = NULL; } 
		if (ape_buffer) { free_bsc(ape_buffer); ape_buffer = NULL; }
		
		ape_buffer = NULL;
		ape_readptr = NULL;
		
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

void ape_file_seek(uint32_t target_sec)
{
	if (!apectrl || !apex) return;
	if (target_sec > apectrl->totsec) target_sec = apectrl->totsec;
	ape_target_sec = target_sec;
	ape_seek_flag = 1;
}
