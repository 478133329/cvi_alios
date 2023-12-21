
#ifndef __CVI_COMM_IPCM_H__
#define __CVI_COMM_IPCM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#include "cvi_common.h"
#include "cvi_type.h"


typedef enum _IPCM_RTOS_BOOT_STATUS_E {
    IPCM_RTOS_SYS_INIT_STAT = 0,
    IPCM_RTOS_PANIC,
    IPCM_RTOS_IPCM_DONE,
    IPCM_RTOS_IPCMSG_DONE, // TODO
    IPCM_RTOS_VI_DONE, // TODO
    IPCM_RTOS_VPSS_DONE, // TODO
    IPCM_RTOS_VENC_DONE, // TODO

    IPCM_RTOS_BOOT_STATUS_BUTT,
} IPCM_RTOS_BOOT_STATUS_E;

typedef enum _IPCM_PORT_TYPE_E {
	IPCM_PORT_SYSTEM = 0,
	IPCM_PORT_MSG,
	IPCM_PORT_ANON_ST,
	IPCM_PORT_BUTT = 255,
} IPCM_PORT_TYPE_E;

typedef struct _IPCM_MSG_PTR_S {
	CVI_U32 data_pos : 20;
	CVI_U32 remaining_rd_len : 12;
} IPCM_MSG_PTR_S;

typedef union _IPCM_MSG_PARAM_U {
	IPCM_MSG_PTR_S stMsgPtr;
	CVI_U32 u32Param;
} IPCM_MSG_PARAM_U;

typedef enum _IPCM_MSG_TYPE_E {
	IPCM_MSG_TYPE_SHM = 0,	// msg_param is share memory addr
	IPCM_MSG_TYPE_RAW_PARAM,	// msg_param is the param
} IPCM_MSG_TYPE_E;

typedef struct _IPCM_MSG_DATA_S {
	CVI_U8 u8PortID;
	CVI_U8 u8MsgID : 7;
	CVI_U8 u8DataType : 1;
	CVI_U16 resv;
	IPCM_MSG_PARAM_U stMsgParam;
} IPCM_MSG_DATA_S;

typedef struct _IPCM_ANON_SHM_DATA_S {
    CVI_VOID *pData;
    CVI_U32 u32Size;
} IPCM_ANON_SHM_DATA_S;

typedef struct _IPCM_ANON_MSG_S {
    CVI_U8 u8PortID;
    CVI_U8 u8MsgID : 7;
	CVI_U8 u8DataType : 1;
	union {
		IPCM_ANON_SHM_DATA_S stData; // while u8DataType is 0
		CVI_U32 u32Param; // while u8DataType is 1
	};
} IPCM_ANON_MSG_S;

typedef CVI_S32 (*IPCM_ANON_MSGPROC_FN)(CVI_VOID *pPriv, IPCM_ANON_MSG_S *pstData);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __CVI_COMM_IPCM_H__ */
