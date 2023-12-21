#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_venc.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"
#include "cvi_datafifo.h"
#include "sys/prctl.h"
#include "aos/kernel.h"
#include "arch_helpers.h"


#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

extern uint8_t fw_h264[];
extern uint8_t *fw_h264_flie;
extern unsigned int fw_h264_size;
extern uint8_t fw_h265[];
extern uint8_t *fw_h265_flie;
extern unsigned int fw_h265_size;

typedef struct _VENC_STREAM_PACK_S {
	VENC_PACK_S pstPack[8];
	VENC_STREAM_S stStream;
	VENC_CHN VeChn;
} VENC_STREAM_PACK_S;

//#define FLOW_DEBUG 1
#ifdef FLOW_DEBUG
#define CVI_MSG_API(msg, ...)		\
	do { \
		if (1) \
			printf("[API] %s = %d, "msg, __func__, __LINE__, ## __VA_ARGS__); \
	} while (0)

#define CVI_MSG_VENC_API_IN  CVI_MSG_API("Chn:%d In\n", VeChn);
#define CVI_MSG_VENC_API_OUT CVI_MSG_API("Chn:%d Out\n", VeChn);

#define DATA_DUMP(data, len)  data_dump((unsigned char *)data, len, __FUNCTION__, __LINE__)

static void data_dump(unsigned char *pu8, int len, const char *func, int line)
{
	int i = 0;
	int sum = 0;

	printf("=====%s %d====,len:%d\n", func, line, len);
	for (i = 0; i < len; i++) {
		//printf("%d:0x%x\n", i, pu8[i]);
		sum += pu8[i];
	}
	printf("sum:%d\n", sum);
}
#else
#define CVI_MSG_VENC_API_IN
#define CVI_MSG_VENC_API_OUT
#define DATA_DUMP(data, len)
#endif

CVI_S32 CVI_VENC_GetDataFifoAddr(VENC_CHN VeChn, CVI_U64 *pu64PhyAddr);
int GetFirmwareSize(uint8_t *fw_addr);

static CVI_S32 venc_handle_firmware (void *pMsg)
{
	CVI_IPCMSG_MESSAGE_S *pstMsg = (CVI_IPCMSG_MESSAGE_S *)pMsg;
	VENC_CHN_ATTR_S *pstAttr = (VENC_CHN_ATTR_S *)pstMsg->pBody;
	CVI_U64 u64FwAddr = 0;

	//firmware load in yoc.bin
	if (GetFirmwareSize(fw_h264) > 10)
		return CVI_SUCCESS;

	if (pstAttr->stVencAttr.enType == PT_H264 && pstMsg->as32PrivData[0]) {
		memcpy(&u64FwAddr, &pstMsg->as32PrivData[1], sizeof(CVI_U64));
		fw_h264_size = ((pstMsg->as32PrivData[0] + 1) >> 1) << 1;

		fw_h264_flie = (uint8_t *)calloc(1, fw_h264_size);
		if (fw_h264_flie == NULL) {
			CVI_TRACE_MSG("allocation fail, firmware !\n");
			return CVI_FAILURE;
		}

		inv_dcache_range(u64FwAddr, fw_h264_size);
		memcpy(fw_h264_flie, (CVI_U8 *)u64FwAddr, fw_h264_size);
		flush_dcache_range((uintptr_t)fw_h264_flie, fw_h264_size);
	} else if (pstAttr->stVencAttr.enType == PT_H265 && pstMsg->as32PrivData[0]) {
		memcpy(&u64FwAddr, &pstMsg->as32PrivData[1], sizeof(CVI_U64));
		fw_h265_size = ((pstMsg->as32PrivData[0] + 1) >> 1) << 1;

		fw_h265_flie = (uint8_t *)calloc(1, fw_h265_size);
		if (fw_h265_flie == NULL) {
			CVI_TRACE_MSG("allocation fail, firmware !\n");
			return CVI_FAILURE;
		}

		inv_dcache_range(u64FwAddr, fw_h265_size);
		memcpy(fw_h265_flie, (CVI_U8 *)u64FwAddr, fw_h265_size);
		flush_dcache_range((uintptr_t)fw_h265_flie, fw_h265_size);
	}

	return CVI_SUCCESS;
}


static CVI_S32 MSG_VENC_CreateChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_ATTR_S *pstAttr = (VENC_CHN_ATTR_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstAttr, sizeof(VENC_CHN_ATTR_S));

	s32Ret = venc_handle_firmware(pstMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("load fw fail, ret:0x%x\n", s32Ret);
		return CVI_FAILURE;
	}

	s32Ret = CVI_VENC_CreateChn(VeChn, pstAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateChn fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_DestroyChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_DestroyChn(VeChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("DestroyChn fail, Chn:%d\n", VeChn);
	}

	if (pstMsg->as32PrivData[0] == PT_H264) {
		free(fw_h264_flie);
		fw_h264_flie = NULL;
		fw_h264_size = 0;
	} else if (pstMsg->as32PrivData[0] == PT_H265) {
		free(fw_h265_flie);
		fw_h265_flie = NULL;
		fw_h265_size = 0;
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_ResetChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_ResetChn(VeChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("ResetChn fail, Chn:%d\n", VeChn);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_StartRecvFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_RECV_PIC_PARAM_S *pstRecvParam = (VENC_RECV_PIC_PARAM_S *)pstMsg->pBody;
	CVI_U64 u64PhyAddr;
	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_GetDataFifoAddr(VeChn, &u64PhyAddr);
	if (s32Ret == CVI_SUCCESS) {
		goto OUT;
	}

	s32Ret = CVI_VENC_StartRecvFrame(VeChn, pstRecvParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StartRecvFrame fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	s32Ret = CVI_VENC_GetDataFifoAddr(VeChn, &u64PhyAddr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetDataFifoAddr fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
	}
OUT:
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	memcpy(respMsg->as32PrivData, &u64PhyAddr, sizeof(u64PhyAddr));

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_StopRecvFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_StopRecvFrame(VeChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StopRecvFrame fail, Chn:%d\n", VeChn);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_QueryStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_STATUS_S stStatus = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stStatus, sizeof(VENC_CHN_STATUS_S));
	s32Ret = CVI_VENC_QueryStatus(VeChn, &stStatus);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StopRecvFrame fail, Chn:%d\n", VeChn);
	}

	DATA_DUMP(&stStatus, sizeof(VENC_CHN_STATUS_S));
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stStatus, sizeof(VENC_CHN_STATUS_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_ATTR_S *pstAttr = (VENC_CHN_ATTR_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstAttr, sizeof(VENC_CHN_ATTR_S));

	s32Ret = CVI_VENC_SetChnAttr(VeChn, pstAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetChnAttr fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_ATTR_S stChnAttr = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stChnAttr, sizeof(VENC_CHN_ATTR_S));
	s32Ret = CVI_VENC_GetChnAttr(VeChn, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetChnAttr fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stChnAttr, sizeof(VENC_CHN_ATTR_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnAttr, sizeof(VENC_CHN_ATTR_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetStream(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = 0;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_STREAM_PACK_S StreamPack = {0};
	VENC_STREAM_S *pstStream = &StreamPack.stStream;
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];

	CVI_MSG_VENC_API_IN;
	pstStream->pstPack = StreamPack.pstPack;

	s32Ret = CVI_VENC_GetStream(VeChn, pstStream, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetStream fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(pstStream, sizeof(VENC_STREAM_S));
	DATA_DUMP(&StreamPack, sizeof(VENC_STREAM_PACK_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &StreamPack, sizeof(VENC_STREAM_PACK_S));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_ReleaseStream(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_ReleaseStream(VeChn, pstStream);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("ReleaseStream fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SendFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VIDEO_FRAME_INFO_S *pstFrame = (VIDEO_FRAME_INFO_S *)pstMsg->pBody;
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_SendFrame(VeChn, pstFrame, s32MilliSec);
	if (s32Ret != CVI_SUCCESS && s32Ret != CVI_ERR_VENC_FRC_NO_ENC) {
		CVI_TRACE_MSG("SendFrame fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_RequestIDR(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_BOOL bInstant  = pstMsg->as32PrivData[0];

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_RequestIDR(VeChn, bInstant);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RequestIDR fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetRoiAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_ROI_ATTR_S *pstRoiAttr = (VENC_ROI_ATTR_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_SetRoiAttr(VeChn, pstRoiAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetRoiAttr fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetRoiAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_ROI_ATTR_S stRoiAttr = {0};
	CVI_U32 u32Index = pstMsg->as32PrivData[0];

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stRoiAttr, sizeof(VENC_ROI_ATTR_S));
	s32Ret = CVI_VENC_GetRoiAttr(VeChn, u32Index, &stRoiAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetRoiAttr fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stRoiAttr, sizeof(VENC_ROI_ATTR_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stRoiAttr, sizeof(VENC_ROI_ATTR_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetH264Trans(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H264_TRANS_S *pstH264Trans = (VENC_H264_TRANS_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstH264Trans, sizeof(VENC_H264_TRANS_S));

	s32Ret = CVI_VENC_SetH264Trans(VeChn, pstH264Trans);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetH264Trans fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetH264Trans(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H264_TRANS_S stH264Trans = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stH264Trans, sizeof(VENC_H264_TRANS_S));
	s32Ret = CVI_VENC_GetH264Trans(VeChn, &stH264Trans);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH264Trans fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stH264Trans, sizeof(VENC_H264_TRANS_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stH264Trans, sizeof(VENC_H264_TRANS_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetH264Entropy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H264_ENTROPY_S *pstH264EntropyEnc = (VENC_H264_ENTROPY_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstH264EntropyEnc, sizeof(VENC_H264_ENTROPY_S));

	s32Ret = CVI_VENC_SetH264Entropy(VeChn, pstH264EntropyEnc);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetH264Entropy fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetH264Entropy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H264_ENTROPY_S stH264EntropyEnc = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stH264EntropyEnc, sizeof(VENC_H264_ENTROPY_S));
	s32Ret = CVI_VENC_GetH264Entropy(VeChn, &stH264EntropyEnc);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH264Entropy fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stH264EntropyEnc, sizeof(VENC_H264_ENTROPY_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stH264EntropyEnc, sizeof(VENC_H264_ENTROPY_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetJpegParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_JPEG_PARAM_S *pstJpegParam = (VENC_JPEG_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstJpegParam, sizeof(VENC_JPEG_PARAM_S));

	s32Ret = CVI_VENC_SetJpegParam(VeChn, pstJpegParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetJpegParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetJpegParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_JPEG_PARAM_S stJpegParam = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stJpegParam, sizeof(VENC_JPEG_PARAM_S));
	s32Ret = CVI_VENC_GetJpegParam(VeChn, &stJpegParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetJpegParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stJpegParam, sizeof(VENC_JPEG_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stJpegParam, sizeof(VENC_JPEG_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetRcParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_RC_PARAM_S *pstRcParam = (VENC_RC_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstRcParam, sizeof(VENC_RC_PARAM_S));

	s32Ret = CVI_VENC_SetRcParam(VeChn, pstRcParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetRcParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetRcParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_RC_PARAM_S stRcParam = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stRcParam, sizeof(VENC_RC_PARAM_S));
	s32Ret = CVI_VENC_GetRcParam(VeChn, &stRcParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetRcParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stRcParam, sizeof(VENC_RC_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stRcParam, sizeof(VENC_RC_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetRefParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_REF_PARAM_S *pstRefParam = (VENC_REF_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstRefParam, sizeof(VENC_REF_PARAM_S));

	s32Ret = CVI_VENC_SetRefParam(VeChn, pstRefParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetRefParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetRefParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_REF_PARAM_S stRefParam = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stRefParam, sizeof(VENC_REF_PARAM_S));
	s32Ret = CVI_VENC_GetRefParam(VeChn, &stRefParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetRefParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stRefParam, sizeof(VENC_REF_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stRefParam, sizeof(VENC_REF_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetChnParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_PARAM_S *pstChnParam = (VENC_CHN_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstChnParam, sizeof(VENC_CHN_PARAM_S));

	s32Ret = CVI_VENC_SetChnParam(VeChn, pstChnParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetChnParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetChnParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_PARAM_S stChnParam = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stChnParam, sizeof(VENC_CHN_PARAM_S));
	s32Ret = CVI_VENC_GetChnParam(VeChn, &stChnParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetChnParam fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stChnParam, sizeof(VENC_CHN_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnParam, sizeof(VENC_CHN_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetModParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_PARAM_MOD_S *pstModParam = (VENC_PARAM_MOD_S *)pstMsg->pBody;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	UNUSED(VeChn);

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstModParam, sizeof(VENC_PARAM_MOD_S));

	s32Ret = CVI_VENC_SetModParam(pstModParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetModParam fail, ret:0x%x\n",s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail\n");
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail\n");
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetModParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_PARAM_MOD_S *pstModParam = (VENC_PARAM_MOD_S *)pstMsg->pBody;
	VENC_PARAM_MOD_S stModParam = {0};
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	UNUSED(VeChn);

	CVI_MSG_VENC_API_IN;

	stModParam.enVencModType = pstModParam->enVencModType;
	DATA_DUMP(&stModParam, sizeof(VENC_PARAM_MOD_S));
	s32Ret = CVI_VENC_GetModParam(&stModParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("stModParam fail, ret:0x%x\n", s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stModParam, sizeof(VENC_PARAM_MOD_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stModParam, sizeof(VENC_PARAM_MOD_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail\n");
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail\n");
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetFrameLostStrategy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_FRAMELOST_S *pstFrmLostParam = (VENC_FRAMELOST_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstFrmLostParam, sizeof(VENC_FRAMELOST_S));

	s32Ret = CVI_VENC_SetFrameLostStrategy(VeChn, pstFrmLostParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetFrameLostStrategy fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetFrameLostStrategy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_FRAMELOST_S stFrmLostParam = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stFrmLostParam, sizeof(VENC_FRAMELOST_S));
	s32Ret = CVI_VENC_GetFrameLostStrategy(VeChn, &stFrmLostParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetFrameLostStrategy fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stFrmLostParam, sizeof(VENC_FRAMELOST_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stFrmLostParam, sizeof(VENC_FRAMELOST_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_AttachVbPool(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_CHN_POOL_S *pstPool = (VENC_CHN_POOL_S *)pstMsg->pBody;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstPool, sizeof(VENC_CHN_POOL_S));

	s32Ret = CVI_VENC_AttachVbPool(VeChn, pstPool);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("AttachVbPool fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_DetachVbPool(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_DetachVbPool(VeChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("DetachVbPool fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetH264Vui(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	VENC_H264_VUI_S * pstH264Vui = (VENC_H264_VUI_S *)pstMsg->pBody;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstH264Vui, sizeof(VENC_H264_VUI_S));

	s32Ret = CVI_VENC_SetH264Vui(VeChn, pstH264Vui);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetH264Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetH264Vui(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H264_VUI_S stH264Vui = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stH264Vui, sizeof(VENC_H264_VUI_S));
	s32Ret = CVI_VENC_GetH264Vui(VeChn, &stH264Vui);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH264Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stH264Vui, sizeof(VENC_H264_VUI_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stH264Vui, sizeof(VENC_H264_VUI_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_SetH265Vui(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	VENC_H265_VUI_S * pstH265Vui = (VENC_H265_VUI_S *)pstMsg->pBody;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(pstH265Vui, sizeof(VENC_H265_VUI_S));

	s32Ret = CVI_VENC_SetH265Vui(VeChn, pstH265Vui);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetH265Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetH265Vui(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VENC_H265_VUI_S stH265Vui = {0};

	CVI_MSG_VENC_API_IN;

	DATA_DUMP(&stH265Vui, sizeof(VENC_H265_VUI_S));
	s32Ret = CVI_VENC_GetH265Vui(VeChn, &stH265Vui);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH265Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stH265Vui, sizeof(VENC_H265_VUI_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stH265Vui, sizeof(VENC_H265_VUI_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_GetFirmwareStrategy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_S32 fw_size = 0;

	CVI_MSG_VENC_API_IN;

	fw_size = GetFirmwareSize(fw_h264);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH265Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &fw_size, sizeof(CVI_S32));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_Suspend(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_Suspend();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetH265Vui fail, Chn:%d ret:0x%x\n", VeChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VENC_Resume(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VENC_CHN VeChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VENC_API_IN;

	s32Ret = CVI_VENC_Resume();

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VeChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VeChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VENC_API_OUT;

	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stVencCmdTable[] = {
	{ MSG_CMD_VENC_CREATE_CHN,        MSG_VENC_CreateChn      },
	{ MSG_CMD_VENC_DESTROY_CHN,       MSG_VENC_DestroyChn     },
	{ MSG_CMD_VENC_RESET_CHN,         MSG_VENC_ResetChn       },
	{ MSG_CMD_VENC_START_RECV_FRAME,  MSG_VENC_StartRecvFrame },
	{ MSG_CMD_VENC_STOP_RECV_FRAME,   MSG_VENC_StopRecvFrame  },
	{ MSG_CMD_VENC_QUERY_STATUS,      MSG_VENC_QueryStatus    },
	{ MSG_CMD_VENC_SET_CHN_ATTR,      MSG_VENC_SetChnAttr     },
	{ MSG_CMD_VENC_GET_CHN_ATTR,      MSG_VENC_GetChnAttr     },
	{ MSG_CMD_VENC_GET_STREAM,        MSG_VENC_GetStream      },
	{ MSG_CMD_VENC_RELEASE_STREAM,    MSG_VENC_ReleaseStream  },
	{ MSG_CMD_VENC_SEND_FRAME,        MSG_VENC_SendFrame      },
	{ MSG_CMD_VENC_REQUEST_IDR,       MSG_VENC_RequestIDR     },
	{ MSG_CMD_VENC_SET_ROI_ATTR,      MSG_VENC_SetRoiAttr     },
	{ MSG_CMD_VENC_GET_ROI_ATTR,      MSG_VENC_GetRoiAttr     },
	{ MSG_CMD_VENC_SET_H264_TRANS,    MSG_VENC_SetH264Trans   },
	{ MSG_CMD_VENC_GET_H264_TRANS,    MSG_VENC_GetH264Trans   },
	{ MSG_CMD_VENC_SET_H264_ENTROPY,  MSG_VENC_SetH264Entropy },
	{ MSG_CMD_VENC_GET_H264_ENTROPY,  MSG_VENC_GetH264Entropy },
	{ MSG_CMD_VENC_SET_JPEG_PARAM,    MSG_VENC_SetJpegParam   },
	{ MSG_CMD_VENC_GET_JPEG_PARAM,    MSG_VENC_GetJpegParam   },
	{ MSG_CMD_VENC_SET_RC_PARAM,      MSG_VENC_SetRcParam     },
	{ MSG_CMD_VENC_GET_RC_PARAM,      MSG_VENC_GetRcParam     },
	{ MSG_CMD_VENC_SET_REF_PARAM,     MSG_VENC_SetRefParam    },
	{ MSG_CMD_VENC_GET_REF_PARAM,     MSG_VENC_GetRefParam    },
	{ MSG_CMD_VENC_SET_CHN_PARAM,     MSG_VENC_SetChnParam    },
	{ MSG_CMD_VENC_GET_CHN_PARAM,     MSG_VENC_GetChnParam    },
	{ MSG_CMD_VENC_SET_MOD_PARAM,     MSG_VENC_SetModParam    },
	{ MSG_CMD_VENC_GET_MOD_PARAM,     MSG_VENC_GetModParam    },
	{ MSG_CMD_VENC_SET_FRAME_LOST,    MSG_VENC_SetFrameLostStrategy    },
	{ MSG_CMD_VENC_GET_FRAME_LOST,    MSG_VENC_GetFrameLostStrategy    },
	{ MSG_CMD_VENC_ATTACH_VBPOOL,	  MSG_VENC_AttachVbPool   },
	{ MSG_CMD_VENC_DETACH_VBPOOL,	  MSG_VENC_DetachVbPool   },
	{ MSG_CMD_VENC_SET_H264VUI,	      MSG_VENC_SetH264Vui     },
	{ MSG_CMD_VENC_GET_H264VUI,	      MSG_VENC_GetH264Vui     },
	{ MSG_CMD_VENC_SET_H265VUI,	      MSG_VENC_SetH265Vui     },
	{ MSG_CMD_VENC_GET_H265VUI,	      MSG_VENC_GetH265Vui     },
	{ MSG_CMD_VENC_GET_FIRMWARE_STRATEGY, MSG_VENC_GetFirmwareStrategy },
	{ MSG_CMD_VENC_SUSPEND,           MSG_VENC_Suspend        },
	{ MSG_CMD_VENC_RESUME,            MSG_VENC_Resume         }
};

MSG_SERVER_MODULE_S g_stModuleVenc = {
	CVI_ID_VENC,
	"venc",
	sizeof(g_stVencCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stVencCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetVencMod(CVI_VOID)
{
	return &g_stModuleVenc;
}

