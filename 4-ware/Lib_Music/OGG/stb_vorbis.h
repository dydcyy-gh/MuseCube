#ifndef __STB_VORBIS_H__
#define __STB_VORBIS_H__

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

// stb_vorbis 不透明句柄
typedef struct stb_vorbis stb_vorbis;

// 自定义内存分配器 (传入 stb_vorbis_open_file)
typedef struct {
    char *alloc_buffer;
    int   alloc_buffer_length_in_bytes;
} stb_vorbis_alloc;

// 文件信息
typedef struct {
    unsigned int sample_rate;
    int          channels;
    unsigned int setup_memory_required;
    unsigned int setup_temp_memory_required;
    unsigned int temp_memory_required;
    int          max_frame_size;
} stb_vorbis_info;

// API
extern stb_vorbis     *stb_vorbis_open_file(FIL *f, int close_handle_on_close, int *error, const stb_vorbis_alloc *alloc);
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);
extern void            stb_vorbis_close(stb_vorbis *f);
extern int             stb_vorbis_get_sample_offset(stb_vorbis *f);
extern int             stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels, short *buffer, int num_shorts);
extern int             stb_vorbis_seek(stb_vorbis *f, unsigned int sample_number);
extern float           stb_vorbis_stream_length_in_seconds(stb_vorbis *f);

#ifdef __cplusplus
}
#endif

#endif // __STB_VORBIS_H__
