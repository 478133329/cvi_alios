#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_vo.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"
#include "cvi_comm_video.h"
#include "cvi_vo_ctx.h"

static CVI_S32 MSG_VO_SetPubAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_DEV VoDev = GET_DEV_ID(pstMsg->u32Module);
	//VO_PUB_ATTR_S *pstPubAttr;

	s32Ret = CVI_VO_SetPubAttr(VoDev, (VO_PUB_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetPubAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetPubAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetPubAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetPubAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_PUB_ATTR_S vo_pup_attr;
	VO_DEV VoDev = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_GetPubAttr(VoDev, &vo_pup_attr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetPubAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &vo_pup_attr, sizeof(vo_pup_attr));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetPubAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetPubAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_Enable(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_DEV VoDev = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_Enable(VoDev);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_Enable Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSGs_VO_Enable call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSGs_VO_Enable call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_Disable(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_DEV VoDev = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_Disable(VoDev);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_Disable Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_Disable call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_Disable call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_EnableVideoLayer(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_EnableVideoLayer(VoLayer);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_EnableVideoLayer Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_EnableVideoLayer call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_EnableVideoLayer call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_DisableVideoLayer(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_DisableVideoLayer(VoLayer);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_DisableVideoLayer Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_DisableVideoLayer call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_DisableVideoLayer call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SetVideoLayerAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_SetVideoLayerAttr(VoLayer, (VO_VIDEO_LAYER_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetVideoLayerAttr:%d Failed : %#x!\n", VoLayer, s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetVideoLayerAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetVideoLayerAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetVideoLayerAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_VIDEO_LAYER_ATTR_S stLayerAttr;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_GetVideoLayerAttr(VoLayer, &stLayerAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetVideoLayerAttr:%d Failed : %#x!\n", VoLayer, s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stLayerAttr, sizeof(stLayerAttr));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_GetVideoLayerAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_GetVideoLayerAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_SetChnAttr(VoLayer, VoChn, (VO_CHN_ATTR_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetChnAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetChnAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetChnAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetChnAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_CHN_ATTR_S stChnAttr;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_GetChnAttr(VoLayer, VoChn, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetChnAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnAttr, sizeof(stChnAttr));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_GetChnAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_GetChnAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SetDisplayBufLen(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	CVI_U32 BufLen = pstMsg->as32PrivData[0];

	s32Ret = CVI_VO_SetDisplayBufLen(VoLayer, BufLen);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetDisplayBufLen Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetDisplayBufLen call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetDisplayBufLen call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetDisplayBufLen(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 BufLen;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_GetDisplayBufLen(VoLayer, &BufLen);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetDisplayBufLen VoLayer:%d BufLen:%d ret:%x!\n", VoLayer, BufLen, s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &BufLen, sizeof(BufLen));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_GetDisplayBufLen call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_GetDisplayBufLen call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_EnableChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_EnableChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_EnableChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_EnableChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_EnableChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}


static CVI_S32 MSG_VO_DisableChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_DisableChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_DisableChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_DisableChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_DisableChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SetChnRotation(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);
	ROTATION_E enRotation = pstMsg->as32PrivData[0];

	s32Ret = CVI_VO_SetChnRotation(VoLayer, VoChn, enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetChnRotation VoLayer:%d VoChn:%d Failed : %#x!\n", VoLayer, VoChn, s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetChnRotation call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetChnRotation call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetChnRotation(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);
	ROTATION_E enRotation;

	s32Ret = CVI_VO_GetChnRotation(VoLayer, VoChn, &enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetChnRotation Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &enRotation, sizeof(enRotation));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_GetChnRotation call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_GetChnRotation call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SendFrame(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_S32 s32MilliSec = pstMsg->as32PrivData[0];

	s32Ret = CVI_VO_SendFrame(VoLayer, VoChn, (VIDEO_FRAME_INFO_S *)pstMsg->pBody, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SendFrame Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SendFrame call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SendFrame call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_ClearChnBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);
	CVI_BOOL bClrAll = pstMsg->as32PrivData[0];

	s32Ret = CVI_VO_ClearChnBuf(VoLayer, VoChn, bClrAll);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_ClearChnBuf Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_ClearChnBuf call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_ClearChnBuf call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_ShowChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_ShowChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_ShowChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_ShowChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_ShowChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_HideChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_HideChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_HideChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_HideChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_HideChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_SetGammaInfo(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_VO_SetGammaInfo((VO_GAMMA_INFO_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_SetGammaInfo Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_SetGammaInfo call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_SetGammaInfo call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_GetGammaInfo(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_GAMMA_INFO_S gamma;

	s32Ret = CVI_VO_GetGammaInfo(&gamma);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_GetGammaInfo Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &gamma, sizeof(gamma));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_GetGammaInfo call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_GetGammaInfo call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_PauseChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_PauseChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_PauseChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_PauseChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_PauseChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_ResumeChn(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_LAYER VoLayer = GET_DEV_ID(pstMsg->u32Module);
	VO_CHN VoChn = GET_CHN_ID(pstMsg->u32Module);

	s32Ret = CVI_VO_ResumeChn(VoLayer, VoChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_ResumeChn Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_ResumeChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_ResumeChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_VoDevIsEnable(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_BOOL bIsEnable;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VO_DEV VoDev = GET_DEV_ID(pstMsg->u32Module);

	bIsEnable = CVI_VO_IsEnabled(VoDev);

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &bIsEnable, sizeof(bIsEnable));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_VoDevIsEnable call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_VoDevIsEnable call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_VoGetCtx(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	extern struct cvi_vo_ctx *voCtx;

	if (!voCtx) {
		s32Ret = CVI_FAILURE;
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, voCtx, sizeof(*voCtx));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("MSG_VO_VoGetCtx call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_VoGetCtx call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_Suspend(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_VO_Suspend();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_VO_Suspend Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_VO_Resume(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_VO_Resume();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("MSG_VO_Resume Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stVoCmdTable[] = {
	{ MSG_CMD_VO_SET_PUBATTR,          MSG_VO_SetPubAttr },
	{ MSG_CMD_VO_GET_PUBATTR,          MSG_VO_GetPubAttr },
	{ MSG_CMD_VO_ENABLE,               MSG_VO_Enable },
	{ MSG_CMD_VO_DISABLE,              MSG_VO_Disable },
	{ MSG_CMD_VO_ENABLE_VIDEOLAYER,    MSG_VO_EnableVideoLayer },
	{ MSG_CMD_VO_DISABLE_VIDEOLAYER,   MSG_VO_DisableVideoLayer },
	{ MSG_CMD_VO_SET_VIDEOLAYERATTR,   MSG_VO_SetVideoLayerAttr },
	{ MSG_CMD_VO_GET_VIDEOLAYERATTR,   MSG_VO_GetVideoLayerAttr },
	{ MSG_CMD_VO_SET_CHNATTR,          MSG_VO_SetChnAttr },
	{ MSG_CMD_VO_GET_CHNATTR,          MSG_VO_GetChnAttr },
	{ MSG_CMD_VO_SET_DISPLAYBUFLEN,    MSG_VO_SetDisplayBufLen },
	{ MSG_CMD_VO_GET_DISPLAYBUFLEN,    MSG_VO_GetDisplayBufLen },
	{ MSG_CMD_VO_ENABLE_CHN,           MSG_VO_EnableChn },
	{ MSG_CMD_VO_DISABLE_CHN,          MSG_VO_DisableChn },
	{ MSG_CMD_VO_SET_CHNROTATION,      MSG_VO_SetChnRotation },
	{ MSG_CMD_VO_GET_CHNROTATION,      MSG_VO_GetChnRotation },
	{ MSG_CMD_VO_SEND_FRAME,           MSG_VO_SendFrame },
	{ MSG_CMD_VO_CLEAR_CHNBUF,         MSG_VO_ClearChnBuf },
	{ MSG_CMD_VO_SHOW_CHN,             MSG_VO_ShowChn },
	{ MSG_CMD_VO_HIDE_CHN,             MSG_VO_HideChn },
	{ MSG_CMD_VO_GAMMA_LUT_UPDATE,     MSG_VO_SetGammaInfo },
	{ MSG_CMD_VO_GAMMA_LUT_READ,       MSG_VO_GetGammaInfo },
	{ MSG_CMD_VO_PAUSE_CHN,            MSG_VO_PauseChn },
	{ MSG_CMD_VO_RESUME_CHN,           MSG_VO_ResumeChn },
	{ MSG_CMD_VO_DEV_IS_ENABLE,        MSG_VO_VoDevIsEnable },
	{ MSG_CMD_VO_GET_CTX,              MSG_VO_VoGetCtx },
	{ MSG_CMD_VO_SUSPEND,              MSG_VO_Suspend },
	{ MSG_CMD_VO_RESUME,               MSG_VO_Resume },
};

MSG_SERVER_MODULE_S g_stModuleVo = {
	CVI_ID_VO,
	"vo",
	sizeof(g_stVoCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stVoCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetVoMod(CVI_VOID)
{
	return &g_stModuleVo;
}
