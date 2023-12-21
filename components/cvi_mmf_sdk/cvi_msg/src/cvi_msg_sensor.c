#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_sys.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "cvi_msg_server.h"
#include "msg_ctx.h"
#include "cvi_comm_cif.h"
#include "cvi_comm_isp.h"
#include "cvi_sensor.h"
#include "cvi_param.h"
#include "cvi_vi.h"
#include "cvi_ae_comm.h"
#include "cvi_awb_comm.h"
#include "sensor_cfg.h"
#include <signal.h>

#define MAX_SNSR_NUM 3
#define CHECK_MSG_SIZE(type, size)												\
	if (sizeof(type) != size) {												\
		CVI_TRACE_MSG("size mismatch !!! expect size: %lu, actual size: %u\n", sizeof(type), size);			\
		return CVI_FAILURE;												\
	}

static pthread_t g_sensorDet_thid;
static SNS_AHD_MODE_S g_AhdMode = AHD_MODE_NONE;
static SNS_TYPE_E g_snsr_type = SNS_TYPE_NONE;
static ISP_SNS_OBJ_S *pstSnsObj[MAX_SNSR_NUM];
static ISP_SENSOR_EXP_FUNC_S stSnsrSensorFunc[MAX_SNSR_NUM];
static AE_SENSOR_EXP_FUNC_S stSensorExpFunc[MAX_SNSR_NUM];

static void *device_auto_detect(void *arg)
{
	VI_PIPE ViPipe = *(CVI_U8 *)arg;
	free(arg);
	MSG_PRIV_DATA_S stPrivData = {0};
	SNS_AHD_MODE_S signal_type = AHD_MODE_NONE;
	SNS_AHD_MODE_S signal_type_old = g_AhdMode;
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_U32 u32ModFd = MODFD(CVI_ID_SENSOR, 0, 0);
	CVI_U32 detect_cnt = 0;
	CVI_S32	snsr_type = get_sensor_type(ViPipe);
	SNS_AHD_OBJ_S *pstAhdObj = getAhdObj(snsr_type);
	if (!pstAhdObj) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "fail to get Ahd obj ,error snsr_type:%d !!!", snsr_type);
		return NULL;
	}
	if(pstAhdObj->pfnAhdInit)
		pstAhdObj->pfnAhdInit(ViPipe, false);
	while (1) {
		usleep(150 * 1000); //150ms
		if(pstAhdObj->pfnGetAhdMode)
			signal_type = pstAhdObj->pfnGetAhdMode(ViPipe);
		detect_cnt++;
		if (signal_type_old != signal_type && detect_cnt >= 3) {
			if (signal_type != AHD_MODE_NONE) {
				if (detect_cnt >= 6) {   //powerOn msg need detect more times,wait sensor stable
					if(pstAhdObj->pfnSetAhdMode)
						pstAhdObj->pfnSetAhdMode(ViPipe, signal_type);
				}
				else
					continue;
			}
			CVI_TRACE_SNS(CVI_DBG_WARN, "send signal ======== %d ========\n", signal_type);
			stPrivData.as32PrivData[0] = signal_type;
			s32Ret = CVI_MSG_SendSync_CB(u32ModFd, 0, CVI_NULL, 0, &stPrivData);
			if (s32Ret != CVI_SUCCESS)
				CVI_TRACE_SNS(CVI_DBG_ERR, "send ahd message [%d] fail!!!\n", stPrivData.as32PrivData[0]);
			g_AhdMode = signal_type_old = signal_type;
		}
		if (signal_type_old == signal_type) {
			detect_cnt = 0;
		}
	}
	return NULL;
}

