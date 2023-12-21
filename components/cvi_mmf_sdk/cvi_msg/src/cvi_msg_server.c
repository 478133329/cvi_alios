#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <inttypes.h>

#include "cvi_type.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "cvi_msg_server.h"
#include "msg_ctx.h"
#include "sys.h"


#define MSG_HANDLE_MAX_TIME_US (10000)

CVI_S32 g_s32MediaMsgId;
static CVI_BOOL g_bInit;
static MSG_SERVER_CONTEXT g_msgServerContext;
static CVI_BOOL g_bMsgSvrStartFlg = CVI_FALSE;
static pthread_t g_msgSvrReceiveThread;

struct worker_private_data {
	CMDPROC_FN msg_cb;
	CVI_S32 siId;
	CVI_IPCMSG_MESSAGE_S stMsg;
};

static void worker_cb(void *data)
{
	struct worker_private_data *pri_data = (struct worker_private_data *)data;

	if (pri_data->stMsg.u32BodyLen)
		pri_data->stMsg.pBody = data + sizeof(struct worker_private_data);
	pri_data->msg_cb(pri_data->siId, &pri_data->stMsg);
}

static CMDPROC_FN MSG_SERVER_GetFunc(CVI_U32 u32ModID, CVI_U32 u32CmdID)
{
	CVI_U32 i;
	MSG_SERVER_MODULE_S *pTmpServerModule =
		g_msgServerContext.pstServermodules[u32ModID];

	if (pTmpServerModule == CVI_NULL) {
		return CVI_NULL;
	}

	for (i = 0; i < pTmpServerModule->u32ModuleCmdAmount; i++) {
		if (u32CmdID == (pTmpServerModule->pstModuleCmdTable + i)->u32Cmd) {
			return (pTmpServerModule->pstModuleCmdTable + i)->CMDPROC_FN_PTR;
		}
	}
	return CVI_NULL;
}

static CVI_S32 MSG_ServerProcNoRegMsg(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S* pstMsg)
{
	CVI_S32 s32Ret;

	CVI_IPCMSG_MESSAGE_S* respMsg = CVI_NULL;

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, -1, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("Server proc call CVI_IPCMSG_CreateRespMessage fail\n");
		return -1;
	}

	s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);

	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("Server proc msg SendAsync fail,ret:[%#x]\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static void MediaMsgReceiveProc(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_U32 u32ModID;
	CVI_S32 s32Ret;
	CMDPROC_FN pProc = CVI_NULL;
	struct timespec ts1, ts2;
	CVI_U64 time_us1, time_us2;

	clock_gettime(CLOCK_MONOTONIC, &ts1);
	time_us1 = ts1.tv_sec*1000000 + ts1.tv_nsec/1000;

	u32ModID = GET_MOD_ID(pstMsg->u32Module);
	switch (u32ModID) {
		case CVI_ID_SYS:
		case CVI_ID_VB:
		case CVI_ID_VI:
		case CVI_ID_VPSS:
		case CVI_ID_VENC:
		case CVI_ID_VDEC:
		case CVI_ID_GDC:
		case CVI_ID_ISP:
		case CVI_ID_RGN:
		case CVI_ID_AUD:
		case CVI_ID_VO:
		case CVI_ID_MIPI_TX:
		case CVI_ID_SENSOR:
			pProc = MSG_SERVER_GetFunc(u32ModID, pstMsg->u32CMD);
			if (pProc != CVI_NULL) {
				if (IS_BLOCK(pstMsg->u32Module)) {
					struct worker_private_data pri_data;
					CVI_VOID *buf = (CVI_VOID *)&pri_data;
					CVI_U32 u32HeadLen = sizeof(pri_data);
					CVI_U32 u32TotalLen = u32HeadLen;

					pri_data.msg_cb = pProc;
					pri_data.siId = siId;
					pri_data.stMsg = *pstMsg;
					pri_data.stMsg.pBody = NULL;
					if (pstMsg->u32BodyLen) {
						u32TotalLen += pstMsg->u32BodyLen;
						buf = malloc(u32TotalLen);
						if (!buf) {
							CVI_TRACE_MSG("malloc failed, len:%d\n", u32TotalLen);
							return;
						}
						memcpy(buf, &pri_data, u32HeadLen);
						memcpy(buf + u32HeadLen, pstMsg->pBody, pstMsg->u32BodyLen);
					}

					s32Ret = wq_pool_task_add(worker_cb, buf, u32TotalLen);
					if (pstMsg->u32BodyLen)
						free(buf);
				} else {
					s32Ret = pProc(siId, pstMsg);
				}
				if (s32Ret != CVI_SUCCESS) {
					CVI_TRACE_MSG("ModID:%u CMD:%u PROC return error:0x%x\n", u32ModID,
								pstMsg->u32CMD, s32Ret);
				}
			} else {
				CVI_TRACE_MSG("ModID:%u CMD:%u Func return null or module isn't registered.\n", u32ModID,
							   pstMsg->u32CMD);
				CVI_S32 s32Ret = MSG_ServerProcNoRegMsg(siId, pstMsg);
				if (s32Ret != CVI_SUCCESS) {
					CVI_TRACE_MSG("Server Proc No Reg Msg fail, err: 0x%x\n", s32Ret);
				}
			}
			break;
		default:
			printf("receive u32ModID:%d msg %d error.\n", u32ModID, pstMsg->u32CMD);
			break;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts2);
	time_us2 = ts2.tv_sec*1000000 + ts2.tv_nsec/1000;
	if ((time_us2 - time_us1) > MSG_HANDLE_MAX_TIME_US)
		CVI_TRACE_MSG("[warning] ID(%"PRIu64") mod(%s) cmd(%d) Message processing takes too long.\n",
			pstMsg->u64Id, sys_get_modname(GET_MOD_ID(pstMsg->u32Module)), pstMsg->u32CMD);
}

