#include "cvi_param.h"

static int s_scene_mode = 0;

//parm 结构 入口
CVI_S32 PARAM_LoadCfg(CVI_VOID) {
    //Media 流媒体 能力集
    PARAM_INIT_MANAGER_CFG();
    return 0;
}
//VB
PARAM_SYS_CFG_S *PARAM_getSysCtx(void)
{
    if(PARAM_GET_MANAGER_CFG() == NULL) {
        return NULL;
    }
    return PARAM_GET_MANAGER_CFG()->pstSysCtx;
}
//VI
PARAM_VI_CFG_S *PARAM_getViCtx(void)
{
    return PARAM_GET_MANAGER_CFG()->pstViCtx;
}
//VPSS
PARAM_VPSS_CFG_S *PARAM_getVpssCtx(void)
{
    if(PARAM_GET_MANAGER_CFG() == NULL) {
        return NULL;
    }
    return PARAM_GET_MANAGER_CFG()->pstVpssCfg;
}
//VENC
PARAM_VENC_CFG_S *PARAM_getVencCtx(void)
{
    if(PARAM_GET_MANAGER_CFG() == NULL) {
        return NULL;
    }
    return PARAM_GET_MANAGER_CFG()->pstVencCfg;
}
//VO
PARAM_VO_CFG_S *PARAM_getVoCtx(void)
{
    if(PARAM_GET_MANAGER_CFG() == NULL) {
        return NULL;
    }
    return PARAM_GET_MANAGER_CFG()->pstVoCfg;
}

void PARAM_setPipeline(int pipeline)
{
    PARAM_SET_MANAGER_CFG_PIPE(pipeline);
}

int PARAM_getPipeline(void)
{
    return PARAM_GET_MANAGER_CFG_PIPE();
}

void PARAM_setSceneMode(int mode)
{
    s_scene_mode = mode;
}

int PARAM_getSceneMode()
{
    return s_scene_mode;
}

int PARAM_Reinit_RawReplay(void)
{
    PARAM_getSysCtx()->pstVbPool->u8VbBlkCnt = 2;
    PARAM_getSysCtx()->u8VbPoolCnt = 1;
    PARAM_getSysCtx()->u8ViCnt = 2;
    PARAM_getSysCtx()->u8VbPoolCnt = 1;

    PARAM_getSysCtx()->stVPSSMode.enMode = VPSS_MODE_DUAL;
    PARAM_getSysCtx()->stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_ONLINE;
    PARAM_getSysCtx()->stVPSSMode.aenInput[0] = VPSS_INPUT_MEM;
    PARAM_getSysCtx()->stVIVPSSMode.aenMode[1] = VI_OFFLINE_VPSS_ONLINE;
    PARAM_getSysCtx()->stVPSSMode.aenInput[1] = VPSS_INPUT_ISP;

    PARAM_getViCtx()->u32WorkSnsCnt = 1;
    PARAM_getViCtx()->pstChnInfo[0].enCompressMode = COMPRESS_MODE_NONE;
    PARAM_getViCtx()->pstChnInfo[0].bYuvBypassPath = CVI_FALSE;

    PARAM_getVpssCtx()->u8GrpCnt = 1;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].VpssGrp = 0;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].u8ChnCnt = 1;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].s32BindVidev = 0;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].stVpssGrpAttr.u8VpssDev = 1;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].bBindMode = CVI_FALSE;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[0].enModId = CVI_ID_VI;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[0].s32DevId = 0;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[0].s32ChnId = 0;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[1].enModId = CVI_ID_VPSS;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[1].s32DevId = 0;
    PARAM_getVpssCtx()->pstVpssGrpCfg[0].astChn[1].s32ChnId = 0;

    PARAM_getVencCtx()->s32VencChnCnt = 1;
    PARAM_getVencCtx()->pstVencChnCfg[0].stChnParam.u8DevChnid = 0;
    PARAM_getVencCtx()->pstVencChnCfg[0].stChnParam.u8DevId = 0;
    PARAM_getVencCtx()->pstVencChnCfg[0].stChnParam.u8VencChn = 0;
    PARAM_getVencCtx()->pstVencChnCfg[0].stChnParam.u8ModId = CVI_ID_VPSS;
    return 0;
}