#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <aos/cli.h>
#include "cvi_type.h"
#include <alsa/pcm.h>
#include <drv/codec.h>
#include <pthread.h>
#include "cvi_aud_dualos.h"
#include "cvi_common_aiao.h"
#include "cvi_datafifo.h"
#include "debug/dbg.h"

static AUDIO_CARD_HANDLE AoHandle;
static int thread_loop_cnt;
CVI_VOID * AudAoProc(CVI_VOID* arg)
{
    AUDIO_CARD_HANDLE* pHandle = (AUDIO_CARD_HANDLE *)arg;
    uint8_t *play_buf = NULL;
    uint32_t wbytes =  pHandle->config.period_bytes;
    uint32_t avail_read = 0;
    uint32_t eRet = 0;
    thread_loop_cnt = 0;
    //play_buf = (uint8_t *)malloc(wbytes);
    aos_debug_printf("%s Enter\r\n", __func__);
    while(pHandle->ThreadRunning)
    {
        eRet = CVI_DATAFIFO_CMD(pHandle->dfHandle, DATAFIFO_CMD_GET_AVAIL_READ_LEN, (CVI_VOID *)&avail_read);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Get avail read fail eRet:%d\r\n", __func__, eRet);
            goto AO_EXIT;
        }
        if (avail_read == 0) {
            aos_msleep(4);
            continue;
        }
        eRet = CVI_DATAFIFO_Read(pHandle->dfHandle, (CVI_VOID **)&play_buf);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Read fail eRet:%d\r\n", __func__, eRet);
            goto AO_EXIT;
        }
        //aos_debug_printf(" AoProc  play_data:%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\r\n",
        //        play_buf[0], play_buf[1], play_buf[2], play_buf[3], play_buf[4], play_buf[5], play_buf[6], play_buf[7], play_buf[8], play_buf[9]);
        aos_pcm_writei(pHandle->pcmHandle, play_buf, aos_pcm_bytes_to_frames(pHandle->pcmHandle, wbytes));
        eRet = CVI_DATAFIFO_CMD(pHandle->dfHandle, DATAFIFO_CMD_READ_DONE, NULL);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Read done fail eRet:%d\r\n", __func__, eRet);
           goto AO_EXIT;
        }
        avail_read = 0;
        thread_loop_cnt++;
    }
AO_EXIT:
    aos_debug_printf("%s Out\r\n", __func__);
    return NULL;
}

CVI_S32 cvi_audio_ao_open(CVI_VOID *handle)
{
    CVI_S32 eRet = 0;
    int dir = 1;
    aos_pcm_hw_params_t *pcm_hw_params;
    AUDIO_CONFIG_INFO *pInfo = (AUDIO_CONFIG_INFO *)handle;

    AoHandle.dev_id = pInfo->Card_id;
    memcpy(&AoHandle.config, pInfo, sizeof(AUDIO_CONFIG_INFO));
    aos_debug_printf("card_id:%d,period_size:%d period_cnt:%d channels:%d rate:%d bitdepth:%d\r\n",
        AoHandle.dev_id, pInfo->period_size, pInfo->period_count, pInfo->channels, pInfo->samplerate, pInfo->bitdepth);

    AoHandle.dfParam.u32EntriesNum = AUDIO_DATAFIFO_LEN;
	AoHandle.dfParam.u32CacheLineSize = pInfo->period_bytes;
	AoHandle.dfParam.bDataReleaseByWriter = CVI_TRUE;
	AoHandle.dfParam.enOpenMode= DATAFIFO_READER;

    eRet = CVI_DATAFIFO_OpenByAddr(&AoHandle.dfHandle, &AoHandle.dfParam, pInfo->DFPhyAddr);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Open fail eRet:%d\r\n", __func__, eRet);
        return eRet;
    }
    //init capture(i2s0). clk path: i2s3(master) -> internal_codec -> i2s0(slave)
    aos_pcm_open (&AoHandle.pcmHandle, "pcmP0", AOS_PCM_STREAM_PLAYBACK, 0);//打开设备“pcmC0

    aos_pcm_hw_params_alloca (&pcm_hw_params);//申请硬件参数内存空间
    aos_pcm_hw_params_any (AoHandle.pcmHandle, pcm_hw_params);//初始化硬件参数
    pcm_hw_params->period_size = pInfo->period_size;
    pcm_hw_params->buffer_size = pInfo->period_size * pInfo->period_count;
    aos_pcm_hw_params_set_access (AoHandle.pcmHandle, pcm_hw_params, AOS_PCM_ACCESS_RW_INTERLEAVED);// 设置音频数据参数为交错模式
    aos_pcm_hw_params_set_format (AoHandle.pcmHandle, pcm_hw_params, pInfo->bitdepth * 8);//设置音频数据参数为小端16bit
    aos_pcm_hw_params_set_rate_near (AoHandle.pcmHandle, pcm_hw_params, &(pInfo->samplerate), &dir);//设置音频数据参数采样率为16K
    aos_pcm_hw_params_set_channels (AoHandle.pcmHandle, pcm_hw_params, pInfo->channels);//设置音频数据参数为2通道
    aos_pcm_hw_params (AoHandle.pcmHandle, pcm_hw_params);//设置硬件参数到具体硬件中
    if (AoHandle.volIndex <= 0 || AoHandle.volIndex > 32) {
        csi_codec_output_t ch;
        eRet = csi_codec_output_analog_gain(&ch, 12);
        AoHandle.volIndex = 12;
        AoHandle.mute = 0;
    }

    pthread_attr_t attr;
    struct sched_param param;

    AoHandle.ThreadRunning = CVI_TRUE;
    param.sched_priority = 30;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    eRet = pthread_create(&AoHandle.thread_id, &attr, (CVI_VOID * (*)(CVI_VOID *))AudAoProc, (void *)&AoHandle);
	if (eRet != CVI_SUCCESS) {
		aos_debug_printf("pthread_create AudAoProc fail :%d\r\n", eRet);
		return CVI_FAILURE;
	}
    pthread_setname_np(AoHandle.thread_id, "AudAoProc");
    pthread_attr_destroy(&attr);

    return eRet;
}

