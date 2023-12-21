/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2022. All rights reserved.
 *
 * File Name: cvi_ispd2_event_server.c
 * Description:
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include "uv.h"

#include "cvi_type.h"
#include "cvi_ispd2_define.h"
#include "cvi_ispd2_log.h"

#include "cvi_ispd2_event_server.h"
#include "cvi_ispd2_message.h"
#include "cvi_ispd2_callback_funcs_apps.h"

#define MAX_UV_ALLOCATE_BUFFER_SIZE			(4096)

// -----------------------------------------------------------------------------
#define TIME_INTERVAL_UV_RUN				(40)		// 40 ms
#define	TIME_INTERVAL_PACKET_RECV			(2)			// 2 sec

#define MAX_CLIENTS							(10)
#define JRPC_SERVER_DEFAULT_BUF_SIZE		(16384)		// 16Kb
#define JRPC_SERVER_STEP_ADD_BUF_SIZE		(8192)		// 8Kb

#define DEFAULT_ADDRESS						"127.0.0.1"
#define DEFAULT_BACKLOG						(2)

// -----------------------------------------------------------------------------
typedef enum {
	EEMPTY = 0,
	EPARTIAL_JSONRPC = 1,
	EPARTIAL_BINARY = 2
} EPREV_DATA_TYPE;

typedef struct {
	TISPDaemon2Info		*ptDaemonInfo;
	int					iFd;
	char				*pszRecvBuffer;
	CVI_U32				u32RecvBufferOffset;
	CVI_U32				u32RecvBufferSize;
	EPREV_DATA_TYPE		eRecvBufferContentType;
	struct timeval		tvLastRecvTime;
} TISPDaemon2ConnectInfo;

// -----------------------------------------------------------------------------
#define MESSAGE_FORMAT_JSONRPC_ARRAY_READY		\
	(EMESSAGE_FORMAT_JSONRPC_ARRAY_HEAD | EMESSAGE_FORMAT_JSONRPC_ARRAY_TAIL)
#define MESSAGE_FORMAT_JSONRPC_ELEMENT_READY	\
	(EMESSAGE_FORMAT_JSONRPC_ELEMENT_HEAD | EMESSAGE_FORMAT_JSONRPC_ELEMENT_TAIL)

#define IS_COMPLETE_JSONRPC(eType) \
	(((eType & MESSAGE_FORMAT_JSONRPC_ARRAY_READY) == MESSAGE_FORMAT_JSONRPC_ARRAY_READY) \
	|| ((eType & MESSAGE_FORMAT_JSONRPC_ELEMENT_READY) == MESSAGE_FORMAT_JSONRPC_ELEMENT_READY))

#define IS_HEADER_ONLY_JSONRPC(eType) \
	((eType & EMESSAGE_FORMAT_JSONRPC_ARRAY_HEAD) || (eType & EMESSAGE_FORMAT_JSONRPC_ELEMENT_HEAD))

#define IS_PARTIAL_JSONRPC(eType)		(eType & EMESSAGE_FORMAT_JSONRPC_PARTICAL)
#define IS_BINARY_DATA(eType)			(eType & EMESSAGE_FORMAT_BINARY)

