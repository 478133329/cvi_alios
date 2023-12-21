#ifndef __CVI_AUD_COMM_H__
#define __CVI_AUD_COMM_H__

#include "cvi_type.h"
typedef struct audio_dev_info
{
    CVI_U8 Card_id;
    CVI_U8 Dev_id;

    CVI_U8 channels;
    CVI_U32 samplerate;
    CVI_U32 bitdepth;
    CVI_U32 period_size;
    CVI_U32 period_count;
    CVI_U32 period_bytes;
    CVI_U64 DFPhyAddr;
    CVI_U8 data_fmt;
}AUDIO_CONFIG_INFO;
#define AUDIO_DATAFIFO_LEN  11
#endif