static CVI_S32 MSG_SENSOR_EnableAhdThread(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_S32 ViPipe = pstMsg->as32PrivData[0];
	CVI_U8 *arg = malloc(sizeof(*arg));

	*arg = ViPipe;
	s32Ret = pthread_create(&g_sensorDet_thid, NULL, device_auto_detect, arg);
	if (s32Ret != 0) {
		CVI_TRACE_MSG("create_thread Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_GetAhdStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	SNS_AHD_MODE_S mode = AHD_MODE_NONE;
	CVI_S32	snsr_type = get_sensor_type(ViPipe);
	SNS_AHD_OBJ_S *pstAhdObj = getAhdObj(snsr_type);
	if (!pstAhdObj) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "fail to get Ahd obj ,error snsr_type:%d !!!", snsr_type);
		return CVI_FAILURE;
	}
	pstAhdObj->pfnAhdInit(ViPipe, false);

	pstAhdObj->pfnGetAhdMode(ViPipe);  //skip first catch
	g_AhdMode = mode = pstAhdObj->pfnGetAhdMode(ViPipe);
	CVI_TRACE_SNS(CVI_DBG_WARN, "get ahd mode:%d ,snsr_type:%d\n", mode, snsr_type);
	if (mode != AHD_MODE_NONE)
		pstAhdObj->pfnSetAhdMode(ViPipe, mode);  //load current mode setting

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &mode, sizeof(SNS_AHD_MODE_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, pipe:%d\n", ViPipe);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, ViPipe:%d\n", ViPipe);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsType(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_TYPE_E SnsType = pstMsg->as32PrivData[0];

	g_snsr_type = SnsType;
	pstSnsObj[ViPipe] = (ISP_SNS_OBJ_S *)getSnsObj(g_snsr_type);
	if (pstSnsObj[ViPipe] == CVI_NULL) {
		CVI_TRACE_MSG("sensor %d not exist!\n", g_snsr_type);
		s32Ret = CVI_FAILURE;
	} else {
		if(pstSnsObj[ViPipe]->pfnExpSensorCb)
			pstSnsObj[ViPipe]->pfnExpSensorCb(&stSnsrSensorFunc[ViPipe]);
		if(pstSnsObj[ViPipe]->pfnExpAeCb)
			pstSnsObj[ViPipe]->pfnExpAeCb(&stSensorExpFunc[ViPipe]);
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsType Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetRxAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	RX_INIT_ATTR_S *stRxInitAttr = (RX_INIT_ATTR_S *)pstMsg->pBody;

	CHECK_MSG_SIZE(RX_INIT_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = (pstSnsObj[ViPipe]->pfnPatchRxAttr) ?
		pstSnsObj[ViPipe]->pfnPatchRxAttr(stRxInitAttr) : CVI_SUCCESS;
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_SENSOR_SetRxAttr callback failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsI2c(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	CVI_S32 astI2cDev = pstMsg->as32PrivData[0];
	CVI_S32 s32I2cAddr = pstMsg->as32PrivData[1];
	ISP_SNS_COMMBUS_U unSnsrBusInfo = {
		.s8I2cDev = -1,
	};

	unSnsrBusInfo.s8I2cDev = (CVI_S8)astI2cDev;

	if (pstSnsObj[ViPipe]->pfnPatchI2cAddr) {
		pstSnsObj[ViPipe]->pfnPatchI2cAddr(s32I2cAddr);
	} else {
		CVI_TRACE_MSG("sensor can not set i2c addr :%d Failed : %x!\n", s32I2cAddr, s32Ret);
		s32Ret = CVI_SUCCESS;
	}
	if ((pstSnsObj[ViPipe]->pfnSetBusInfo) && (unSnsrBusInfo.s8I2cDev != -1)) {
		s32Ret = pstSnsObj[ViPipe]->pfnSetBusInfo(ViPipe, unSnsrBusInfo);
	} else {
		CVI_TRACE_MSG("sensor can not set i2c busid :%d Failed : %x!\n", astI2cDev, s32Ret);
		s32Ret = CVI_SUCCESS;
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetIspAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	ISP_INIT_ATTR_S *stIspAttr = (ISP_INIT_ATTR_S *)pstMsg->pBody;

	CHECK_MSG_SIZE(ISP_INIT_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = (pstSnsObj[ViPipe]->pfnSetInit) ? pstSnsObj[ViPipe]->pfnSetInit(ViPipe, stIspAttr) : CVI_SUCCESS;
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_SENSOR_SetIspAttr callback failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_RegCallback(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	ALG_LIB_S stAeLib;
	ALG_LIB_S stAwbLib;
	ISP_DEV IspDev = pstMsg->as32PrivData[0];

	stAeLib.s32Id = IspDev;
	stAwbLib.s32Id = IspDev;
	strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));
	strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));
	//  strncpy(stAfLib.acLibName, CVI_AF_LIB_NAME, sizeof(CVI_AF_LIB_NAME));

	s32Ret = (pstSnsObj[ViPipe]->pfnRegisterCallback) ?
		pstSnsObj[ViPipe]->pfnRegisterCallback(ViPipe, &stAeLib, &stAwbLib) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_RegCallback Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_UnRegCallback(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	ALG_LIB_S stAeLib;
	ALG_LIB_S stAwbLib;
	ISP_DEV IspDev = pstMsg->as32PrivData[0];

	stAeLib.s32Id = IspDev;
	stAwbLib.s32Id = IspDev;
	strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));
	strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));
	//  strncpy(stAfLib.acLibName, CVI_AF_LIB_NAME, sizeof(CVI_AF_LIB_NAME));

	s32Ret = (pstSnsObj[ViPipe]->pfnUnRegisterCallback) ?
		pstSnsObj[ViPipe]->pfnUnRegisterCallback(ViPipe, &stAeLib, &stAwbLib) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_UnRegCallback Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsImgMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	ISP_CMOS_SENSOR_IMAGE_MODE_S *stSnsrMode = (ISP_CMOS_SENSOR_IMAGE_MODE_S *)pstMsg->pBody;

	CHECK_MSG_SIZE(ISP_CMOS_SENSOR_IMAGE_MODE_S, pstMsg->u32BodyLen);

	s32Ret = (stSnsrSensorFunc[ViPipe].pfn_cmos_set_image_mode) ?
		stSnsrSensorFunc[ViPipe].pfn_cmos_set_image_mode(ViPipe, stSnsrMode) : CVI_SUCCESS;
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsImgMode callback failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsWdrMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	WDR_MODE_E wdrMode = pstMsg->as32PrivData[0];

	s32Ret = (stSnsrSensorFunc[ViPipe].pfn_cmos_set_wdr_mode) ?
		stSnsrSensorFunc[ViPipe].pfn_cmos_set_wdr_mode(ViPipe, wdrMode) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsWdrMode Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_GetSnsRxAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_COMBO_DEV_ATTR_S *stDevAttr = (SNS_COMBO_DEV_ATTR_S *)pstMsg->pBody;

	s32Ret = (pstSnsObj[ViPipe]->pfnGetRxAttr) ?
		pstSnsObj[ViPipe]->pfnGetRxAttr(ViPipe, stDevAttr) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_GetSnsRxAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stDevAttr, sizeof(SNS_COMBO_DEV_ATTR_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsProbe(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = (pstSnsObj[ViPipe]->pfnSnsProbe) ?
		pstSnsObj[ViPipe]->pfnSnsProbe(ViPipe) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsProbe Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsGpioInit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct snsr_rst_gpio_s snsr_gpio;
	CVI_U32 devNo = GET_DEV_ID(pstMsg->u32Module);

	snsr_gpio.snsr_rst_port_idx = pstMsg->as32PrivData[0];
	snsr_gpio.snsr_rst_pin = pstMsg->as32PrivData[1];
	snsr_gpio.snsr_rst_pol = pstMsg->as32PrivData[2];
	cvi_cif_reset_snsr_gpio_init(devNo, &snsr_gpio);
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsGpioInit Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_RstSnsGpio(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 devNo = GET_DEV_ID(pstMsg->u32Module);
	CVI_U32 rstEnable = pstMsg->as32PrivData[0];

	cif_reset_snsr_gpio(devNo, rstEnable);
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_RstSnsGpio Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_RstMipi(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 devNo = GET_DEV_ID(pstMsg->u32Module);
	CVI_U32 rstEnable = pstMsg->as32PrivData[0];

	if (rstEnable == 1) {
		cif_reset_mipi(devNo);
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_RstMipi Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetMipiAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_TYPE_E SnsType = pstMsg->as32PrivData[0];
	ISP_SNS_OBJ_S *mipiSnsObj = (ISP_SNS_OBJ_S *)getSnsObj(SnsType);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	SNS_COMBO_DEV_ATTR_S stDevAttr;

	mipiSnsObj->pfnGetRxAttr(ViPipe, &stDevAttr);
	cif_set_dev_attr(&stDevAttr);

	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetMipiAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_EnableSnsClk(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 devNo = GET_DEV_ID(pstMsg->u32Module);
	CVI_U32 clkEnable = pstMsg->as32PrivData[0];

	if (clkEnable == 1) {
		cif_enable_snsr_clk(devNo, clkEnable);
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_EnableSnsClk Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsStandby(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	if (pstSnsObj[ViPipe]->pfnStandby) {
		pstSnsObj[ViPipe]->pfnStandby(ViPipe);
	} else {
		CVI_TRACE_MSG("sensor no standby func !\n");
		s32Ret = CVI_SUCCESS;
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsStandby Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsInit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	if (stSnsrSensorFunc[ViPipe].pfn_cmos_sensor_init) {
		stSnsrSensorFunc[ViPipe].pfn_cmos_sensor_init(ViPipe);
	} else {
		CVI_TRACE_MSG("sensor no set Isp Init func !\n");
		s32Ret = CVI_SUCCESS;
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsInit Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetVIFlipMirrorCB(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_DEV ViDev = pstMsg->as32PrivData[0];

	if (pstSnsObj[ViPipe]->pfnMirrorFlip) {
		CVI_VI_RegChnFlipMirrorCallBack(ViPipe, ViDev, (void *)pstSnsObj[ViPipe]->pfnMirrorFlip);
	}
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetVIFlipMirrorCB Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, CVI_NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_GetAeDefault(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	AE_SENSOR_DEFAULT_S *stAeDefault = (AE_SENSOR_DEFAULT_S *)pstMsg->pBody;

	s32Ret = (stSensorExpFunc[ViPipe].pfn_cmos_get_ae_default) ?
		stSensorExpFunc[ViPipe].pfn_cmos_get_ae_default(ViPipe, stAeDefault) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_GetAeDefault Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stAeDefault, sizeof(AE_SENSOR_DEFAULT_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_GetIspBlkLev(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	ISP_CMOS_BLACK_LEVEL_S *stBlc = (ISP_CMOS_BLACK_LEVEL_S *)pstMsg->pBody;

	s32Ret = (stSnsrSensorFunc[ViPipe].pfn_cmos_get_isp_black_level) ?
		stSnsrSensorFunc[ViPipe].pfn_cmos_get_isp_black_level(ViPipe, stBlc) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_GetIspBlkLev Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stBlc, sizeof(ISP_CMOS_BLACK_LEVEL_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetSnsFps(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	CVI_U8 fps = GET_CHN_ID(pstMsg->u32Module);
	AE_SENSOR_DEFAULT_S *stSnsDft = (AE_SENSOR_DEFAULT_S *)pstMsg->pBody;

	s32Ret = (stSensorExpFunc[ViPipe].pfn_cmos_fps_set) ?
		stSensorExpFunc[ViPipe].pfn_cmos_fps_set(ViPipe, fps, stSnsDft) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetSnsFps Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stSnsDft, sizeof(AE_SENSOR_DEFAULT_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_GetExpRatio(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_EXP_MAX_S *stExpMax = (SNS_EXP_MAX_S *)pstMsg->pBody;

	s32Ret = (stSensorExpFunc[ViPipe].pfn_cmos_get_inttime_max) ?
		stSensorExpFunc[ViPipe].pfn_cmos_get_inttime_max(ViPipe, stExpMax->manual, stExpMax->ratio,
			stExpMax->IntTimeMax, stExpMax->IntTimeMin, stExpMax->LFMaxIntTime) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_GetExpRatio Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stExpMax, sizeof(SNS_EXP_MAX_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetDgainCalc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_GAIN_S *stDgain = (SNS_GAIN_S *)pstMsg->pBody;

	s32Ret = (stSensorExpFunc[ViPipe].pfn_cmos_dgain_calc_table) ?
		stSensorExpFunc[ViPipe].pfn_cmos_dgain_calc_table(ViPipe, &(stDgain->gain), &(stDgain->gainDb)) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetDgainCalc Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stDgain, sizeof(SNS_GAIN_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SENSOR_SetAgainCalc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	SNS_GAIN_S *stAgain = (SNS_GAIN_S *)pstMsg->pBody;

	s32Ret = (stSensorExpFunc[ViPipe].pfn_cmos_again_calc_table) ?
		stSensorExpFunc[ViPipe].pfn_cmos_again_calc_table(ViPipe, &(stAgain->gain), &(stAgain->gainDb)) : CVI_SUCCESS;
	if (s32Ret != 0) {
		CVI_TRACE_MSG("MSG_SENSOR_SetAgainCalc Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stAgain, sizeof(SNS_GAIN_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stSensorCmdTable[] = {
	{ MSG_CMD_SENSOR_AHD ,				MSG_SENSOR_EnableAhdThread },
	{ MSG_CMD_SENSOR_GET_STATUS,		MSG_SENSOR_GetAhdStatus },
	{ MSG_CMD_SENSOR_SET_TYPE,			MSG_SENSOR_SetSnsType },
	{ MSG_CMD_SENSOR_SET_RX_ATTR,		MSG_SENSOR_SetRxAttr },
	{ MSG_CMD_SENSOR_SET_SNS_I2C,		MSG_SENSOR_SetSnsI2c },
	{ MSG_CMD_SENSOR_SET_ISP_INIT,		MSG_SENSOR_SetIspAttr },
	{ MSG_CMD_SENSOR_ISP_REG_CB,		MSG_SENSOR_RegCallback },
	{ MSG_CMD_SENSOR_ISP_UNREG_CB,		MSG_SENSOR_UnRegCallback },
	{ MSG_CMD_SENSOR_SET_IMG_MODE,		MSG_SENSOR_SetSnsImgMode },
	{ MSG_CMD_SENSOR_SET_WDR_MODE,		MSG_SENSOR_SetSnsWdrMode },
	{ MSG_CMD_SENSOR_GET_RX_ATTR,		MSG_SENSOR_GetSnsRxAttr },
	{ MSG_CMD_SENSOR_SET_SNS_PROBE,		MSG_SENSOR_SetSnsProbe },
	{ MSG_CMD_SENSOR_SET_GPIO_INIT,		MSG_SENSOR_SetSnsGpioInit },
	{ MSG_CMD_SENSOR_RESET_GPIO,		MSG_SENSOR_RstSnsGpio },
	{ MSG_CMD_SENSOR_RESET_MIPI,		MSG_SENSOR_RstMipi },
	{ MSG_CMD_SENSOR_SET_MIPI_ATTR,		MSG_SENSOR_SetMipiAttr },
	{ MSG_CMD_SENSOR_EN_SNS_CLK,		MSG_SENSOR_EnableSnsClk },
	{ MSG_CMD_SENSOR_SET_SNS_STANDBY,	MSG_SENSOR_SetSnsStandby },
	{ MSG_CMD_SENSOR_SET_SNS_INIT,		MSG_SENSOR_SetSnsInit },
	{ MSG_CMD_SENSOR_SET_FLIPMIRROR_CB,	MSG_SENSOR_SetVIFlipMirrorCB },
	{ MSG_CMD_SENSOR_GET_AE_DEFAULT,	MSG_SENSOR_GetAeDefault },
	{ MSG_CMD_SENSOR_GET_BLK_LEVEL,		MSG_SENSOR_GetIspBlkLev },
	{ MSG_CMD_SENSOR_SET_SNS_FPS,		MSG_SENSOR_SetSnsFps },
	{ MSG_CMD_SENSOR_GET_EXP_RAT,		MSG_SENSOR_GetExpRatio },
	{ MSG_CMD_SENSOR_SET_DGAIN_CALC,	MSG_SENSOR_SetDgainCalc },
	{ MSG_CMD_SENSOR_SET_AGAIN_CALC,	MSG_SENSOR_SetAgainCalc },
};

MSG_SERVER_MODULE_S g_stModuleSensor = {
	CVI_ID_SENSOR,
	"sensor",
	sizeof(g_stSensorCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stSensorCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetSensorMod(CVI_VOID)
{
	return &g_stModuleSensor;
}
