#include "stm32f4xx.h"                  // Device header
#include "aac.h"
#include "music.h"
#include "systick_conf.h"
#include "malloc.h"
#include "ff.h"
#include "string.h"
#include "i2s.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include "defines.h"
#include "spectrum_dsp.h"

//music二进制信号量
extern SemaphoreHandle_t xI2SSemaphore;// I2S 传输完成标志

__aacctrl * aacctrl;	    //aac控制结构体 

uint8_t config_err = 0;

//填充PCM数据到DAC
//buf:PCM数据首地址
//size:pcm数据量(16位为单位)
//nch:声道数(1,单声道,2立体声)
void aac_fill_buffer(uint16_t* buf,uint16_t size,uint8_t nch)
{
	uint16_t i; 
	uint16_t *p_out;
    int16_t *p_in = (int16_t*)buf; // 视为有符号16位数据处理

	if(I2SdmaBuff == 0) p_out=(uint16_t*)music_ctrl.i2sbuf1;
	else p_out=(uint16_t*)music_ctrl.i2sbuf2;

    if(g_hdp0_or_spk1) // 开启音量调节
    {
        if(nch==2)
        {
            for(i=0;i<size;i++) 
            {
                int32_t val = ((int32_t)p_in[i] * g_spk_value) >> 8;
                p_out[i] = (int16_t)val;
            }
        }
        else	//单声道
        {
            for (i = 0; i < size; i++) 
            {
                int32_t val = ((int32_t)p_in[i] * g_spk_value) >> 8;
                p_out[2*i] = (int16_t)val;
                p_out[2*i+1] = (int16_t)val;
            }
        }
    }
    else // 原样输出
    {
        if(nch==2)
        {
            // memcpy效率通常比循环高
            memcpy(p_out, buf, size * sizeof(uint16_t));
        }
        else	//单声道
        {
            for (i = 0; i < size; i++) 
            {
                p_out[2*i] = buf[i];
                p_out[2*i+1] = buf[i];
            }
        }
    }
} 

//得到当前播放时间
//fx:文件指针
//aacx:aac播放控制器
void aac_get_curtime(FIL*fx,__aacctrl *aacx)
{
	uint32_t fpos=0;  	 
	if(fx->fptr>aacx->datastart)
		fpos = (fx->fptr > aacx->datastart) ? (fx->fptr - aacx->datastart) : 0;	//得到当前文件播放到的地方 
	aacx->cursec = fpos * aacx->totsec / (f_size(fx) - aacx->datastart);	//当前播放到第多少秒了
}

//aac文件快进快退函数
//pos:需要定位到的文件位置
//返回值:当前文件位置(即定位后的结果)
uint32_t aac_file_seek(uint32_t pos)
{
    if(pos > f_size(music_ctrl.file))
    {
        pos = f_size(music_ctrl.file);
    }
    f_lseek(music_ctrl.file, pos);
    return f_tell(music_ctrl.file);
}

uint8_t aac_get_info(uint8_t *pname, __aacctrl* pctrl) 
{
    HAACDecoder decoder = NULL;
    AACFrameInfo frame_info;
    FIL *faac = NULL;
    uint8_t *buf = NULL;
    uint32_t br;
    uint8_t res = 0;
    int offset = -1;
    uint32_t file_size = 0;
    uint8_t file_opened = 0;
    int is_adif = 0;
    uint32_t adif_header_size = 0;

    decoder = AACInitDecoder();
    if (!decoder) res = 1;

    if (!res) {	
        faac = malloc_bsc(sizeof(FIL));
        buf = malloc_bsc(AAC_FILE_BUF_SZ);
        if (!faac || !buf) res = 2;
    }

    if (!res) {
        if (f_open(faac, (const char*)pname, FA_READ) != FR_OK) res = 3;
    }

    if (!res) {
        file_opened = 1;
        file_size = f_size(faac);
        uint32_t read_size = AAC_FILE_BUF_SZ < file_size ? AAC_FILE_BUF_SZ : file_size;
        if (f_read(faac, buf, read_size, &br) != FR_OK || br < 1024) res = 4;
    }

    if (!res) 
	{
        if (br >= 4 && memcmp(buf, "ADIF", 4) == 0) {
            is_adif = 1;
            uint8_t* ptr = buf + 4;
            adif_header_size = 4;

            uint8_t flags = *ptr++;
            adif_header_size++;
            uint8_t bitstream_type = (flags >> 4) & 0x01;

            if (bitstream_type == 0) {
                if (ptr + 3 <= buf + br) {
                    pctrl->bitrate = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
                    ptr += 3;
                    adif_header_size += 3;
                }
            }
            pctrl->datastart = adif_header_size;
            offset = adif_header_size;
        } else 
		{
            offset = AACFindSyncWord(buf, br);
        }

        if (offset < 0 && !is_adif) res = 5;
    }

    if (!res) {
        unsigned char *p_work;
        int valid_bytes;

        if (is_adif) {
            p_work = buf + adif_header_size;
            valid_bytes = br - adif_header_size;
        } else {
            p_work = buf + offset;
            valid_bytes = br - offset;
            pctrl->datastart = offset;
        }

        if (AACDecode(decoder, &p_work, &valid_bytes, (short*)music_ctrl.tbuf) != 0) {
            res = 6;
        } else {
            AACGetLastFrameInfo(decoder, &frame_info);

            if (is_adif && pctrl->bitrate == 0) 
                pctrl->bitrate = frame_info.bitRate;

            pctrl->nChans = frame_info.nChans;
            pctrl->Samplerate_Core = frame_info.sampRateCore;
            pctrl->Samplerate_Out = frame_info.sampRateOut;
            pctrl->bit_depth = frame_info.bitsPerSample;
            pctrl->outputSamps = frame_info.outputSamps;

            if (!is_adif) {
                pctrl->bitrate = frame_info.bitRate;
            }

            if (pctrl->bitrate > 0) {
                uint32_t data_size = file_size - pctrl->datastart;
                pctrl->totsec = (data_size * 8) / pctrl->bitrate;
            } else {
                pctrl->totsec = 0;
            }
        }
    }

    if (decoder) AACFreeDecoder(decoder);
    if (faac) {
        if (file_opened) f_close(faac);
        free_bsc(faac);
    }
    if (buf) free_bsc(buf);
    config_err = res;
    return res;
}

