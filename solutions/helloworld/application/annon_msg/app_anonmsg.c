#include <stdio.h>
#include <unistd.h>
#include "cvi_type.h"
#include "cvi_msg_server.h"
#include "app_anonmsg.h"
#include "cvi_param.h"
#include "cvi_bin.h"

typedef enum _ANONMSG_TYPE_E{
    ANONMSG_PQPARM = 0,
    ANONMSG_MAX,
}ANONMSG_TYPE_E;

static int _prase_pqbin_param(int param)
{
    PARAM_VI_CFG_S * pstViCfg = PARAM_getViCtx();
    if (pstViCfg && pstViCfg->pstIspCfg) {
        if (pstViCfg->pstIspCfg[param/2].stPQBinDes[param%2].pIspBinData != NULL) {
            CVI_BIN_ImportBinData(pstViCfg->pstIspCfg[param/2].stPQBinDes[param%2].pIspBinData, 
            pstViCfg->pstIspCfg[param/2].stPQBinDes[param%2].u32IspBinDataLen);
        }
    }
    return 0;
}

int _custom_msg_process(ANONMSG_TYPE_E msgid, int buf_type, void * buf, unsigned int len)
{
    unsigned long _param = 0;
    if (buf_type == IPCM_MSG_TYPE_RAW_PARAM) {
        _param = (unsigned long)buf;
    }
    switch (msgid)
    {
    case ANONMSG_PQPARM:
        if (buf_type == IPCM_MSG_TYPE_RAW_PARAM) {
            _prase_pqbin_param(_param);
        }
        break;
    default:
        break;
    }
    return 0;
}

int APP_ANONMSG_process(void *priv, IPCM_ANON_MSG_S *data)
{
    unsigned char msg_id, data_type;
    unsigned int data_len;
    int ret = 0;
    if (data == NULL) {
        printf( " _anon_msg_process error handle data null \r\n");
        return -1;
    }
    if (data->u8PortID != IPCM_PORT_ANON_ST) {
        printf( " _anon_msg_process error port_id error \r\n");
        return -1;
    }
    msg_id = data->u8MsgID;
    data_type = data->u8DataType;
    data_len = data->stData.u32Size;

    if (data_type == IPCM_MSG_TYPE_RAW_PARAM) {
        _custom_msg_process(msg_id, data_type, data->stData.pData, data_len);
    } else {
        _custom_msg_process(msg_id, data_type, (void *)(unsigned long)data->u32Param, 4);
    }
    return ret;
}
