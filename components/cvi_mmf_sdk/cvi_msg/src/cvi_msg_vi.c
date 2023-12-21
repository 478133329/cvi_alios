#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_vi.h"
#include "cvi_sys.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"

#include "cvi_isp.h"
#include "cvi_comm_isp.h"
#include "cvi_comm_3a.h"
#include "cvi_awb_comm.h"
#include "cvi_ae.h"
#include "vi_isp.h"
#include "vi_tun_cfg.h"
#include "cvi_sns_ctrl.h"
#include "cvi_mw_base.h"
#include "vi_ioctl.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define CHECK_MSG_SIZE(type, size)												\
	if (sizeof(type) != size) {												\
		CVI_TRACE_MSG("size mismatch !!! expect size: %lu, actual size: %u\n", sizeof(type), size);			\
		return CVI_FAILURE;												\
	}

static CVI_S32 MSG_VI_SetDevAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_DEV_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetDevAttr(ViDev, (VI_DEV_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetDevAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetDevAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV_ATTR_S  stDevAttr;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_DEV_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetDevAttr(ViDev, &stDevAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetDevAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stDevAttr, sizeof(VI_DEV_ATTR_S));
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

static CVI_S32 MSG_VI_EnableDev(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_EnableDev(ViDev);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_EnableDev Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_DisableDev(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_DisableDev(ViDev);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_DisableDev Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_CreatePipe(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_PIPE_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_CreatePipe(ViPipe, (VI_PIPE_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_CreatePipe Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_DestroyPipe(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_DestroyPipe(ViPipe);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_DestroyPipe Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_SetPipeAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_PIPE_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetPipeAttr(ViDev, (VI_PIPE_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetPipeAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetPipeAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE_ATTR_S  stPipeAttr;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_PIPE_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetPipeAttr(ViDev, &stPipeAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetPipeAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stPipeAttr, sizeof(VI_PIPE_ATTR_S));
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

static CVI_S32 MSG_VI_SetPipeDumpAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_DUMP_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetPipeDumpAttr(ViDev, (VI_DUMP_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetPipeDumpAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetPipeDumpAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DUMP_ATTR_S pstDumpAttr;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_DUMP_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetPipeDumpAttr(ViDev, &pstDumpAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetPipeDumpAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &pstDumpAttr, sizeof(VI_DUMP_ATTR_S));
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

static CVI_S32 MSG_VI_StartPipe(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_StartPipe(ViPipe);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_StartPipe Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_StopPipe(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_StopPipe(ViPipe);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_StopPipe Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_SetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_CHN_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetChnAttr(ViDev, ViChn, (VI_CHN_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetChnAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_CHN_ATTR_S  stChnAttr;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_CHN_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetChnAttr(ViDev, ViChn, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetChnAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnAttr, sizeof(VI_CHN_ATTR_S));
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

static CVI_S32 MSG_VI_EnableChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_EnableChn(ViPipe, ViChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_EnableChn Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_DisableChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_DisableChn(ViPipe, ViChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_DisableChn Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];
	VIDEO_FRAME_INFO_S stFrameInfo = {0};

	s32Ret = CVI_VI_GetChnFrame(ViPipe, ViChn, &stFrameInfo, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetChnFrame Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stFrameInfo, sizeof(stFrameInfo));
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

static CVI_S32 MSG_VI_ReleaseChnFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VIDEO_FRAME_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_ReleaseChnFrame(ViPipe, ViChn, (VIDEO_FRAME_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_ReleaseChnFrame dev:%d chn:%d Failed : %#x!\n", ViPipe, ViChn, s32Ret);
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

static CVI_S32 MSG_VI_GetPipeFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];
	VIDEO_FRAME_INFO_S stFrameInfo[2] = {0};

	memcpy(stFrameInfo, pstMsg->pBody, sizeof(VIDEO_FRAME_INFO_S) * 2);
	s32Ret = CVI_VI_GetPipeFrame(ViPipe, stFrameInfo, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetPipeFrame Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stFrameInfo, sizeof(VIDEO_FRAME_INFO_S) * 2);
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

static CVI_S32 MSG_VI_ReleasePipeFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VIDEO_FRAME_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_ReleasePipeFrame(ViPipe, (VIDEO_FRAME_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_ReleasePipe Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_SetChnCrop(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_CROP_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetChnCrop(ViDev, ViChn, (VI_CROP_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetChnCrop Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnCrop(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_CROP_INFO_S  stCropInfo;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_CROP_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetChnCrop(ViDev, ViChn, &stCropInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetChnCrop Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stCropInfo, sizeof(VI_CROP_INFO_S));
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

static CVI_S32 MSG_VI_DumpHwRegisterToFile(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_DUMP_REGISTER_TABLE_S	reg_tbl;
	ISP_INNER_STATE_INFO_S		*pstInnerStateInfo;
	CVI_U64 phyAddr = ((((uint64_t)0xFFFFFFFF) & pstMsg->as32PrivData[1]) << 32)
				| (((uint64_t)0xFFFFFFFF) & pstMsg->as32PrivData[0]);
	CVI_U32 size = pstMsg->as32PrivData[2];

	//dual_os linux reg_tbl not set
	pstInnerStateInfo = (ISP_INNER_STATE_INFO_S *)malloc(sizeof(ISP_INNER_STATE_INFO_S));
	if (pstInnerStateInfo == NULL) {
		CVI_TRACE_LOG(CVI_DBG_ERR, "malloc fail");
		return CVI_FAILURE;
	}

	if (CVI_ISP_QueryInnerStateInfo(ViPipe, pstInnerStateInfo) != CVI_SUCCESS) {
		CVI_TRACE_LOG(CVI_DBG_ERR, "CVI_ISP_QueryInnerStateInfo fail");
		return CVI_FAILURE;
	}

	reg_tbl.MlscGainLut.RGain = pstInnerStateInfo->mlscGainTable.RGain;
	reg_tbl.MlscGainLut.GGain = pstInnerStateInfo->mlscGainTable.GGain;
	reg_tbl.MlscGainLut.BGain = pstInnerStateInfo->mlscGainTable.BGain;

	memset((void *)phyAddr, 0, size);
	CVI_SYS_IonFlushCache(phyAddr, (void *)phyAddr, size);

	s32Ret = CVI_VI_DumpHwRegisterToFile(ViPipe, (void *)phyAddr, &reg_tbl);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("DumpHwRegisterToFile Failed : %#x!\n", s32Ret);
	}
	free(pstInnerStateInfo);
	CVI_SYS_IonFlushCache(phyAddr, (void *)phyAddr, size);

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

static CVI_S32 MSG_VI_SetDevTimingAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_DEV_TIMING_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetDevTimingAttr(ViDev, (VI_DEV_TIMING_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetDevTimingAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetDevTimingAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_DEV_TIMING_ATTR_S stTimingAttr;

	s32Ret = CVI_VI_GetDevTimingAttr(ViDev, &stTimingAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetDevTimingAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stTimingAttr, sizeof(VI_DEV_TIMING_ATTR_S));
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

static CVI_S32 MSG_VI_SetPipeFrameSource(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_SetPipeFrameSource(ViPipe, *(VI_PIPE_FRAME_SOURCE_E *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_EnableChn Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetPipeFrameSource(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_PIPE_FRAME_SOURCE_E enSoure;

	s32Ret = CVI_VI_GetPipeFrameSource(ViPipe, &enSoure);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_DisableChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &enSoure, sizeof(VI_PIPE_FRAME_SOURCE_E));
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

static CVI_S32 MSG_VI_SendPipeRaw(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 i = 0;
	CVI_S32 s32MilliSec = 0;
	VI_PIPE PipeId[VI_MAX_PIPE_NUM] = {0};
	const VIDEO_FRAME_INFO_S *pstVideoFrame[VI_MAX_PIPE_NUM];
	VIDEO_FRAME_INFO_S stVideoFrame[VI_MAX_PIPE_NUM];

	memcpy(&stVideoFrame, pstMsg->pBody, sizeof(stVideoFrame[VI_MAX_PIPE_NUM]));

	CVI_U32 u32PipeNum = (CVI_U32)pstMsg->as32PrivData[0];

	for (; i < u32PipeNum; ++i) {
		PipeId[i] = pstMsg->as32PrivData[i + 1];
		pstVideoFrame[i] = &stVideoFrame[i];
	}

	s32MilliSec = pstMsg->as32PrivData[i + 1];

	s32Ret = CVI_VI_SendPipeRaw(u32PipeNum, PipeId, pstVideoFrame, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_EnableChn Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_StartSmoothRawDump(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_SMOOTH_RAW_DUMP_INFO_S stDumpInfo;
	CVI_U64 phyAddr = ((((uint64_t)0xFFFFFFFF) & pstMsg->as32PrivData[1]) << 32)
				| (((uint64_t)0xFFFFFFFF) & pstMsg->as32PrivData[0]);

	CHECK_MSG_SIZE(VI_SMOOTH_RAW_DUMP_INFO_S, pstMsg->u32BodyLen);

	stDumpInfo = *(VI_SMOOTH_RAW_DUMP_INFO_S *)pstMsg->pBody;
	stDumpInfo.phy_addr_list = (CVI_U64 *)phyAddr;

	s32Ret = CVI_VI_StartSmoothRawDump(&stDumpInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StartSmoothRawDump Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_StopSmoothRawDump(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CHECK_MSG_SIZE(VI_SMOOTH_RAW_DUMP_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_StopSmoothRawDump((VI_SMOOTH_RAW_DUMP_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("StopSmoothRawDump Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetSmoothRawDump(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];
	VIDEO_FRAME_INFO_S stVideoFrame[2] = {0};

	memcpy(stVideoFrame, pstMsg->pBody, sizeof(VIDEO_FRAME_INFO_S) * 2);
	s32Ret = CVI_VI_GetSmoothRawDump(ViPipe, stVideoFrame, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetSmoothRawDump Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stVideoFrame, sizeof(VIDEO_FRAME_INFO_S) * 2);
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

static CVI_S32 MSG_VI_PutSmoothRawDump(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_PutSmoothRawDump(ViPipe, (VIDEO_FRAME_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("PutSmoothRawDump Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetDevNum(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	CVI_U32 devNum;

	s32Ret = CVI_VI_GetDevNum(&devNum);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_GetDevNum Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &devNum, sizeof(CVI_U32));
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

static CVI_S32 MSG_VI_SetPipeCrop(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(CROP_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetPipeCrop(ViDev, (CROP_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_SetPipeCrop Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetPipeCrop(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CROP_INFO_S  stCropInfo;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(CROP_INFO_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetPipeCrop(ViDev, &stCropInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_GetPipeCrop Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stCropInfo, sizeof(CROP_INFO_S));
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

static CVI_S32 MSG_VI_SetChnRotation(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);
	ROTATION_E enRotation;

	CHECK_MSG_SIZE(ROTATION_E, pstMsg->u32BodyLen);

	enRotation = *(ROTATION_E *)pstMsg->pBody;
	s32Ret = CVI_VI_SetChnRotation(ViDev, ViChn, enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_SetChnRotation Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnRotation(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);
	ROTATION_E enRotation;

	s32Ret = CVI_VI_GetChnRotation(ViPipe, ViChn, &enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_GetChnRotation Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &enRotation, sizeof(ROTATION_E));
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

static CVI_S32 MSG_VI_SetChnFlipMirror(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_BOOL bFlip, bMirror;

	bFlip = pstMsg->as32PrivData[0];
	bMirror = pstMsg->as32PrivData[1];

	s32Ret = CVI_VI_SetChnFlipMirror(ViDev, ViChn, bFlip, bMirror);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_SetChnRotation Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnFlipMirror(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_BOOL bFlip, bMirror;
	CVI_U32 *bMirrFilp = (CVI_U32 *)pstMsg->pBody;

	s32Ret = CVI_VI_GetChnFlipMirror(ViPipe, ViChn, &bFlip, &bMirror);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("VI_GetChnFlipMirror Failed : %#x!\n", s32Ret);
	}

	*bMirrFilp = bFlip;
	*(bMirrFilp + 1) = bMirror;

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, bMirrFilp, sizeof(CVI_U32) * 2);
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


static CVI_S32 MSG_VI_GetRgbMapLeBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	struct cvi_vip_memblock rgbMapBuf;

	s32Ret = CVI_VI_GetRgbMapLeBuf(ViPipe, &rgbMapBuf);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetRgbMapLeBuf Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &rgbMapBuf, sizeof(struct cvi_vip_memblock));
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

static CVI_S32 MSG_VI_GetRgbMapSeBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
	struct cvi_vip_memblock rgbMapBuf;

	s32Ret = CVI_VI_GetRgbMapSeBuf(ViPipe, &rgbMapBuf);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("GetRgbMapSeBuf Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &rgbMapBuf, sizeof(struct cvi_vip_memblock));
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

static CVI_S32 MSG_VI_QueryPipeStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE_STATUS_S  stStatus;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_PIPE_STATUS_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_QueryPipeStatus(ViDev, &stStatus);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_QueryPipeStatus Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stStatus, sizeof(VI_PIPE_STATUS_S));
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

static CVI_S32 MSG_VI_QueryChnStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_CHN_STATUS_S  stChnStatus;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_CHN_STATUS_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_QueryChnStatus(ViDev, ViChn, &stChnStatus);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_QueryChnStatus Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnStatus, sizeof(VI_CHN_STATUS_S));
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

static CVI_S32 MSG_VI_SetBypassFrm(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VI_SetBypassFrm(ViPipe, *(CVI_U8 *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetBypassFrm Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_SUSPEND(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_VI_Suspend();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_Suspend Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_RESUME(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_VI_Resume();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_Resume Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_SetChnLDCAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_LDC_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_SetChnLDCAttr(ViDev, ViChn, (VI_LDC_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_SetChnLDCAttr Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_VI_GetChnLDCAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_LDC_ATTR_S  stLDCAttr;
	VI_DEV ViDev = GET_DEV_ID(pstMsg->u32Module);
	VI_CHN ViChn = GET_CHN_ID(pstMsg->u32Module);

	CHECK_MSG_SIZE(VI_LDC_ATTR_S, pstMsg->u32BodyLen);

	s32Ret = CVI_VI_GetChnLDCAttr(ViDev, ViChn, &stLDCAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VI_GetChnLDCAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stLDCAttr, sizeof(VI_CHN_ATTR_S));
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

static CVI_S32 MSG_VI_DbgSetTuningDis(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct vdev *d;

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		s32Ret = CVI_FAILURE;
	}

	s32Ret = vi_set_tuning_dis(d->fd, (CVI_S32 *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("vi_set_tuning_dis Failed : %#x!\n", s32Ret);
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

static MSG_MODULE_CMD_S g_stViCmdTable[] = {
	{ MSG_CMD_VI_SET_DEV_ATTR,		MSG_VI_SetDevAttr },
	{ MSG_CMD_VI_GET_DEV_ATTR,		MSG_VI_GetDevAttr },
	{ MSG_CMD_VI_ENABLE_DEV,		MSG_VI_EnableDev },
	{ MSG_CMD_VI_DISABLE_DEV,		MSG_VI_DisableDev },
	{ MSG_CMD_VI_CREATE_PIPE,		MSG_VI_CreatePipe },
	{ MSG_CMD_VI_DESTROY_PIPE,		MSG_VI_DestroyPipe },
	{ MSG_CMD_VI_SET_PIPE_ATTR,		MSG_VI_SetPipeAttr },
	{ MSG_CMD_VI_GET_PIPE_ATTR,		MSG_VI_GetPipeAttr },
	{ MSG_CMD_VI_SET_PIPE_DUMP_ATTR,	MSG_VI_SetPipeDumpAttr },
	{ MSG_CMD_VI_GET_PIPE_DUMP_ATTR,	MSG_VI_GetPipeDumpAttr },
	{ MSG_CMD_VI_START_PIPE,		MSG_VI_StartPipe },
	{ MSG_CMD_VI_STOP_PIPE,			MSG_VI_StopPipe },
	{ MSG_CMD_VI_SET_CHN_ATTR,		MSG_VI_SetChnAttr },
	{ MSG_CMD_VI_GET_CHN_ATTR,		MSG_VI_GetChnAttr },
	{ MSG_CMD_VI_ENABLE_CHN,		MSG_VI_EnableChn },
	{ MSG_CMD_VI_DISABLE_CHN,		MSG_VI_DisableChn },
	{ MSG_CMD_VI_GET_CHN_FRAME,		MSG_VI_GetChnFrame },
	{ MSG_CMD_VI_RELEASE_CHN_FRAME,		MSG_VI_ReleaseChnFrame },
	{ MSG_CMD_VI_GET_PIPE_FRAME,		MSG_VI_GetPipeFrame },
	{ MSG_CMD_VI_RELEASE_PIPE_FRAME,	MSG_VI_ReleasePipeFrame },
	{ MSG_CMD_VI_SET_CHN_CROP,		MSG_VI_SetChnCrop },
	{ MSG_CMD_VI_GET_CHN_CROP,		MSG_VI_GetChnCrop },
	{ MSG_CMD_VI_DUMP_HW_REG_TO_FILE,	MSG_VI_DumpHwRegisterToFile },
	{ MSG_CMD_VI_SET_DEV_TIMING_ATTR,	MSG_VI_SetDevTimingAttr},
	{ MSG_CMD_VI_GET_DEV_TIMING_ATTR,	MSG_VI_GetDevTimingAttr},
	{ MSG_CMD_VI_SET_PIPE_FRM_SRC,		MSG_VI_SetPipeFrameSource},
	{ MSG_CMD_VI_GET_PIPE_FRM_SRC,		MSG_VI_GetPipeFrameSource},
	{ MSG_CMD_VI_SEND_PIPE_RAW,		MSG_VI_SendPipeRaw},
	{ MSG_CMD_VI_START_SMOOTH_RAWDUMP,	MSG_VI_StartSmoothRawDump},
	{ MSG_CMD_VI_STOP_SMOOTH_RAWDUMP,	MSG_VI_StopSmoothRawDump},
	{ MSG_CMD_VI_GET_SMOOTH_RAWDUMP,	MSG_VI_GetSmoothRawDump},
	{ MSG_CMD_VI_PUT_SMOOTH_RAWDUMP,	MSG_VI_PutSmoothRawDump},
	{ MSG_CMD_VI_GET_DEV_NUM,		MSG_VI_GetDevNum },
	{ MSG_CMD_VI_SET_PIPE_CROP,		MSG_VI_SetPipeCrop },
	{ MSG_CMD_VI_GET_PIPE_CROP,		MSG_VI_GetPipeCrop },
	{ MSG_CMD_VI_SET_CHN_ROTATION,		MSG_VI_SetChnRotation },
	{ MSG_CMD_VI_GET_CHN_ROTATION,		MSG_VI_GetChnRotation },
	{ MSG_CMD_VI_SET_CHN_FLIP_MIRROR,	MSG_VI_SetChnFlipMirror },
	{ MSG_CMD_VI_GET_CHN_FLIP_MIRROR,	MSG_VI_GetChnFlipMirror },
	{ MSG_CMD_VI_GET_RGBMAP_LE_BUF,		MSG_VI_GetRgbMapLeBuf },
	{ MSG_CMD_VI_GET_RGBMAP_SE_BUF,		MSG_VI_GetRgbMapSeBuf },
	{ MSG_CMD_VI_GET_PIPE_STATUS,		MSG_VI_QueryPipeStatus},
	{ MSG_CMD_VI_GET_CHN_STATUS,		MSG_VI_QueryChnStatus},
	{ MSG_CMD_VI_SET_BYPASS_FRM,		MSG_VI_SetBypassFrm},
	{ MSG_CMD_VI_SUSPEND,			MSG_VI_SUSPEND},
	{ MSG_CMD_VI_RESUME,			MSG_VI_RESUME},
	{ MSG_CMD_VI_SET_CHN_LDC_ATTR,		MSG_VI_SetChnLDCAttr},
	{ MSG_CMD_VI_GET_CHN_LDC_ATTR,		MSG_VI_GetChnLDCAttr},
	{ MSG_CMD_VI_DBG_SET_TUNING_DIS,	MSG_VI_DbgSetTuningDis},
};

MSG_SERVER_MODULE_S g_stModuleVi = {
	CVI_ID_VI,
	"vi",
	sizeof(g_stViCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stViCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetViMod(CVI_VOID)
{
	return &g_stModuleVi;
}

