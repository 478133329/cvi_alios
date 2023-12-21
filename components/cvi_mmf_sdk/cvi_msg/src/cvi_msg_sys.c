#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_sys.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

extern CVI_BOOL bFbOnSc;

static CVI_S32 MSG_SYS_Init(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	UNUSED(siId);
	UNUSED(pstMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Exit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	UNUSED(siId);
	UNUSED(pstMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Bind(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_CHN_S *pstChn = pstMsg->pBody;

	s32Ret = CVI_SYS_Bind(&pstChn[0], &pstChn[1]);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_Bind Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_UnBind(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_CHN_S *pstChn = pstMsg->pBody;

	s32Ret = CVI_SYS_UnBind(&pstChn[0], &pstChn[1]);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_UnBind Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetBindbyDest(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_CHN_S stSrcChn;

	s32Ret = CVI_SYS_GetBindbyDest((MMF_CHN_S *)pstMsg->pBody, &stSrcChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetBindbyDest Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stSrcChn, sizeof(stSrcChn));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetBindbySrc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_BIND_DEST_S stBindDest;

	s32Ret = CVI_SYS_GetBindbySrc((MMF_CHN_S *)pstMsg->pBody, &stBindDest);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetBindbySrc Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, stBindDest.astMmfChn,
				sizeof(MMF_CHN_S) * stBindDest.u32Num);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}
	respMsg->as32PrivData[0] = stBindDest.u32Num;

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetVersion(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_VERSION_S stVersion;

	s32Ret = CVI_SYS_GetVersion(&stVersion);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetVersion Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stVersion, sizeof(stVersion));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetChipId(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 u32ChipId;

	s32Ret = CVI_SYS_GetChipId(&u32ChipId);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetChipId Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &u32ChipId, sizeof(u32ChipId));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetPowerOnReason(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 u32PowerOnReason;

	s32Ret = CVI_SYS_GetPowerOnReason(&u32PowerOnReason);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetPowerOnReason Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &u32PowerOnReason,
				sizeof(u32PowerOnReason));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetChipVersion(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	CVI_U32 u32ChipVersion;

	s32Ret = CVI_SYS_GetChipVersion(&u32ChipVersion);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetChipVersion Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &u32ChipVersion,
				sizeof(u32ChipVersion));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_SetVIVPSSMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_SetVIVPSSMode((VI_VPSS_MODE_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_SetVIVPSSMode Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetVIVPSSMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_VPSS_MODE_S stVIVPSSMode;

	s32Ret = CVI_SYS_GetVIVPSSMode(&stVIVPSSMode);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetVIVPSSMode Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stVIVPSSMode, sizeof(stVIVPSSMode));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_SetVPSSMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_SetVPSSMode(pstMsg->as32PrivData[0]);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_SetVPSSMode Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetVPSSMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VPSS_MODE_E enVpssMode;

	enVpssMode = CVI_SYS_GetVPSSMode();

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, 0, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}
	respMsg->as32PrivData[0] = enVpssMode;

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_SetVPSSModeEx(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_SetVPSSModeEx((VPSS_MODE_S *)pstMsg->pBody);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_SetVPSSModeEx Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_GetVPSSModeEx(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VPSS_MODE_S stVPSSMode;

	s32Ret = CVI_SYS_GetVPSSModeEx(&stVPSSMode);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_GetVPSSModeEx Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stVPSSMode, sizeof(stVPSSMode));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Vi_open(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_VI_Open();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_VI_Open Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Vi_Close(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_VI_Close();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_VI_Close Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CreateGrp call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Alios_Init(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_Init();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_Alios_Init Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("Alios_Init call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("Alios_Init call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_Alios_Exit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_SYS_Exit();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_SYS_Alios_Exit Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("Alios_Exit call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("Alios_Exit call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_SYS_SetFbOnSc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	bFbOnSc = true;
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("Alios_Exit call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("Alios_Exit call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stSysCmdTable[] = {
	{ MSG_CMD_SYS_INIT,               MSG_SYS_Init },
	{ MSG_CMD_SYS_EXIT,               MSG_SYS_Exit },
	{ MSG_CMD_SYS_BIND,               MSG_SYS_Bind },
	{ MSG_CMD_SYS_UNBIND,             MSG_SYS_UnBind },
	{ MSG_CMD_SYS_GET_BIND_BY_DEST,   MSG_SYS_GetBindbyDest },
	{ MSG_CMD_SYS_GET_BIND_BY_SRC,    MSG_SYS_GetBindbySrc },
	{ MSG_CMD_SYS_GET_VERSION,        MSG_SYS_GetVersion },
	{ MSG_CMD_SYS_GET_CHIP_ID,        MSG_SYS_GetChipId },
	{ MSG_CMD_SYS_GET_POWER_REASON,   MSG_SYS_GetPowerOnReason },
	{ MSG_CMD_SYS_GET_CHIP_VERSION,   MSG_SYS_GetChipVersion },
	{ MSG_CMD_SYS_SET_VI_VPSS_MODE,   MSG_SYS_SetVIVPSSMode },
	{ MSG_CMD_SYS_GET_VI_VPSS_MODE,   MSG_SYS_GetVIVPSSMode },
	{ MSG_CMD_SYS_SET_VPSS_MODE,      MSG_SYS_SetVPSSMode },
	{ MSG_CMD_SYS_GET_VPSS_MODE,      MSG_SYS_GetVPSSMode },
	{ MSG_CMD_SYS_SET_VPSS_MODE_EX,   MSG_SYS_SetVPSSModeEx },
	{ MSG_CMD_SYS_GET_VPSS_MODE_EX,   MSG_SYS_GetVPSSModeEx },
	{ MSG_CMD_SYS_VI_OPEN,            MSG_SYS_Vi_open },
	{ MSG_CMD_SYS_VI_CLOSE,           MSG_SYS_Vi_Close },
	{ MSG_CMD_SYS_ALIOS_INIT,         MSG_SYS_Alios_Init },
	{ MSG_CMD_SYS_ALIOS_EXIT,         MSG_SYS_Alios_Exit },
	{ MSG_CMD_SYS_FB_ON_SC,           MSG_SYS_SetFbOnSc }
};

MSG_SERVER_MODULE_S g_stModuleSys = {
	CVI_ID_SYS,
	"sys",
	sizeof(g_stSysCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stSysCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetSysMod(CVI_VOID)
{
	return &g_stModuleSys;
}