static void *MediaMsgReceiveThread(void *arg)
{
	CVI_S32 s32Ret;

	arg = arg;
	prctl(PR_SET_NAME, "MediaMsg", 0, 0, 0);

	/* first connect */
	s32Ret = CVI_IPCMSG_Connect(&g_s32MediaMsgId, "CVI_MMF_MSG", MediaMsgReceiveProc);
	if (s32Ret != CVI_SUCCESS) {
		printf("Connect fail\n");
		return NULL;
	}

	CVI_TRACE_MSG("IPCMSG Run...\n");
	CVI_IPCMSG_Run(g_s32MediaMsgId);

	/* reconnect detect and process */
	while (g_bMsgSvrStartFlg) {
		if (CVI_IPCMSG_IsConnected(g_s32MediaMsgId) != CVI_TRUE) {
			CVI_IPCMSG_Disconnect(g_s32MediaMsgId);

			s32Ret = CVI_IPCMSG_Connect(&g_s32MediaMsgId, "CVI_MMF_MSG", MediaMsgReceiveProc);
			if (s32Ret != CVI_SUCCESS) {
				printf("Connect fail\n");
				return NULL;
			}
			CVI_IPCMSG_Run(g_s32MediaMsgId);
		}
	}

	return NULL;
}

CVI_S32 CVI_MEDIA_MSG_RegMod(MOD_ID_E enModId, MSG_SERVER_MODULE_S *pstServermodules)
{
	if (pstServermodules == NULL) {
		CVI_TRACE_MSG("RegMedia msg fail, for Module ptr is null.\n");
		return CVI_FAILURE;
	}

	if (enModId >= CVI_ID_BUTT) {
		CVI_TRACE_MSG("RegMedia msg fail, for moduleId[%d] err.\n", enModId);
		return CVI_FAILURE;
	}

	g_msgServerContext.pstServermodules[enModId] = pstServermodules;

	return CVI_SUCCESS;
}

