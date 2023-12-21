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

static void release(void* arg)
{
	//printf("release %p\n",arg);
}

static AUDIO_CARD_HANDLE AiHandle;
static int thread_loop_cnt;
CVI_VOID * AudAiProc(CVI_VOID* arg)
{
    AUDIO_CARD_HANDLE* pHandle = (AUDIO_CARD_HANDLE *)arg;
    uint8_t *cap_buf = NULL;
    uint32_t rbytes =  pHandle->config.period_bytes;
    uint32_t avail_write = 0;
    uint32_t eRet = 0;
    thread_loop_cnt = 0;
    cap_buf = (uint8_t *)malloc(rbytes);
    aos_debug_printf("%s Enter\r\n", __func__);
    while(pHandle->ThreadRunning)
    {
        eRet = CVI_DATAFIFO_Write(pHandle->dfHandle, NULL);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Write NULL fail eRet:%d\r\n", __func__, eRet);
            goto AI_EXIT;
        }
        eRet = CVI_DATAFIFO_CMD(pHandle->dfHandle, DATAFIFO_CMD_GET_AVAIL_WRITE_LEN, (CVI_VOID *)&avail_write);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO get avail write fail eRet:%d\r\n", __func__, eRet);
            goto AI_EXIT;
        }
        if (avail_write == 0) {
            aos_msleep(4);
            continue;
        }
        memset(cap_buf, 0, rbytes);
        eRet = aos_pcm_readi(pHandle->pcmHandle, cap_buf, aos_pcm_bytes_to_frames(pHandle->pcmHandle, rbytes));
        //aos_debug_printf(" AiProc read_size:%d, data:%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\r\n", eRet,
        //        cap_buf[0], cap_buf[1], cap_buf[2], cap_buf[3], cap_buf[4], cap_buf[5], cap_buf[6], cap_buf[7], cap_buf[8], cap_buf[9]);
        eRet = CVI_DATAFIFO_Write(pHandle->dfHandle, cap_buf);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Write data fail eRet:%d\r\n", __func__, eRet);
            goto AI_EXIT;
        }
        eRet = CVI_DATAFIFO_CMD(pHandle->dfHandle, DATAFIFO_CMD_WRITE_DONE, NULL);
        if (eRet != CVI_SUCCESS) {
            aos_debug_printf("%s, DATAFIFO Write done fail eRet:%d\r\n", __func__, eRet);
            goto AI_EXIT;
        }
        avail_write = 0;
        thread_loop_cnt++;
    }
AI_EXIT:
    if(cap_buf)
        free(cap_buf);
    aos_debug_printf("%s Out\r\n", __func__);
    return NULL;
}

CVI_S32 cvi_audio_ai_open(CVI_VOID *handle, CVI_U64 *addr)
{
    CVI_S32 eRet = 0;
    int dir = 1;
    aos_pcm_hw_params_t *pcm_hw_params;
    AUDIO_CONFIG_INFO *pInfo = (AUDIO_CONFIG_INFO *)handle;

    AiHandle.dev_id = pInfo->Card_id;
    memcpy(&AiHandle.config, pInfo, sizeof(AUDIO_CONFIG_INFO));
    aos_debug_printf("card_id:%d,period_size:%d period_cnt:%d channels:%d rate:%d bitdepth:%d\r\n",
          AiHandle.dev_id, pInfo->period_size, pInfo->period_count, pInfo->channels, pInfo->samplerate, pInfo->bitdepth);

    AiHandle.dfParam.u32EntriesNum = AUDIO_DATAFIFO_LEN;
	AiHandle.dfParam.u32CacheLineSize = pInfo->period_bytes;
	AiHandle.dfParam.bDataReleaseByWriter = CVI_TRUE;
	AiHandle.dfParam.enOpenMode= DATAFIFO_WRITER;

    eRet = CVI_DATAFIFO_Open(&AiHandle.dfHandle, &AiHandle.dfParam);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Open fail eRet:%d\r\n", __func__, eRet);
        return eRet;
    }
    aos_debug_printf("Audio AI open datafifo Handle:0x%x\r\n", AiHandle.dfHandle);
    eRet = CVI_DATAFIFO_CMD(AiHandle.dfHandle, DATAFIFO_CMD_GET_PHY_ADDR, addr);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Get PHY ADDR fail eRet:%d\r\n", __func__, eRet);
        return eRet;
    }
    AiHandle.config.DFPhyAddr = *addr;
    eRet = CVI_DATAFIFO_CMD(AiHandle.dfHandle, DATAFIFO_CMD_SET_DATA_RELEASE_CALLBACK, release);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO SET CALLBACK fail eRet:%d\r\n", __func__, eRet);
        return eRet;
    }

    //init capture(i2s0). clk path: i2s3(master) -> internal_codec -> i2s0(slave)
    aos_pcm_open (&AiHandle.pcmHandle, "pcmC0", AOS_PCM_STREAM_CAPTURE, 0);//打开设备“pcmC0

    aos_pcm_hw_params_alloca (&pcm_hw_params);//申请硬件参数内存空间
    aos_pcm_hw_params_any (AiHandle.pcmHandle, pcm_hw_params);//初始化硬件参数
    pcm_hw_params->period_size = pInfo->period_size;
    pcm_hw_params->buffer_size = pInfo->period_size * pInfo->period_count;
    aos_pcm_hw_params_set_access (AiHandle.pcmHandle, pcm_hw_params, AOS_PCM_ACCESS_RW_INTERLEAVED);// 设置音频数据参数为交错模式
    aos_pcm_hw_params_set_format (AiHandle.pcmHandle, pcm_hw_params, pInfo->bitdepth * 8);//设置音频数据参数为小端16bit
    aos_pcm_hw_params_set_rate_near (AiHandle.pcmHandle, pcm_hw_params, &(pInfo->samplerate), &dir);//设置音频数据参数采样率为16K
    aos_pcm_hw_params_set_channels (AiHandle.pcmHandle, pcm_hw_params, pInfo->channels);//设置音频数据参数为2通道
    aos_pcm_hw_params (AiHandle.pcmHandle, pcm_hw_params);//设置硬件参数到具体硬件中
    if (AiHandle.volIndex <= 0 || AiHandle.volIndex > 32) {
        csi_codec_input_t ch;
        eRet = csi_codec_input_analog_gain(&ch, 12);
        AiHandle.volIndex = 12;
        AiHandle.mute = 0;
    }

    pthread_attr_t attr;
    struct sched_param param;

    AiHandle.ThreadRunning = CVI_TRUE;
    param.sched_priority = 30;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    eRet = pthread_create(&AiHandle.thread_id, &attr, (CVI_VOID * (*)(CVI_VOID *))AudAiProc, (void *)&AiHandle);
	if (eRet != CVI_SUCCESS) {
		aos_debug_printf("pthread_create AudAiProc fail :%d\r\n", eRet);
		return CVI_FAILURE;
	}
    pthread_setname_np(AiHandle.thread_id, "AudAiProc");
    pthread_attr_destroy(&attr);
    return eRet;
}

