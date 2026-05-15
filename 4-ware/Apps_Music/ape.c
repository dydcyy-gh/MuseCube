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

__apectrl * apectrl;	//APE播放控制结构体

#define AUDIO_MIN(x,y)	((x)<(y)? (x):(y))

//apedecoder.c里面需要的数组 
extern filter_int *filterbuf64;		//需要2816字节 

// [优化] 原地音量处理，无需拷贝
// buf: 既是输入也是输出 (I2S DMA buffer)
// size: 采样点总数 (例如 4096)
void ape_apply_volume_in_place(uint16_t* buf, uint16_t size)
{
    // 如果是最大音量，直接返回，实现真正的 Zero-Copy (CPU占用为0)
    if (!g_hdp0_or_spk1 || g_spk_value == 0xFF) return;

    // 使用32位指针处理，利用 STM32 总线位宽一次读写两个采样点
    uint32_t *p32 = (uint32_t *)buf;
    uint32_t samples_pairs = size / 2; // 循环次数减半

    for (uint32_t i = 0; i < samples_pairs; i++) 
    {
        uint32_t raw = p32[i]; // 一次读取两个 int16 (Low:左声道, High:右声道)
        
        // 分离高低16位并符号扩展为32位整数
        int16_t low = (int16_t)(raw & 0xFFFF);
        int16_t high = (int16_t)(raw >> 16);

        // 分别计算音量 (位移代替除法)
        int32_t val_low = ((int32_t)low * g_spk_value) >> 8;
        int32_t val_high = ((int32_t)high * g_spk_value) >> 8;

        // 拼合回32位并写入
        // 关键：(uint16_t)强制转换是为了截断高位符号，防止负数干扰位或运算
        p32[i] = (uint32_t)((uint16_t)val_low) | ((uint32_t)((uint16_t)val_high) << 16);
    }
}

//得到当前播放时间
void ape_get_curtime(FIL*fx,__apectrl *apectrl)
{
	long long fpos=0;  	 
	if(fx->fptr>apectrl->datastart)fpos=fx->fptr-apectrl->datastart;	
	apectrl->cursec=fpos*apectrl->totsec/(f_size(fx)-apectrl->datastart);	
}

struct ape_ctx_t *apex; 

int bytesinbuffer;

uint8_t *ape_readptr;
uint8_t *ape_buffer;
int *decoded0;
int *decoded1; // 这个指针现在将变成动态指向 DMA 缓冲区的指针

uint8_t ape_play_song_prepare(uint8_t* fname)
{
    uint8_t res = 0;
	uint32_t totalsamples;
	
	filterbuf64=malloc_bsc(2816);  
	apectrl=malloc_bsc(sizeof(__apectrl));
	apex=malloc_bsc(sizeof(struct ape_ctx_t));
	decoded0=malloc_bsc(APE_BLOCKS_PER_LOOP*4);
	
	music_ctrl.file=(FIL*)malloc_bsc(sizeof(FIL));
	music_ctrl.i2sbuf1=malloc_bsc(APE_BLOCKS_PER_LOOP*4);
	music_ctrl.i2sbuf2=malloc_bsc(APE_BLOCKS_PER_LOOP*4);  
	ape_buffer=malloc_bsc(APE_FILE_BUF_SZ);
	
    // 检查指针有效性 (移除了 decoded1)
	if(filterbuf64&&apectrl&&apex&&decoded0&&music_ctrl.file&&music_ctrl.i2sbuf1&&music_ctrl.i2sbuf2&&ape_buffer)
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
		I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_16bextended);
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
	static int firstbyte = 0;
	static int bytesconsumed = 0;
	static int currentframe = 0;
	static int nblocks = 0;
	static int blockstodecode = 0;
	int n;
	uint8_t res = 0;

	if(Music_Status == Song_Prepare)
	{
		uint8_t res = ape_play_song_prepare(fname);
		if(!res) 
		{
			currentframe = 0; 
			firstbyte = 3;
			I2S_Play_Start();
			Music_Status = Song_Playing;
		}
		else Music_Status = Song_End;
	}
	else if(Music_Status == Song_Playing)
	{
		xSemaphoreTake(xI2SSemaphore, portMAX_DELAY);//传输完成
		
		// 1. 确定目标 DMA 缓冲区
		uint16_t* target_buf;
		if(I2SdmaBuff == 0)
			target_buf = (uint16_t*)music_ctrl.i2sbuf1;
		else
			target_buf = (uint16_t*)music_ctrl.i2sbuf2;
		
        // 2. [关键优化] 将 decoded1 直接指向目标 DMA 缓冲区
        // 注意：APE解码器通常输出 int 类型，但这里假设你的库配置或逻辑
        // 已经将其作为打包的 16bit 数据处理 (参考原代码 logic)
        // 强制转换指针，让解码器直接向 DMA 内存写数据
        decoded1 = (int*)target_buf;

		if(Music_Suspend_Flag) 
		{
			memset(target_buf, 0, APE_BLOCKS_PER_LOOP * 4); 
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
                
                // 3. 执行解码，直接写入 DMA Buffer
				res =decode_chunk(apex,ape_readptr,&firstbyte,&bytesconsumed,decoded0,decoded1,blockstodecode);
				if(res!=0)
				{
					Music_Status = Song_End;
				} 
                
                // 4. [优化] 原地音量调节
                // 此时数据已经在 target_buf 中了，直接修改即可
                ape_apply_volume_in_place(target_buf, APE_BLOCKS_PER_LOOP*2);
				
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

		if (filterbuf64) { 
			free_bsc(filterbuf64); 
			filterbuf64 = NULL; 
		}
		
		if (apectrl) { 
			free_bsc(apectrl); 
			apectrl = NULL; 
		}
        
        if (apex) {
            if (apex->seektable) { 
				free_bsc(apex->seektable); 
				apex->seektable = NULL; 
			}
            free_bsc(apex); 
            apex = NULL;
        }
        
		if (decoded0) { free_bsc(decoded0); decoded0 = NULL; }
		if (music_ctrl.i2sbuf1) { free_bsc(music_ctrl.i2sbuf1); music_ctrl.i2sbuf1 = NULL; }
		if (music_ctrl.i2sbuf2) { free_bsc(music_ctrl.i2sbuf2); music_ctrl.i2sbuf2 = NULL; } 
		if (ape_buffer) { free_bsc(ape_buffer); ape_buffer = NULL; }
		
		
		ape_buffer = NULL;
		ape_readptr = NULL;
        decoded1 = NULL;
		
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
			chosen_file_path_free();
			Music_Status = Song_Prepare;
		}
	}
}
