#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "cvi_type.h"
#include "cvi_gdc.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"
#include "ldc_uapi.h"
#include "cvi_comm_gdc.h"

static CVI_S32 MSG_GDC_BegainJob(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_GDC_BeginJob(&hHandle);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_BeginJob Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &hHandle, sizeof(hHandle));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("BegainJob call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("BegainJob call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_EndJob(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	hHandle = *(GDC_HANDLE *)(pstMsg->pBody);
	s32Ret = CVI_GDC_EndJob(hHandle);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_EndJob Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("EndJob call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("EndJob call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_CancelJob(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	hHandle = *(GDC_HANDLE *)(pstMsg->pBody);
	s32Ret = CVI_GDC_CancelJob(hHandle);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_CancelJob Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CancelJob call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CancelJob call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_AddRotationTask(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct gdc_task_attr attr;
	GDC_TASK_ATTR_S stTask;
	CVI_U32 enRotation;

	attr = *(struct gdc_task_attr *)(pstMsg->pBody);
	hHandle = attr.handle;

	memcpy(&stTask.stImgIn, &attr.stImgIn, sizeof(attr.stImgIn));
	memcpy(&stTask.stImgOut, &attr.stImgOut, sizeof(attr.stImgOut));
	memcpy(stTask.au64privateData, attr.au64privateData, sizeof(attr.au64privateData));
	stTask.reserved = attr.reserved;
	enRotation = attr.enRotation;

	s32Ret = CVI_GDC_AddRotationTask(hHandle, &stTask, enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_AddRotationTask Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("AddRotationTask call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("AddRotationTask call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_AddLDCTask(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct gdc_task_attr attr;
	GDC_TASK_ATTR_S stTask;
	CVI_U32 enRotation;
	LDC_ATTR_S stLDCAttr;

	attr = *(struct gdc_task_attr *)(pstMsg->pBody);
	hHandle = attr.handle;

	memcpy(&stTask.stImgIn, &attr.stImgIn, sizeof(attr.stImgIn));
	memcpy(&stTask.stImgOut, &attr.stImgOut, sizeof(attr.stImgOut));
	memcpy(stTask.au64privateData, attr.au64privateData, sizeof(attr.au64privateData));
	stTask.reserved = attr.reserved;
	enRotation = attr.enRotation;
	memcpy(&stLDCAttr, &attr.stLDCAttr, sizeof(attr.stLDCAttr));

	s32Ret = CVI_GDC_AddLDCTask(hHandle, &stTask, &stLDCAttr, enRotation);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_AddLDCTask Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("AddLDCTask call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("AddLDCTask call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_SetBufWrapAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct gdc_task_attr attr;
	GDC_TASK_ATTR_S stTask;
	struct _LDC_BUF_WRAP_S stBufWrap;

	attr = *(struct gdc_task_attr *)(pstMsg->pBody);
	hHandle = attr.handle;

	memcpy(&stTask.stImgIn, &attr.stImgIn, sizeof(attr.stImgIn));
	memcpy(&stTask.stImgOut, &attr.stImgOut, sizeof(attr.stImgOut));
	memcpy(&stBufWrap, &attr.stBufWrap, sizeof(attr.stBufWrap));

	s32Ret = CVI_GDC_SetBufWrapAttr(hHandle, &stTask, &stBufWrap);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_SetBufWrapAttr Failed : %#x!\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("SetBufWrapAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetBufWrapAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_GetBufWrapAttr(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	GDC_HANDLE hHandle;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct gdc_task_attr attr;
	GDC_TASK_ATTR_S stTask;
	struct _LDC_BUF_WRAP_S stBufWrap;
	struct ldc_buf_wrap_cfg cfg;

	attr = *(struct gdc_task_attr *)(pstMsg->pBody);
	hHandle = attr.handle;

	memcpy(&stTask.stImgIn, &attr.stImgIn, sizeof(attr.stImgIn));
	memcpy(&stTask.stImgOut, &attr.stImgOut, sizeof(attr.stImgOut));

	s32Ret = CVI_GDC_GetBufWrapAttr(hHandle, &stTask, &stBufWrap);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_SetBufWrapAttr Failed : %#x!\n", s32Ret);
	}

	memcpy(&cfg.stTask.stImgIn, &stTask.stImgIn, sizeof(attr.stImgIn));
	memcpy(&cfg.stTask.stImgOut, &stTask.stImgOut, sizeof(attr.stImgOut));
	memcpy(&cfg.stBufWrap, &stBufWrap, sizeof(cfg.stBufWrap));
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &cfg, sizeof(cfg));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("SetBufWrapAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("SetBufWrapAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_SUSPEND(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_GDC_Suspend();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_Suspend Failed : %#x!\n", s32Ret);
	}
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CVI_GDC_Suspend CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_Suspend CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_GDC_RESUME(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

	s32Ret = CVI_GDC_Resume();
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_Resume Failed : %#x!\n", s32Ret);
	}
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("CVI_GDC_Resume CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_GDC_Resume CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stGdcCmdTable[] = {
	{ MSG_CMD_GDC_BEGAIN_JOB,   MSG_GDC_BegainJob },
	{ MSG_CMD_GDC_END_JOB,      MSG_GDC_EndJob },
	{ MSG_CMD_GDC_CANCEL_JOB,   MSG_GDC_CancelJob },
	{ MSG_CMD_GDC_ADD_ROT_TASK, MSG_GDC_AddRotationTask },
	{ MSG_CMD_GDC_ADD_LDC_TASK, MSG_GDC_AddLDCTask },
	{ MSG_CMD_GDC_SET_BUF_WRAP, MSG_GDC_SetBufWrapAttr },
	{ MSG_CMD_GDC_GET_BUF_WRAP, MSG_GDC_GetBufWrapAttr },
	{ MSG_CMD_GDC_SUSPEND, MSG_GDC_SUSPEND },
	{ MSG_CMD_GDC_RESUME, MSG_GDC_RESUME },
};

MSG_SERVER_MODULE_S g_stModuleGdc = {
	CVI_ID_GDC,
	"gdc",
	sizeof(g_stGdcCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stGdcCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetGdcMod(CVI_VOID)
{
	return &g_stModuleGdc;
}