#define CONNECTINFO_RESET_RECV_BUFFER_STATUS(connectInfo) {	\
	connectInfo->u32RecvBufferOffset = 0; \
	connectInfo->eRecvBufferContentType = EEMPTY; \
}
#define CONNECTINFO_RESET_BINARY_MODE_STATUS(connectInfo) {	\
	if (connectInfo->ptDaemonInfo->tDeviceInfo.bKeepBinaryInDataInfoOnce) { \
		connectInfo->ptDaemonInfo->tDeviceInfo.bKeepBinaryInDataInfoOnce = CVI_FALSE; \
	} else { \
		CVI_ISPD2_ResetBinaryInStructure(&(connectInfo->ptDaemonInfo->tDeviceInfo.tBinaryInData)); \
	} \
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_CB_AllocBuffer(uv_handle_t *pHandle, size_t lSuggestedLength, uv_buf_t *pBuf)
{
	lSuggestedLength = MIN(MAX_UV_ALLOCATE_BUFFER_SIZE, lSuggestedLength);
	pBuf->base = (char *)malloc(lSuggestedLength);
	pBuf->len = (pBuf->base != NULL) ? lSuggestedLength : 0;

	// ISP_DAEMON2_DEBUG_EX(LOG_DEBUG, "UV allocate buffer suggested length : %d", lSuggestedLength);

	UNUSED(pHandle);
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_CB_Close(uv_handle_t *pHandle)
{
	SAFE_FREE(pHandle);
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_HandleMessageBuffer(char *pszBuf, CVI_U32 u32BufOffset, CVI_U32 u32RecvLen,
	TISPDaemon2ConnectInfo *ptConnectObj)
{
	TISPDaemon2Info		*ptObject = ptConnectObj->ptDaemonInfo;
	TBinaryData			*ptBinaryInData = &(ptObject->tDeviceInfo.tBinaryInData);
	CVI_U32				u32ContentTotalSize = u32BufOffset + (CVI_U32)u32RecvLen;
	CVI_U32				u32ContentTailPos = u32ContentTotalSize - 1;
	int					iFd = ptConnectObj->iFd;

	CVI_BOOL			bBinaryMode;

	bBinaryMode = (ptBinaryInData->eDataType != EBINARYDATA_NONE);
	bBinaryMode = bBinaryMode && (
		(ptBinaryInData->eDataState == EBINARYSTATE_INITIAL)
		|| (ptBinaryInData->eDataState == EBINARYSTATE_RECV));
	bBinaryMode = bBinaryMode && (ptBinaryInData->u32Size > 0);

	if (CVI_ISPD2_Message_ResetRecvCommandChecker(pszBuf + u32BufOffset)) {
		ISP_DAEMON2_DEBUG(LOG_DEBUG, "Recv. reset command Content is BINARY content");

		CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
		CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
		return;
	}

	if (!bBinaryMode) {
		// JSONRpc text mode
		ISP_DAEMON2_DEBUG(LOG_DEBUG, "Recv JSONRpc content mode");

		CVI_U32 eFullDataType;

		eFullDataType = CVI_ISPD2_Message_CommandChecker(pszBuf, 0, u32ContentTailPos);

		if (u32BufOffset == 0) {
			if (IS_COMPLETE_JSONRPC(eFullDataType)) {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG, "Content is JSONRPC Array/Element (full)");

				CVI_ISPD2_Message_HandleMessage(pszBuf, u32ContentTotalSize, iFd,
					&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			} else if (IS_HEADER_ONLY_JSONRPC(eFullDataType)) {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG,
				//	"Content is JSONRPC Head only, skip JSONRPC handler this round");

				ptConnectObj->u32RecvBufferOffset += u32RecvLen;
				ptConnectObj->eRecvBufferContentType = EPARTIAL_JSONRPC;
			} else {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG,
				//	"Ignore this data (partial/tail (without head info) JSONRPC or BiNARY data");

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			}
		} else {
			CVI_U32 ePartialDataType;

			ePartialDataType = CVI_ISPD2_Message_CommandChecker(pszBuf, u32BufOffset, u32ContentTailPos);

			if (IS_COMPLETE_JSONRPC(ePartialDataType)) {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG, "Packet Content is JSONRPC Array/Element (full)");

				CVI_ISPD2_Message_HandleMessage(pszBuf + u32BufOffset, u32RecvLen, iFd,
					&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			} else if (ptConnectObj->eRecvBufferContentType == EPARTIAL_JSONRPC) {
				if (IS_COMPLETE_JSONRPC(eFullDataType)) {
					// ISP_DAEMON2_DEBUG(LOG_DEBUG, "This round is partial."
					//	"But the total content is JSONRPC Array/Element (full)");

					CVI_ISPD2_Message_HandleMessage(pszBuf, u32ContentTotalSize, iFd,
						&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));

					CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
					CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
				} else if (IS_HEADER_ONLY_JSONRPC(ePartialDataType)) {
					// ISP_DAEMON2_DEBUG(LOG_DEBUG,
					//	"Previous data is invalid content (get new head)\n"
					//	"Ignore previous data and move current data to buffer start");

					memmove(ptConnectObj->pszRecvBuffer,
						ptConnectObj->pszRecvBuffer + u32BufOffset,
						u32RecvLen);

					ptConnectObj->u32RecvBufferOffset = u32RecvLen;
					ptConnectObj->eRecvBufferContentType = EPARTIAL_JSONRPC;
				} else if (IS_PARTIAL_JSONRPC(ePartialDataType)) {
					// ISP_DAEMON2_DEBUG(LOG_DEBUG,
					//	"Partial only, skip JSONRPC handler this round");

					ptConnectObj->u32RecvBufferOffset += u32RecvLen;
					ptConnectObj->eRecvBufferContentType = EPARTIAL_JSONRPC;
				} else {
					if (IS_BINARY_DATA(ePartialDataType)) {
						ISP_DAEMON2_DEBUG(LOG_DEBUG,
							"Data format mis-match (JSONRPC -> BINARY)");
					// } else {
					//	ISP_DAEMON2_DEBUG(LOG_DEBUG,
					//		"Partial data is tail but head is not match");
					}

					CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
					CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
				}
			} else {
				// if (ptConnectObj->eRecvBufferContentType == EPARTIAL_BINARY) {
				//	ISP_DAEMON2_DEBUG(LOG_DEBUG,
				//		"Prev. is binary data but recv. status is not binary mode");
				// } else {
				//	ISP_DAEMON2_DEBUG(LOG_DEBUG,
				//		"Partial data is tail but head is not match");
				// }

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			}
		}
	} else {
		// Binary mode
		ISP_DAEMON2_DEBUG(LOG_DEBUG, "Recv BINARY content mode");

		if (ptBinaryInData->pu8Buffer == CVI_NULL) {
			if (ptBinaryInData->u32Size == u32ContentTotalSize) {
				ptBinaryInData->eDataState = EBINARYSTATE_DONE;
				ptBinaryInData->pu8Buffer = (CVI_U8 *)pszBuf;
				ptBinaryInData->u32RecvSize = u32ContentTotalSize;

				CVI_ISPD2_Message_HandleBinary(pszBuf, u32ContentTotalSize, iFd,
					&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			} else if (ptBinaryInData->u32Size > u32ContentTotalSize) {
				ptBinaryInData->eDataState = EBINARYSTATE_RECV;
				ptBinaryInData->u32RecvSize = u32ContentTotalSize;
				ptConnectObj->u32RecvBufferOffset += u32RecvLen;
				ptConnectObj->eRecvBufferContentType = EPARTIAL_BINARY;
			} else {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG, "Binary data size mismatch, reset");
				// over size
				ptBinaryInData->eDataState = EBINARYSTATE_ERROR;
				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			}
		} else {
			if (ptBinaryInData->u32Size == (ptBinaryInData->u32RecvSize + u32ContentTotalSize)) {
				ptBinaryInData->eDataState = EBINARYSTATE_DONE;
				memcpy(ptBinaryInData->pu8Buffer + ptBinaryInData->u32RecvSize,
					pszBuf, ptBinaryInData->u32Size - ptBinaryInData->u32RecvSize);

				CVI_ISPD2_Message_HandleBinary((const char *) ptBinaryInData->pu8Buffer,
					ptBinaryInData->u32Size, iFd,
					&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));

				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			} else if (ptBinaryInData->u32Size > (ptBinaryInData->u32RecvSize + u32ContentTotalSize)) {
				ptBinaryInData->eDataState = EBINARYSTATE_RECV;
				memcpy(ptBinaryInData->pu8Buffer + ptBinaryInData->u32RecvSize,
					pszBuf, u32ContentTotalSize);
				ptBinaryInData->u32RecvSize += u32ContentTotalSize;
				ptConnectObj->u32RecvBufferOffset = 0;
				ptConnectObj->eRecvBufferContentType = EPARTIAL_BINARY;

			} else {
				// ISP_DAEMON2_DEBUG(LOG_DEBUG, "Binary data size mismatch, reset");
				// over size
				ptBinaryInData->eDataState = EBINARYSTATE_ERROR;
				CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
				CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
			}
		}
	}

	// CVI_ISPD2_Message_HandleMessage(pszBuf, u32RecvLen, iFd,
	//	&(ptObject->tHandlerInfo), &(ptObject->tDeviceInfo));
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_HandleSocketPacket(const char *pcBuffer, CVI_U32 u32BufferLen,
	TISPDaemon2ConnectInfo *ptConnectObj)
{
	TISPDaemon2Info			*ptObject = ptConnectObj->ptDaemonInfo;
	TBinaryData				*ptBinaryInData = &(ptObject->tDeviceInfo.tBinaryInData);

	struct timeval	tvCurrentTime;

	gettimeofday(&tvCurrentTime, NULL);

	if (
		(ptConnectObj->u32RecvBufferOffset > 0)
		&& (tvCurrentTime.tv_sec >= (ptConnectObj->tvLastRecvTime.tv_sec + TIME_INTERVAL_PACKET_RECV))
	) {
		ISP_DAEMON2_DEBUG(LOG_DEBUG, "Reset receive buffer");
		CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
		CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
	}
	ptConnectObj->tvLastRecvTime = tvCurrentTime;

	if ((ptBinaryInData->eDataState == EBINARYSTATE_INITIAL) && (ptBinaryInData->u32Size > 0)) {
		CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
	}

	CVI_U32			u32RecvBufOffset, u32RecvBufSize;

	u32RecvBufOffset	= ptConnectObj->u32RecvBufferOffset;
	u32RecvBufSize		= ptConnectObj->u32RecvBufferSize;

	if ((u32RecvBufSize - u32RecvBufOffset) < u32BufferLen) {
		char	*pszTemp = NULL;
		CVI_U32	u32AddedBufSize, u32ReallocBufSize;

		u32AddedBufSize = MAX(u32BufferLen, JRPC_SERVER_STEP_ADD_BUF_SIZE);
		u32AddedBufSize = (u32AddedBufSize + 3) >> 2 << 2;

		u32ReallocBufSize = u32RecvBufSize + u32AddedBufSize;

		pszTemp = (char *)realloc(ptConnectObj->pszRecvBuffer, u32ReallocBufSize * sizeof(char));
		if (pszTemp == NULL) {
			ISP_DAEMON2_DEBUG(LOG_ERR, "Reallocate receive buffer fail");
		} else {
			ISP_DAEMON2_DEBUG_EX(LOG_DEBUG, "Reallocate receive buffer from (%u) to (%u)",
				u32RecvBufSize, u32ReallocBufSize);
			ptConnectObj->pszRecvBuffer = pszTemp;
			ptConnectObj->u32RecvBufferSize = u32ReallocBufSize;
			u32RecvBufSize = ptConnectObj->u32RecvBufferSize;
		}
	}

	char		*pszBuf = ptConnectObj->pszRecvBuffer;
	CVI_U32		u32AvailableRecvBufSize = u32RecvBufSize - u32RecvBufOffset;

	if (u32AvailableRecvBufSize < u32BufferLen) {
		ISP_DAEMON2_DEBUG(LOG_WARNING, "No avaiable memory for socket command (Clean current buffer content)");
		CONNECTINFO_RESET_RECV_BUFFER_STATUS(ptConnectObj);
		CONNECTINFO_RESET_BINARY_MODE_STATUS(ptConnectObj);
		return;
	}

	memcpy((void *)pszBuf + u32RecvBufOffset, pcBuffer, sizeof(char) * u32BufferLen);

	CVI_ISPD2_ES_HandleMessageBuffer(pszBuf, u32RecvBufOffset, u32BufferLen, ptConnectObj);
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_CB_SocketRead(uv_stream_t *pUVClientHandle, ssize_t lReadLength, const uv_buf_t *pBuf)
{
	uv_stream_t_ex			*pUVClientEx = (uv_stream_t_ex *)(pUVClientHandle);
	uv_stream_t				*pUVClient = (uv_stream_t *)&(pUVClientEx->UVTcp);
	TISPDaemon2ConnectInfo	*ptConnectObj = (TISPDaemon2ConnectInfo *)pUVClientEx->ptHandle;

	if (lReadLength > 0) {
		ISP_DAEMON2_DEBUG_EX(LOG_DEBUG, "===================>>> Recv content: %ld, %ld\n",
			lReadLength, pBuf->len);

		CVI_ISPD2_ES_HandleSocketPacket(pBuf->base, lReadLength, ptConnectObj);
	} else if (lReadLength < 0) {
		if (lReadLength != UV_EOF) {
			ISP_DAEMON2_DEBUG_EX(LOG_ERR, "Read error : %s", uv_err_name(lReadLength));
		}

		ISP_DAEMON2_DEBUG(LOG_DEBUG, "Client close");

		uv_read_stop(pUVClient);

		ptConnectObj->ptDaemonInfo->u8ClientCount--;

		SAFE_FREE(ptConnectObj->pszRecvBuffer);
		SAFE_FREE(ptConnectObj);

		uv_close((uv_handle_t *)pUVClient, CVI_ISPD2_ES_CB_Close);
	}
	free(pBuf->base);
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_CB_NewConnection(uv_stream_t *pUVServerHandle, int iStatus)
{
	uv_stream_t_ex			*pUVServerEx = (uv_stream_t_ex *)(pUVServerHandle);
	uv_stream_t				*pUVServer = (uv_stream_t *)&(pUVServerEx->UVTcp);
	TISPDaemon2Info			*ptObject = (TISPDaemon2Info *)(pUVServerEx->ptHandle);

	uv_stream_t_ex			*pUVClientEx = NULL;
	TISPDaemon2ConnectInfo	*ptConnectObj = NULL;

	if (iStatus < 0) {
		ISP_DAEMON2_DEBUG_EX(LOG_ERR, "New connection error : %s", uv_strerror(iStatus));
		return;
	}

	pUVClientEx = (uv_stream_t_ex *)malloc(sizeof(uv_stream_t_ex));
	if (pUVClientEx == NULL) {
		ISP_DAEMON2_DEBUG(LOG_ERR, "Allocate clientEx fail");

		return;
	}

	uv_tcp_init(ptObject->pUVLoop, &(pUVClientEx->UVTcp));

	if (uv_accept(pUVServer, (uv_stream_t *)&(pUVClientEx->UVTcp)) == 0) {
		if (ptObject->u8ClientCount >= MAX_CLIENTS) {
			ISP_DAEMON2_DEBUG(LOG_ERR, "Connection over limit");

			uv_close((uv_handle_t *)&(pUVClientEx->UVTcp), CVI_ISPD2_ES_CB_Close);
			return;
		}

		ptConnectObj = (TISPDaemon2ConnectInfo *)calloc(1, sizeof(TISPDaemon2ConnectInfo));
		if (ptConnectObj == NULL) {
			ISP_DAEMON2_DEBUG(LOG_ERR, "Allocate client watcher fail");

			uv_close((uv_handle_t *)&(pUVClientEx->UVTcp), CVI_ISPD2_ES_CB_Close);
			return;
		}

		ptConnectObj->pszRecvBuffer = (char *)calloc(JRPC_SERVER_DEFAULT_BUF_SIZE, sizeof(char));
		if (ptConnectObj->pszRecvBuffer == NULL) {
			ISP_DAEMON2_DEBUG(LOG_ERR, "Allocate client recv buffer fail");

			SAFE_FREE(ptConnectObj);
			uv_close((uv_handle_t *)&(pUVClientEx->UVTcp), CVI_ISPD2_ES_CB_Close);
			return;
		}

		ptConnectObj->u32RecvBufferOffset = 0;
		ptConnectObj->u32RecvBufferSize = JRPC_SERVER_DEFAULT_BUF_SIZE;
		ptConnectObj->eRecvBufferContentType = EEMPTY;
		gettimeofday(&(ptConnectObj->tvLastRecvTime), NULL);
		ptConnectObj->ptDaemonInfo = ptObject;

		uv_os_fd_t UVFd;

		uv_fileno((uv_handle_t *)&(pUVClientEx->UVTcp), &UVFd);
		ptConnectObj->iFd = (int)UVFd;
		pUVClientEx->ptHandle = (void *)ptConnectObj;

		ptObject->u8ClientCount++;

		uv_read_start((uv_stream_t *)pUVClientEx, CVI_ISPD2_ES_CB_AllocBuffer, CVI_ISPD2_ES_CB_SocketRead);
	} else {
		ISP_DAEMON2_DEBUG(LOG_ERR, "Accept fail");

		uv_close((uv_handle_t *)&(pUVClientEx->UVTcp), CVI_ISPD2_ES_CB_Close);
	}
}

// -----------------------------------------------------------------------------
CVI_S32 CVI_ISPD2_ES_CreateService(TISPDaemon2Info *ptObject, uint16_t u16ServicePort)
{
	struct sockaddr_in	stSockAddr;
	int					iListenRes;

	SAFE_FREE(ptObject->pUVLoop);

	ptObject->pUVLoop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
	if (ptObject->pUVLoop == NULL) {
		ISP_DAEMON2_DEBUG(LOG_ERR, "Allocate loop fail");

		return CVI_FAILURE;
	}

	uv_loop_init(ptObject->pUVLoop);

	memset(&(ptObject->UVServerEx), 0, sizeof(uv_stream_t_ex));
	ptObject->UVServerEx.ptHandle = (void *)ptObject;

	uv_tcp_init(ptObject->pUVLoop, &(ptObject->UVServerEx.UVTcp));

	uv_ip4_addr(DEFAULT_ADDRESS, u16ServicePort, &stSockAddr);
	uv_tcp_bind(&(ptObject->UVServerEx.UVTcp), (const struct sockaddr *)&stSockAddr, 0);

	iListenRes = uv_listen((uv_stream_t *)(&(ptObject->UVServerEx)),
		DEFAULT_BACKLOG, CVI_ISPD2_ES_CB_NewConnection);
	if (iListenRes != 0) {
		ISP_DAEMON2_DEBUG_EX(LOG_ERR, "Listen error : %s", uv_strerror(iListenRes));
		SAFE_FREE(ptObject->pUVLoop);
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

// -----------------------------------------------------------------------------
static void *CVI_ISPD2_ES_ServiceThread(void *hHandle)
{
	TISPDaemon2Info *ptObject = (TISPDaemon2Info *)hHandle;
	
	prctl(PR_SET_NAME, "ISPD2_ES_Service", 0, 0, 0);
	
	ptObject->bServiceThreadRunning = CVI_TRUE;

	while (ptObject->bServiceThreadRunning) {
		uv_run(ptObject->pUVLoop, UV_RUN_NOWAIT);
		usleep(TIME_INTERVAL_UV_RUN * 1000);
	}

	pthread_exit(NULL);

	return 0;
}

// -----------------------------------------------------------------------------
CVI_S32 CVI_ISPD2_ES_RunService(TISPDaemon2Info *ptObject)
{
	if (ptObject->bServiceThreadRunning == CVI_TRUE) {
		return CVI_FAILURE;
	}

	pthread_create(&(ptObject->thDaemonThreadId), NULL, CVI_ISPD2_ES_ServiceThread, (void *)ptObject);

	return CVI_SUCCESS;
}

// -----------------------------------------------------------------------------
static void CVI_ISPD2_ES_CB_CloseHandle(uv_handle_t *pHandle, void *pArg)
{
	void *pvServerAddr = ((uv_tcp_t *)pArg);

	if (pHandle == pvServerAddr) {
		uv_close(pHandle, NULL);
	} else {
		uv_close(pHandle, CVI_ISPD2_ES_CB_Close);
	}
}

// -----------------------------------------------------------------------------
CVI_S32 CVI_ISPD2_ES_DestoryService(TISPDaemon2Info *ptObject)
{
	if (ptObject->bServiceThreadRunning != CVI_TRUE) {
		return CVI_FAILURE;
	}
	ptObject->bServiceThreadRunning = CVI_FALSE;

	if (ptObject->pUVLoop) {
		uv_stop(ptObject->pUVLoop);
	}

	pthread_join(ptObject->thDaemonThreadId, NULL);
	ptObject->thDaemonThreadId = 0;

	if (ptObject->pUVLoop) {
		uv_walk(ptObject->pUVLoop, CVI_ISPD2_ES_CB_CloseHandle, &(ptObject->UVServerEx.UVTcp));

		CVI_S8 s8CloseRound = 5;

		while ((s8CloseRound > 0) && (uv_loop_close(ptObject->pUVLoop) != 0)) {
			uv_run(ptObject->pUVLoop, UV_RUN_NOWAIT);
			usleep(TIME_INTERVAL_UV_RUN * 1000);
			s8CloseRound--;
		}

		SAFE_FREE(ptObject->pUVLoop);
	}
	memset(&(ptObject->UVServerEx), 0, sizeof(uv_stream_t_ex));

	return CVI_SUCCESS;
}

// -----------------------------------------------------------------------------
