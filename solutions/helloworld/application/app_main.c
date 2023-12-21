/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */
#include <aos/kernel.h>
#include <stdio.h>
#include <ulog/ulog.h>
#include <unistd.h>
#include "common_yocsystem.h"
#include "media_video.h"
#include "media_audio.h"
#include "platform.h"
#include "custom_event.h"
#include "cvi_param.h"
#include "cvi_msg_server.h"
#include "app_anonmsg.h"
#include "app_param.h"

#if CONFIG_PQTOOL_SUPPORT == 1
#include "cvi_ispd2.h"
#endif

extern void ipcm_driver_test_init(void);

#define TAG "app"

#include "cvi_comm_ipcm.h"
#include "cvi_ipcm.h"
#include <console_uart.h>

#define ANON_PRIV_DATA_MAGIC 0x443355aa
#define ANON_TEST_PARAM_DATA 0x4a5a2230

static IPCM_ANON_MSGPROC_FN _handler_hook = NULL;
static void *_hook_priv_data = NULL;

static int _anon_msg_process(CVI_VOID *priv, IPCM_ANON_MSG_S *data)
{
	unsigned char port_id, msg_id, data_type;
	unsigned int data_len;
	int ret = 0;

	if (priv != (void *)ANON_PRIV_DATA_MAGIC) {
		printf("======anon test fail, reg handle magic err.\n");
		return -1;
	}
	if (data == NULL) {
		printf("======anon test fail, handle data is null.\n");
		return -1;
	}

	if (_handler_hook) {
		ret = _handler_hook(_hook_priv_data, data);
		if (ret == 1) // hook
			return 0;
	}

	port_id = data->u8PortID;
	msg_id = data->u8MsgID;
	data_type = data->u8DataType;
	data_len = data->stData.u32Size;

	if ((port_id == IPCM_PORT_ANON_ST) && (msg_id == 0) && (data_type == IPCM_MSG_TYPE_SHM)) {
		console_push_data_to_ringbuffer(data->stData.pData, data_len);
	}
#ifdef CONFIG_RTOS_ANNON_MSG
	APP_ANONMSG_process(priv, data);
#endif
	return ret;
}

static int anon_test_init(void)
{
	int ret = 0;

	ret = CVI_IPCM_AnonInit();
	if (ret) {
		printf("======anon test ipcm_anon_init fail:%d.\n",ret);
		return ret;
	}

	ret = CVI_IPCM_RegisterAnonHandle(IPCM_PORT_ANON_ST, _anon_msg_process, (void *)(unsigned long)ANON_PRIV_DATA_MAGIC);
	if (ret) {
		printf("======anon test ipcm_anon_register_handle fail:%d.\n",ret);
		CVI_IPCM_AnonUninit();
		return ret;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	//board pinmux init
	PLATFORM_IoInit();

    CVI_IPCM_SetRtosSysBootStat();

	YOC_SYSTEM_Init();

	//cli and ulog init
	YOC_SYSTEM_ToolInit();

	//Fs init
	//YOC_SYSTEM_FsVfsInit();
	//load cfg
	PARAM_LoadCfg();
	//video driver init
	MEDIA_VIDEO_SysInit();
#ifdef CONFIG_AUD_DRV_SEL
	//audio driver init
	MEDIA_AUDIO_Init();
#endif
	ipcm_driver_test_init();

	CVI_MSG_Init();

	anon_test_init();
#ifdef CONFIG_RTOS_PRASE_PARAM
	APP_PARAM_Parse();
#endif
#ifdef CONFIG_FAST_BOOT_MODE
	//custom_evenet_pre
	//media video
	MEDIA_VIDEO_Init(0);
#endif

#if 0
	//network
	#if (CONFIG_APP_ETHERNET_SUPPORT == 1)
	ethernet_init();
	#endif
	#if (CONFIG_APP_HI3861_WIFI_SUPPORT == 1)
	APP_WifiInit();
	#endif
	//cli and ulog init
	// YOC_SYSTEM_ToolInit();
	#if (CONFIG_PQTOOL_SUPPORT == 1)
	usleep(1000);
	isp_daemon2_init(5566);
	#endif
	LOGI(TAG, "app start........\n");
	APP_CustomEventStart();
#endif
	while (1) {
		aos_msleep(3000);
	};
}