CVI_S32 CVI_MSG_Init(CVI_VOID)
{
	CVI_S32 s32Ret;
	pthread_attr_t attr;
	struct sched_param tsk;
	CVI_IPCMSG_CONNECT_S stConnectAttr = { 0, CVI_IPCMSG_MEDIA_PORT, 1 };

	if (g_bInit) {
		CVI_TRACE_MSG("Already initialized.\n");
		return CVI_SUCCESS;
	}

	s32Ret = CVI_IPCMSG_AddService("CVI_MMF_MSG", &stConnectAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_IPCMSG_AddService fail\n");
		return CVI_FAILURE;
	}

	g_bMsgSvrStartFlg = CVI_TRUE;
	tsk.sched_priority = 46;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_setschedparam(&attr, &tsk);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, 101*1024);
	s32Ret = pthread_create(&g_msgSvrReceiveThread, &attr, MediaMsgReceiveThread, CVI_NULL);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("pthread_create MediaMsgReceiveThread fail\n");
		return CVI_FAILURE;
	}
	pthread_attr_destroy(&attr);
	CVI_TRACE_MSG("Media_MSG_Init successfull\n");

	wq_pool_init(THREAD_POOL_NUM);
	g_msgServerContext.pstServermodules[CVI_ID_SYS] = MSG_GetSysMod();
	g_msgServerContext.pstServermodules[CVI_ID_VB] = MSG_GetVbMod();
	g_msgServerContext.pstServermodules[CVI_ID_VI] = MSG_GetViMod();
	g_msgServerContext.pstServermodules[CVI_ID_VPSS] = MSG_GetVpssMod();
	g_msgServerContext.pstServermodules[CVI_ID_VENC] = MSG_GetVencMod();
	g_msgServerContext.pstServermodules[CVI_ID_VDEC] = MSG_GetVdecMod();
	g_msgServerContext.pstServermodules[CVI_ID_GDC] = MSG_GetGdcMod();
	g_msgServerContext.pstServermodules[CVI_ID_ISP] = MSG_GetIspMod();
	g_msgServerContext.pstServermodules[CVI_ID_RGN] = MSG_GetRgnMod();
	g_msgServerContext.pstServermodules[CVI_ID_AUD] = MSG_GetAudioMod();
	g_msgServerContext.pstServermodules[CVI_ID_VO] = MSG_GetVoMod();
	g_msgServerContext.pstServermodules[CVI_ID_MIPI_TX] = MSG_GetMipitxMod();
	g_msgServerContext.pstServermodules[CVI_ID_SENSOR] = MSG_GetSensorMod();
	g_bInit = CVI_TRUE;
	return s32Ret;
}

CVI_S32 CVI_MSG_Deinit(CVI_VOID)
{
	CVI_S32 s32Ret;

	if (!g_bInit) {
		CVI_TRACE_MSG("Already deinitialized.\n");
		return CVI_SUCCESS;
	}

	g_bMsgSvrStartFlg = CVI_FALSE;
	s32Ret = CVI_IPCMSG_Disconnect(g_s32MediaMsgId);
	if (s32Ret != CVI_SUCCESS) {
		printf("Disconnect fail , ret:%d\n", s32Ret);
	}

	pthread_join(g_msgSvrReceiveThread, CVI_NULL);

	CVI_IPCMSG_DelService("CVI_MMF_MSG");

	wq_pool_deinit();
	g_bInit = CVI_FALSE;

	return s32Ret;
}

CVI_S32 CVI_MSG_GetSiId(CVI_VOID)
{
	return g_s32MediaMsgId;
}

CVI_S32 CVI_MSG_SendSync_CB(CVI_U32 u32Module, CVI_U32 u32CMD, CVI_VOID *pBody, CVI_U32 u32BodyLen,
					 MSG_PRIV_DATA_S *pstPrivData)
{
	CVI_S32 s32Ret;
	CVI_IPCMSG_MESSAGE_S *pReq = NULL;
	CVI_IPCMSG_MESSAGE_S *pResp = NULL;

	if (g_s32MediaMsgId < 0) {
		CVI_TRACE_MSG("The MSG is not initialized.\n");
		return CVI_FAILURE;
	}

	pReq = CVI_IPCMSG_CreateMessage(u32Module, u32CMD, pBody, u32BodyLen);
	if (pReq == NULL) {
		CVI_TRACE_MSG("CVI_IPCMSG_CreateMessage return NULL.\n");
		return -1;
	}
	if (pstPrivData != NULL) {
		memcpy(pReq->as32PrivData, pstPrivData->as32PrivData, sizeof(CVI_S32) * CVI_IPCMSG_PRIVDATA_NUM);
	}
	s32Ret = CVI_IPCMSG_SendSync(g_s32MediaMsgId, pReq, &pResp, CVI_IPCMSG_SEND_SYNC_TIMEOUT);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("CVI_IPCMSG_SendSyncCB fail s32Ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(pReq);
		CVI_IPCMSG_DestroyMessage(pResp);
		return s32Ret;
	}
	s32Ret = pResp->s32RetVal;
	if (s32Ret == CVI_SUCCESS && (pResp->u32BodyLen > 0)) {
		memcpy(pBody, pResp->pBody, pResp->u32BodyLen);

		if (pstPrivData != NULL) {
			memcpy(pstPrivData->as32PrivData, pResp->as32PrivData,
				sizeof(CVI_S32) * CVI_IPCMSG_PRIVDATA_NUM);
		}
	}
	CVI_IPCMSG_DestroyMessage(pReq);
	CVI_IPCMSG_DestroyMessage(pResp);

	return s32Ret;
}