CVI_S32 cvi_audio_ao_close(CVI_VOID *handle)
{
    CVI_S32 eRet = 0;

    if(AoHandle.ThreadRunning)
    {
        AoHandle.ThreadRunning = CVI_FALSE;
        pthread_join(AoHandle.thread_id, NULL);
    }
    aos_pcm_close(AoHandle.pcmHandle);
    AoHandle.pcmHandle = NULL;
    eRet = CVI_DATAFIFO_Close(AoHandle.dfHandle);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Close fail eRet:%d\r\n", __func__, eRet);
    }
    return eRet;
}

CVI_S32 cvi_audio_ao_setvol(CVI_U8 card_id, CVI_S32 volume)
{
    CVI_S32 eRet = 0;
    csi_codec_output_t ch;

    eRet = csi_codec_output_analog_gain(&ch, volume);
    AoHandle.volIndex = volume;
    return eRet;
}

CVI_S32 cvi_audio_ao_getvol(CVI_U8 card_id, CVI_S32 *volume)
{
    CVI_S32 eRet = 0;

    *volume = AoHandle.volIndex;
    return eRet;
}

CVI_S32 cvi_audio_ao_setmute(CVI_U8 card_id, CVI_S32 enable)
{
    CVI_S32 eRet = 0;
    csi_codec_output_t ch;
    printf("cvi_audio_ao_setmute:card:%d, enable:%d\n", card_id, enable);
    if(enable){
        csi_codec_output_mute(&ch, enable);
    }
    else{
        csi_codec_output_mute(&ch, enable);
        csi_codec_output_analog_gain(&ch, AoHandle.volIndex);
    }
    AoHandle.mute = enable;
    return eRet;
}

CVI_S32 cvi_audio_ao_getmute(CVI_U8 card_id, CVI_S32 *enable)
{
    CVI_S32 eRet = 0;

    *enable = AoHandle.mute;
    return eRet;
}

//proc_ao
static void ao_proc(void){
    printf("\n------------- CVI AO ATTRIBUTE -------------\n");
    printf("AoDev    Channels    SampleRate    BitWidth\n");
    printf("  %d       %d           %6d           %2d\n", AoHandle.dev_id,AoHandle.config.channels, AoHandle.config.samplerate, AoHandle.config.bitdepth * 8);
    printf("\n");
    printf("-------------  CVI AO STATUS   -------------\n");
    printf("Mute\n");
    printf(" %s\n", AoHandle.mute  == 1 ? "yes" : "no");
    printf("\n");
    printf("Vol\n");
    printf(" %d\n", AoHandle.volIndex);
    printf("\n");
    printf("ThreadRunning\n");
    printf(" %s\n", AoHandle.ThreadRunning == true? "yes" : "no");
    printf("\n");
    printf("AudAoProc Loop Count\n");
    printf(" %d\n", thread_loop_cnt);
    printf("\n");
}

static void ao_proc_show(int32_t argc, char **argv)
{
    ao_proc();
}

ALIOS_CLI_CMD_REGISTER(ao_proc_show, proc_ao, ao info);