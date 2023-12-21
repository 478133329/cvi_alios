#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_sys.h"
#include "cvi_vdec.h"
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

CVI_S32 CVI_VDEC_GetDataFifoAddr(VENC_CHN VdChn, CVI_U64 *pu64PhyAddr);

//#define FLOW_DEBUG 1
#ifdef FLOW_DEBUG
#define CVI_MSG_API(msg, ...)		\
	do { \
		if (1) \
			printf("[API] %s = %d, "msg, __func__, __LINE__, ## __VA_ARGS__); \
	} while (0)

#define CVI_MSG_VDEC_API_IN  CVI_MSG_API("Chn:%d In\n", VdChn);
#define CVI_MSG_VDEC_API_OUT CVI_MSG_API("Chn:%d Out\n", VdChn);

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
#define CVI_MSG_VDEC_API_IN
#define CVI_MSG_VDEC_API_OUT
#define DATA_DUMP(data, len)
#endif

CVI_S32 CVI_VDEC_IonAlloc(VDEC_CHN VdChn, uint64_t *paddr, CVI_S32 size);
CVI_S32 CVI_VDEC_IonFree(VDEC_CHN VdChn ,uint64_t paddr);
int GetFirmwareSize(uint8_t *fw_addr);

static CVI_S32 vdec_handle_firmware (void *pMsg)
{
	CVI_IPCMSG_MESSAGE_S *pstMsg = (CVI_IPCMSG_MESSAGE_S *)pMsg;
	VDEC_CHN_ATTR_S *pstAttr = (VDEC_CHN_ATTR_S *)pstMsg->pBody;
	CVI_U64 u64FwAddr = 0;

	//firmware load in yoc.bin
	if (GetFirmwareSize(fw_h264) > 10)
		return CVI_SUCCESS;

	if (pstAttr->enType == PT_H264 && pstMsg->as32PrivData[0]) {
		memcpy(&u64FwAddr, &pstMsg->as32PrivData[1], sizeof(CVI_U64));
		fw_h264_size = pstMsg->as32PrivData[0];

		fw_h264_flie = (uint8_t *)calloc(1, fw_h264_size);
		if (fw_h264_flie == NULL) {
			CVI_TRACE_MSG("allocation fail, firmware !\n");
			return CVI_FAILURE;
		}

		inv_dcache_range(u64FwAddr, fw_h264_size);
		memcpy(fw_h264_flie, (CVI_U8 *)u64FwAddr, fw_h264_size);
		flush_dcache_range((uintptr_t)fw_h264_flie, fw_h264_size);
	} else if (pstAttr->enType == PT_H265 && pstMsg->as32PrivData[0]) {
		memcpy(&u64FwAddr, &pstMsg->as32PrivData[1], sizeof(CVI_U64));
		fw_h265_size = pstMsg->as32PrivData[0];

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

static CVI_S32 MSG_VDEC_CreateChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_ATTR_S *pstAttr = (VDEC_CHN_ATTR_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstAttr, sizeof(VDEC_CHN_ATTR_S));

	s32Ret = vdec_handle_firmware(pstMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("load fw fail, ret:0x%x\n", s32Ret);
		return CVI_FAILURE;
	}

	s32Ret = CVI_VDEC_CreateChn(VdChn, pstAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateChn fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_DestroyChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_DestroyChn(VdChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("DestroyChn fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
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
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_GetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_ATTR_S stAttr = {0};

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(&stAttr, sizeof(VDEC_CHN_ATTR_S));
	s32Ret = CVI_VDEC_GetChnAttr(VdChn, &stAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetChnAttr fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stAttr, sizeof(VDEC_CHN_ATTR_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stAttr, sizeof(VDEC_CHN_ATTR_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_SetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_ATTR_S *pstAttr = (VDEC_CHN_ATTR_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstAttr, sizeof(VDEC_CHN_ATTR_S));

	s32Ret = CVI_VDEC_SetChnAttr(VdChn, pstAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetChnAttr fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_StartRecvStream(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U64 u64PhyAddr;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_StartRecvStream(VdChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StartRecvStream fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	s32Ret = CVI_VDEC_GetDataFifoAddr(VdChn, &u64PhyAddr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetDataFifoAddr fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	memcpy(respMsg->as32PrivData, &u64PhyAddr, sizeof(u64PhyAddr));

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_StopRecvStream(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_StopRecvStream(VdChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StopRecvStream fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_ResetChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_ResetChn(VdChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("ResetChn fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_SetChnParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_PARAM_S *pstParam = (VDEC_CHN_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstParam, sizeof(VDEC_CHN_PARAM_S));

	s32Ret = CVI_VDEC_SetChnParam(VdChn, pstParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetChnParam fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_GetChnParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_PARAM_S stParam = {0};

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(&stParam, sizeof(VDEC_CHN_PARAM_S));
	s32Ret = CVI_VDEC_GetChnParam(VdChn, &stParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetChnParam fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stParam, sizeof(VDEC_CHN_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stParam, sizeof(VDEC_CHN_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_SendStream(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_STREAM_S *pstStream = (VDEC_STREAM_S *)pstMsg->pBody;
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];
	memcpy(&pstStream->u64PTS, &pstMsg->as32PrivData[1], sizeof(pstStream->u64PTS));

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstStream, sizeof(VDEC_STREAM_S));

	pstStream->pu8Addr = (void *)pstStream->u64PTS;
	CVI_SYS_IonInvalidateCache(pstStream->u64PTS, pstStream->pu8Addr, pstStream->u32Len);

	s32Ret = CVI_VDEC_SendStream(VdChn, pstStream, s32MilliSec);
	if (s32Ret != CVI_SUCCESS && s32Ret != CVI_ERR_VDEC_BUF_FULL) {
		CVI_TRACE_MSG("SendStream fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_GetFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VIDEO_FRAME_INFO_S stFrameInfo = {0};
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(&stFrameInfo, sizeof(VIDEO_FRAME_INFO_S));
	s32Ret = CVI_VDEC_GetFrame(VdChn, &stFrameInfo, s32MilliSec);
	if (s32Ret != CVI_SUCCESS && s32Ret != CVI_ERR_VDEC_ERR_INVALID_RET) {
		CVI_TRACE_MSG("GetFrame fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stFrameInfo, sizeof(VIDEO_FRAME_INFO_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stFrameInfo, sizeof(VIDEO_FRAME_INFO_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_ReleaseFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VIDEO_FRAME_INFO_S *pstFrameInfo = (VIDEO_FRAME_INFO_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstFrameInfo, sizeof(VIDEO_FRAME_INFO_S));

	s32Ret = CVI_VDEC_ReleaseFrame(VdChn, pstFrameInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("ReleaseFrame fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_AttachVbPool(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_POOL_S *pstPool = (VDEC_CHN_POOL_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstPool, sizeof(VDEC_CHN_POOL_S));

	s32Ret = CVI_VDEC_AttachVbPool(VdChn, pstPool);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("AttachVbPool fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_DetachVbPool(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_DetachVbPool(VdChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("DetachVbPool fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_SetModParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_MOD_PARAM_S *pstModParam = (VDEC_MOD_PARAM_S *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(pstModParam, sizeof(VDEC_MOD_PARAM_S));

	s32Ret = CVI_VDEC_SetModParam(pstModParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetModParam fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_GetModParam(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_MOD_PARAM_S stModParam = {0};
	UNUSED(VdChn);

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(&stModParam, sizeof(VDEC_MOD_PARAM_S));
	s32Ret = CVI_VDEC_GetModParam(&stModParam);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetModParam fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stModParam, sizeof(VDEC_MOD_PARAM_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stModParam, sizeof(VDEC_MOD_PARAM_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_QueryStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VDEC_CHN_STATUS_S stStatus = {0};

	CVI_MSG_VDEC_API_IN;

	DATA_DUMP(&stStatus, sizeof(VDEC_CHN_STATUS_S));
	s32Ret = CVI_VDEC_QueryStatus(VdChn, &stStatus);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("QueryStatus fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}
	DATA_DUMP(&stStatus, sizeof(VDEC_CHN_STATUS_S));

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stStatus, sizeof(VDEC_CHN_STATUS_S));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_IonAlloc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_S32 IonSize = pstMsg->as32PrivData[0];
	uint64_t paddr = 0;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_IonAlloc(VdChn, &paddr, IonSize);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IonAlloc fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &paddr, sizeof(uint64_t));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_IonFree(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	uint64_t *paddr = (uint64_t *)pstMsg->pBody;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_IonFree(VdChn , *paddr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IonFree fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_GetFirmwareStrategy(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_S32 fw_size = 0;

	CVI_MSG_VDEC_API_IN;

	fw_size = GetFirmwareSize(fw_h264);

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &fw_size, sizeof(CVI_S32));
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_Suspend(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_Suspend();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VDEC_Suspend fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static CVI_S32 MSG_VDEC_Resume(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	VDEC_CHN VdChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_MSG_VDEC_API_IN;

	s32Ret = CVI_VDEC_Resume();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VDEC_Suspend fail, Chn:%d ret:0x%x\n", VdChn, s32Ret);
		// don't return, send s32Ret to client
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (!respMsg) {
		CVI_TRACE_MSG("CreateRespMessage fail, Chn:%d\n", VdChn);
		return CVI_FAILURE;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("IPCMSG_SendOnly fail, Chn:%d\n", VdChn);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return CVI_FAILURE;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	CVI_MSG_VDEC_API_OUT;

	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stVdecCmdTable[] = {
	{ MSG_CMD_VDEC_CREATE_CHN,	    MSG_VDEC_CreateChn     },
	{ MSG_CMD_VDEC_DESTROY_CHN,	    MSG_VDEC_DestroyChn    },
	{ MSG_CMD_VDEC_GET_CHN_ATTR,    MSG_VDEC_GetChnAttr    },
	{ MSG_CMD_VDEC_SET_CHN_ATTR,    MSG_VDEC_SetChnAttr    },
	{ MSG_CMD_VDEC_START_RECV_FRAME,         MSG_VDEC_StartRecvStream       },
	{ MSG_CMD_VDEC_STOP_RECV_FRAME,          MSG_VDEC_StopRecvStream        },
	{ MSG_CMD_VDEC_RESET_CHN,       MSG_VDEC_ResetChn      },
	{ MSG_CMD_VDEC_SET_CHN_PARAM,   MSG_VDEC_SetChnParam   },
	{ MSG_CMD_VDEC_GET_CHN_PARAM,   MSG_VDEC_GetChnParam   },
	{ MSG_CMD_VDEC_SEND_STREAM,     MSG_VDEC_SendStream    },
	{ MSG_CMD_VDEC_GET_FRAME,       MSG_VDEC_GetFrame      },
	{ MSG_CMD_VDEC_RELEASE_FRAME,   MSG_VDEC_ReleaseFrame  },
	{ MSG_CMD_VDEC_ATTACH_VBPOOL,   MSG_VDEC_AttachVbPool  },
	{ MSG_CMD_VDEC_DETACH_VBPOOL,   MSG_VDEC_DetachVbPool  },
	{ MSG_CMD_VDEC_SET_MOD_PARAM,   MSG_VDEC_SetModParam   },
	{ MSG_CMD_VDEC_GET_MOD_PARAM,   MSG_VDEC_GetModParam   },
	{ MSG_CMD_VDEC_QUERY_STATUS,    MSG_VDEC_QueryStatus   },
	{ MSG_CMD_VDEC_ION_ALLOC,       MSG_VDEC_IonAlloc      },
	{ MSG_CMD_VDEC_ION_FREE,        MSG_VDEC_IonFree       },
	{ MSG_CMD_VDEC_GET_FIRMWARE_STRATEGY,    MSG_VDEC_GetFirmwareStrategy    },
	{ MSG_CMD_VDEC_SUSPEND,         MSG_VDEC_Suspend       },
	{ MSG_CMD_VDEC_RESUME,          MSG_VDEC_Resume        }
};


MSG_SERVER_MODULE_S g_stModuleVdec = {
	CVI_ID_VDEC,
	"vdec",
	sizeof(g_stVdecCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stVdecCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetVdecMod(CVI_VOID)
{
	return &g_stModuleVdec;
}