HAACDecoder aacdecoder;
AACFrameInfo aacframeinfo;
uint8_t* aac_buffer = NULL;//输入buffer  
uint8_t* aac_readptr = NULL;//AAC解码读指针
int aac_offset = 0;	//偏移量
int aac_bytesleft = 0;//buffer还剩余的有效数据
uint8_t aac_outofdata = 1;

uint8_t aac_play_song_prepare(uint8_t* fname)
{ 
	uint8_t res = 0;
 	aacctrl=malloc_bsc(sizeof(__aacctrl)); 
	aac_buffer = malloc_bsc(AAC_FILE_BUF_SZ); 	//申请解码buf大小
	music_ctrl.file=(FIL*)malloc_bsc(sizeof(FIL));
	music_ctrl.i2sbuf1=malloc_bsc(2048*2*sizeof(uint16_t));
	music_ctrl.i2sbuf2=malloc_bsc(2048*2*sizeof(uint16_t));
	music_ctrl.tbuf=malloc_bsc(2048*2*sizeof(uint16_t));
	
	if(!aacctrl||!aac_buffer||!music_ctrl.file||!music_ctrl.i2sbuf1||!music_ctrl.i2sbuf2||!music_ctrl.tbuf)//内存申请失败
	{
		free_bsc(aacctrl);
		free_bsc(aac_buffer);
		free_bsc(music_ctrl.file);
		free_bsc(music_ctrl.i2sbuf1);
		free_bsc(music_ctrl.i2sbuf2);
		free_bsc(music_ctrl.tbuf); 
		res = 1;
	}
	else memset(music_ctrl.file, 0, sizeof(FIL)); 
	if(res==0)
	{
		memset(music_ctrl.i2sbuf1,0,2048*2*sizeof(uint16_t));	//数据清零 
		memset(music_ctrl.i2sbuf2,0,2048*2*sizeof(uint16_t));	//数据清零 
		memset(aacctrl,0,sizeof(__aacctrl));//数据清零
		res = aac_get_info(fname,aacctrl);  
	}
	if(res==0)
	{ 
		I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_16bextended);	
		I2S2_SampleRate_Set(aacctrl->Samplerate_Out);		//设置采样率 
		I2S2_TX_DMA_Init(music_ctrl.i2sbuf1,music_ctrl.i2sbuf2,aacctrl->outputSamps);//配置TX DMA
		aacdecoder=AACInitDecoder(); 					//aac解码申请内存
		res = f_open(music_ctrl.file,(char*)fname,FA_READ);	//打开文件

        // [新增] 填充 music_info 结构体
        music_info.total_sec = aacctrl->totsec;    // 总秒数
        music_info.bitrate = aacctrl->bitrate;     // 比特率
        music_info.samplerate = aacctrl->Samplerate_Out; // 采样率
        music_info.bit_depth = aacctrl->bit_depth;       // 位深
        music_info.current_sec = 0;                // 当前秒数清零
	}
	if(!res && aacdecoder)//打开文件成功
	{
		f_lseek(music_ctrl.file,aacctrl->datastart);	//跳过文件头中tag信息
	}
	
	if(res)
	{
		f_close(music_ctrl.file);
		free_bsc(music_ctrl.file); 
		music_ctrl.file = NULL;
	}
	return res;
}