CVI_S32 cvi_audio_ai_close(CVI_VOID *handle)
{
    CVI_S32 eRet = 0;

    if(AiHandle.ThreadRunning)
    {
        AiHandle.ThreadRunning = CVI_FALSE;
        pthread_join(AiHandle.thread_id, NULL);
    }
    aos_pcm_close(AiHandle.pcmHandle);
    AiHandle.pcmHandle = NULL;
    eRet = CVI_DATAFIFO_Write(AiHandle.dfHandle, NULL);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Write NULL fail eRet:%d\r\n", __func__, eRet);
        return eRet;
    }
    eRet = CVI_DATAFIFO_Close(AiHandle.dfHandle);
    if (eRet != CVI_SUCCESS) {
        aos_debug_printf("%s, DATAFIFO Close fail eRet:%d\r\n", __func__, eRet);
    }
    return eRet;
}

CVI_S32 cvi_audio_ai_setvol(CVI_U8 card_id, CVI_S32 volume)
{
    CVI_S32 eRet = 0;
    csi_codec_input_t ch;

    eRet = csi_codec_input_analog_gain(&ch, volume);
    AiHandle.volIndex = volume;
    return eRet;
}

CVI_S32 cvi_audio_ai_getvol(CVI_U8 card_id, CVI_S32 *volume)
{
    CVI_S32 eRet = 0;

    *volume = AiHandle.volIndex;
    return eRet;
}

CVI_S32 cvi_audio_ai_setmute(CVI_U8 card_id, CVI_S32 enable)
{
    CVI_S32 eRet = 0;
    csi_codec_input_t ch;

    if(enable){
        csi_codec_input_mute(&ch, enable);
    }
    else{
        csi_codec_input_mute(&ch, enable);
        csi_codec_input_analog_gain(&ch, AiHandle.volIndex);
    }
    AiHandle.mute = enable;
    return eRet;
}

CVI_S32 cvi_audio_ai_getmute(CVI_U8 card_id, CVI_S32 *enable)
{
    CVI_S32 eRet = 0;

    *enable = AiHandle.mute;
    return eRet;
}

//proc_ai
static void ai_proc(void){
    printf("\n------------- CVI AI ATTRIBUTE -------------\n");
    printf("AiDev    Channels    SampleRate    BitWidth\n");
    printf("  %d       %d           %6d           %2d\n", AiHandle.dev_id,AiHandle.config.channels, AiHandle.config.samplerate, AiHandle.config.bitdepth * 8);
    printf("\n");
    printf("-------------  CVI AI STATUS   -------------\n");
    printf("Mute\n");
    printf(" %s\n", AiHandle.mute  == 1 ? "yes" : "no");
    printf("\n");
    printf("Vol\n");
    printf(" %d\n", AiHandle.volIndex);
    printf("\n");
    printf("ThreadRunning\n");
    printf(" %s\n", AiHandle.ThreadRunning == true? "yes" : "no");
    printf("\n");
    printf("AudAiProc Loop Count\n");
    printf(" %d\n", thread_loop_cnt);
    printf("\n");
}

static void ai_proc_show(int32_t argc, char **argv)
{
    ai_proc();
}

ALIOS_CLI_CMD_REGISTER(ai_proc_show, proc_ai, audio info);