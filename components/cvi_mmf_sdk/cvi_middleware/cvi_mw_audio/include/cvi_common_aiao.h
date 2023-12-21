#ifndef __CVI_COMMON_AIAO_H__
#define __CVI_COMMON_AIAO_H__

#include "cvi_type.h"
#include <alsa/pcm.h>
#include <pthread.h>
#include "cvi_datafifo.h"
typedef struct audio_card_info
{
    CVI_U8 dev_id;
    AUDIO_CONFIG_INFO config;
    aos_pcm_t *pcmHandle;
    CVI_DATAFIFO_HANDLE dfHandle;
    CVI_DATAFIFO_PARAMS_S dfParam;
    pthread_t thread_id;
    CVI_BOOL ThreadRunning;
    CVI_S32 volIndex;
    CVI_S32 mute;
}AUDIO_CARD_HANDLE;
#endif