void aac_play_song_task(uint8_t* fname)
{
	uint8_t res = 0;
	uint32_t br = 0; 
	int err = 0;

	if(Music_Status == Song_Prepare)
	{
        if (aac_play_song_prepare(fname) == 0) 
		{
             Music_Status = Song_Playing;
			 I2S_Play_Start();
        }
		else Music_Status = Song_End;
    }
	else if(Music_Status == Song_Playing)
	{
		xSemaphoreTake(xI2SSemaphore, portMAX_DELAY);//传输完成
		
		// 确定目标缓冲区
		uint16_t* target_buf;
		if(I2SdmaBuff == 0)
			target_buf = (uint16_t*)music_ctrl.i2sbuf1;
		else
			target_buf = (uint16_t*)music_ctrl.i2sbuf2;
		
		if(Music_Suspend_Flag) // 暂停状态，填充0
		{
			memset(target_buf, 0, 2048 * 2 * sizeof(uint16_t));
		}
		else // 正常播放状态
		{
			if(aac_outofdata)
			{
				aac_readptr = aac_buffer;	// AAC读指针指向buffer
				aac_offset = 0;		//偏移量为0
				aac_bytesleft = 0;
				aac_outofdata = 0;
				res=f_read(music_ctrl.file,aac_buffer,AAC_FILE_BUF_SZ,&br);//一次读取AAC_FILE_BUF_SZ字节
				if(res)//读数据出错了
				{
					Music_Status = Song_End;
				}
				if(br == 0)		//读数为0,说明解码完成了.
				{
					Music_Status = Song_End;
				}
				aac_bytesleft += br;//buffer里面有多少有效AAC数据
				err = 0;
			}
			
			if(Music_Status == Song_Playing) // 检查状态是否改变
			{
				aac_offset = AACFindSyncWord(aac_readptr,aac_bytesleft);//在readptr位置,开始查找同步字符
				if(aac_offset<0)	//没有找到同步字符,跳出帧解码循环
				{ 
					aac_outofdata = 1;
				}
				else	        //找到同步字符了
				{
					aac_readptr += aac_offset;		//AAC读指针偏移到同步字符处.
					aac_bytesleft -= aac_offset;		//buffer里面的有效数据个数,必须减去偏��量
					err = AACDecode(aacdecoder,&aac_readptr,&aac_bytesleft,(short*)music_ctrl.tbuf);//解码一帧AAC数据
					if(err!=0)
					{
						aac_outofdata = 1;
					}
					else
					{
						AACGetLastFrameInfo(aacdecoder,&aacframeinfo);	//得到刚刚解码的AAC帧信息
						if(aacctrl->bitrate!=aacframeinfo.bitRate)		//更新码率
						{
							aacctrl->bitrate = aacframeinfo.bitRate; 
						}
						aac_fill_buffer((uint16_t*)music_ctrl.tbuf,aacframeinfo.outputSamps,aacframeinfo.nChans);//填充pcm数据
					}
					
					if(aac_bytesleft < AAC_MAINBUF_SIZE*2)//当数组内容小于2倍MAINBUF_SIZE的时候,必须补充新的数据进来.
					{
						memmove(aac_buffer,aac_readptr,aac_bytesleft);//移动readptr所指向的数据到buffer里面,数据量大小为:bytesleft
						f_read(music_ctrl.file,aac_buffer+aac_bytesleft,AAC_FILE_BUF_SZ-aac_bytesleft,&br);//补充余下的数据
						if(br<AAC_FILE_BUF_SZ-aac_bytesleft)
						{
							memset(aac_buffer+aac_bytesleft+br,0,AAC_FILE_BUF_SZ-aac_bytesleft-br); 
						}
						aac_bytesleft = AAC_FILE_BUF_SZ;  
						aac_readptr=aac_buffer; 
					}

                    // [新增] 更新全局播放时间
                    aac_get_curtime(music_ctrl.file, aacctrl);
                    music_info.current_sec = aacctrl->cursec;
				}
			}
		}
		Extract_FFT((uint8_t*)target_buf);
	}
	else //请求结束播放/播放完成
	{
		I2S_Play_Stop();
		
        if (aacdecoder) {
            AACFreeDecoder(aacdecoder);
            aacdecoder = NULL;
        }
		if (music_ctrl.file) {
            f_close(music_ctrl.file);
            free_bsc(music_ctrl.file);
            music_ctrl.file = NULL;
        }
		if (aacctrl) { 
			free_bsc(aacctrl); 
			aacctrl = NULL; 
		}
		if (aac_buffer) { 
			free_bsc(aac_buffer); 
			aac_buffer = NULL; 
		}
		if (music_ctrl.i2sbuf1) { 
			free_bsc(music_ctrl.i2sbuf1); 
			music_ctrl.i2sbuf1 = NULL; 
		}
		if (music_ctrl.i2sbuf2) { 
			free_bsc(music_ctrl.i2sbuf2); 
			music_ctrl.i2sbuf2 = NULL; 
		}
		if (music_ctrl.tbuf) { 
			free_bsc(music_ctrl.tbuf); 
			music_ctrl.tbuf = NULL; 
		}

		aac_buffer = NULL;
		aac_readptr = NULL;
		aac_bytesleft = 0;
		aac_offset = 0;
		aac_outofdata = 1;
		
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
