#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "cvi_type.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"
//#include "isp_mgr_buf.h"
#include "cvi_comm_isp.h"
#include "3A_internal.h"
#include "cvi_isp.h"
#include "cvi_ae.h"
#include "cvi_awb.h"
#include "cvi_sys.h"
#include "cvi_vi.h"
#include "cvi_sns_ctrl.h"
#include "sensor_cfg.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define CHECK_MSG_SIZE(type, size)												\
    if (sizeof(type) != size) {												\
        CVI_TRACE_MSG("size mismatch !!! expect size: %lu, actual size: %u\n", sizeof(type), size);			\
        return CVI_FAILURE;												\
    }

static int sensor_init_by_ipcmsg(VI_PIPE ViPipe, ISP_SNS_CFG_S *pstSnsCfg)
{
    //Sensor
    SNS_COMBO_DEV_ATTR_S devAttr = {0};
    CVI_S32 snsr_type;
    ISP_SNS_OBJ_S *pSnsObj;
    ISP_CMOS_SENSOR_IMAGE_MODE_S stSnsrMode;
    ISP_SENSOR_EXP_FUNC_S stSnsrSensorFunc = {0};
    ISP_INIT_ATTR_S InitAttr = {0};
    ALG_LIB_S stAeLib = {0};
    ALG_LIB_S stAwbLib = {0};
    RX_INIT_ATTR_S rx_init_attr = {0};
    CVI_S32 s32Ret;

    if(pstSnsCfg == NULL) {
        CVI_TRACE_MSG("%s pstSnsCfg NULL err \n",__func__);
        return CVI_FAILURE;
    }

    snsr_type = get_sensor_type(ViPipe);
    pSnsObj = getSnsObj(snsr_type);
    // getSnsMode(ViPipe, &stSnsrMode);
    /* clock enable */
    // vip_clk_en();
    /************************************************
     * start sensor
     ************************************************/
    InitAttr.enGainMode = SNS_GAIN_MODE_SHARE;

    if (!pSnsObj) {
        CVI_TRACE_MSG("sns obj[%d] is null.\n", ViPipe);
        return CVI_FAILURE;
    }
    InitAttr.u16UseHwSync = pstSnsCfg->bHwSync;

    rx_init_attr.MipiDev       = pstSnsCfg->S32MipiDevno;
    rx_init_attr.stMclkAttr.bMclkEn = pstSnsCfg->bMclkEn;
    rx_init_attr.stMclkAttr.u8Mclk = pstSnsCfg->u8Mclk;
    for (int i = 0; i < MIPI_LANE_NUM + 1; i++) {
        rx_init_attr.as16LaneId[i] = pstSnsCfg->lane_id[i];
        rx_init_attr.as8PNSwap[i] = pstSnsCfg->pn_swap[i];
    }

    if (pSnsObj->pfnPatchRxAttr) {
        s32Ret = pSnsObj->pfnPatchRxAttr(&rx_init_attr);
    }
    if(pSnsObj->pfnSetInit){
        pSnsObj->pfnSetInit(ViPipe, &InitAttr);
    }
    if(pSnsObj->pfnRegisterCallback){
        pSnsObj->pfnRegisterCallback(ViPipe, &stAeLib, &stAwbLib);
    }
    if(pSnsObj->pfnExpSensorCb){
        pSnsObj->pfnExpSensorCb(&stSnsrSensorFunc);
    }
    if(stSnsrSensorFunc.pfn_cmos_sensor_global_init){
        stSnsrSensorFunc.pfn_cmos_sensor_global_init(ViPipe);
    }
    if(stSnsrSensorFunc.pfn_cmos_set_wdr_mode){
        s32Ret = stSnsrSensorFunc.pfn_cmos_set_wdr_mode(ViPipe, pstSnsCfg->enWDRMode);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("sensor set wdr mode failed!\n");
            return CVI_FAILURE;
        }
    }
    stSnsrMode.u16Width = pstSnsCfg->stSnsSize.u32Width;
    stSnsrMode.u16Height = pstSnsCfg->stSnsSize.u32Height;
    stSnsrMode.f32Fps = pstSnsCfg->f32FrameRate;
    if(stSnsrSensorFunc.pfn_cmos_set_image_mode){
        s32Ret = stSnsrSensorFunc.pfn_cmos_set_image_mode(ViPipe, &stSnsrMode);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("sensor set image mode failed!\n");
            return CVI_FAILURE;
        }
    }

    cif_reset_mipi(ViPipe);
    usleep(100);
    if(pSnsObj->pfnGetRxAttr){
        pSnsObj->pfnGetRxAttr(ViPipe, &devAttr);
    }
    devAttr.devno = pstSnsCfg->S32MipiDevno;

    cif_set_dev_attr(&devAttr);
    cif_enable_snsr_clk(ViPipe, 1);
    usleep(100);
    if (pSnsObj->pfnSnsProbe) {
        s32Ret = pSnsObj->pfnSnsProbe(ViPipe);
        if (s32Ret) {
            CVI_TRACE_MSG("sensor probe failed!\n");
            return CVI_FAILURE;
        }
    }
    return CVI_SUCCESS;
}
#if 1
static  int sensor_exit_by_ipcmsg(VI_PIPE ViPipe, ISP_SNS_CFG_S *pstSnsInfo)
{
    CVI_TRACE_MSG("just for test api not achieve!\n");
    return CVI_SUCCESS;
}
#endif

static CVI_S32 MSG_ISP_SnsInit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

    CHECK_MSG_SIZE(ISP_SNS_CFG_S, pstMsg->u32BodyLen);
    s32Ret = sensor_init_by_ipcmsg(ViPipe, (ISP_SNS_CFG_S *)pstMsg->pBody);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_ISP_SnsInit Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_ISP_SnsExit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    s32Ret = sensor_exit_by_ipcmsg(ViPipe, (ISP_SNS_CFG_S *)pstMsg->pBody);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_ISP_SnsExit Failed : %#x!\n", s32Ret);
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

static int start_isp(VI_PIPE ViPipe)
{
    CVI_U32 s32Ret = CVI_SUCCESS;

    VI_DEV_ATTR_S stViDevAttr;
    ISP_PUB_ATTR_S stPubAttr;

    memset(&stViDevAttr, 0, sizeof(VI_DEV_ATTR_S));
    memset(&stPubAttr, 0, sizeof(ISP_PUB_ATTR_S));

    s32Ret = CVI_VI_GetDevAttr(ViPipe, &stViDevAttr);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_VI_GetDevAttr failed!\n");
    }

    stPubAttr.stWndRect.u32Width = stPubAttr.stSnsSize.u32Width = stViDevAttr.stSize.u32Width;
    stPubAttr.stWndRect.u32Height = stPubAttr.stSnsSize.u32Height = stViDevAttr.stSize.u32Height;
    stPubAttr.f32FrameRate = 25;
    stPubAttr.enBayer = stViDevAttr.enBayerFormat;
    stPubAttr.enWDRMode = stViDevAttr.stWDRAttr.enWDRMode;

    //Param init
    ISP_STATISTICS_CFG_S stsCfg = {0};
    CVI_ISP_GetStatisticsConfig(ViPipe, &stsCfg);
    stsCfg.stAECfg.stCrop[0].bEnable = 0;
    stsCfg.stAECfg.stCrop[0].u16X = stsCfg.stAECfg.stCrop[0].u16Y = 0;
    stsCfg.stAECfg.stCrop[0].u16W = stPubAttr.stWndRect.u32Width;
    stsCfg.stAECfg.stCrop[0].u16H = stPubAttr.stWndRect.u32Height;
    memset(stsCfg.stAECfg.au8Weight, 1, AE_WEIGHT_ZONE_ROW * AE_WEIGHT_ZONE_COLUMN * sizeof(CVI_U8));
    stsCfg.stWBCfg.u16ZoneRow = AWB_ZONE_ORIG_ROW;
    stsCfg.stWBCfg.u16ZoneCol = AWB_ZONE_ORIG_COLUMN;
    stsCfg.stWBCfg.stCrop.bEnable = 0;
    stsCfg.stWBCfg.stCrop.u16X = stsCfg.stWBCfg.stCrop.u16Y = 0;
    stsCfg.stWBCfg.stCrop.u16W = stPubAttr.stWndRect.u32Width;
    stsCfg.stWBCfg.stCrop.u16H = stPubAttr.stWndRect.u32Height;
    stsCfg.stWBCfg.u16BlackLevel = 0;
    stsCfg.stWBCfg.u16WhiteLevel = 4095;
    stsCfg.stFocusCfg.stConfig.bEnable = 1;
    stsCfg.stFocusCfg.stConfig.u8HFltShift = 1;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[0] = 1;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[1] = 2;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[2] = 3;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[3] = 5;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[4] = 10;
    stsCfg.stFocusCfg.stConfig.stRawCfg.PreGammaEn = 0;
    stsCfg.stFocusCfg.stConfig.stPreFltCfg.PreFltEn = 1;
    stsCfg.stFocusCfg.stConfig.u16Hwnd = 17;
    stsCfg.stFocusCfg.stConfig.u16Vwnd = 15;
    stsCfg.stFocusCfg.stConfig.stCrop.bEnable = 0;
    // AF offset and size has some limitation.
    stsCfg.stFocusCfg.stConfig.stCrop.u16X = AF_XOFFSET_MIN;
    stsCfg.stFocusCfg.stConfig.stCrop.u16Y = AF_YOFFSET_MIN;
    stsCfg.stFocusCfg.stConfig.stCrop.u16W = stPubAttr.stWndRect.u32Width - AF_XOFFSET_MIN * 2;
    stsCfg.stFocusCfg.stConfig.stCrop.u16H = stPubAttr.stWndRect.u32Height - AF_YOFFSET_MIN * 2;
    //Horizontal HP0
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[0] = 0;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[1] = 0;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[2] = 13;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[3] = 24;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[4] = 0;
    //Horizontal HP1
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[0] = 1;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[1] = 2;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[2] = 4;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[3] = 8;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[4] = 0;
    //Vertical HP
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[0] = 13;
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[1] = 24;
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[2] = 0;
    stsCfg.unKey.bit1FEAeGloStat = stsCfg.unKey.bit1FEAeLocStat =
    stsCfg.unKey.bit1AwbStat1 = stsCfg.unKey.bit1AwbStat2 = stsCfg.unKey.bit1FEAfStat = 1;
    //LDG
    stsCfg.stFocusCfg.stConfig.u8ThLow = 0;
    stsCfg.stFocusCfg.stConfig.u8ThHigh = 255;
    stsCfg.stFocusCfg.stConfig.u8GainLow = 30;
    stsCfg.stFocusCfg.stConfig.u8GainHigh = 20;
    stsCfg.stFocusCfg.stConfig.u8SlopLow = 8;
    stsCfg.stFocusCfg.stConfig.u8SlopHigh = 15;
    //Register callback & call API
    if (stViDevAttr.enInputDataType != VI_DATA_TYPE_YUV) {
        ALG_LIB_S stAeLib, stAwbLib;
        stAeLib.s32Id = stAwbLib.s32Id = ViPipe;
        ISP_BIND_ATTR_S stBindAttr;
        stBindAttr.stAeLib.s32Id = stBindAttr.stAwbLib.s32Id = ViPipe;

        strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);
        strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);
        strncpy(stBindAttr.stAeLib.acLibName, CVI_AE_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);
        strncpy(stBindAttr.stAwbLib.acLibName, CVI_AWB_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);

        s32Ret = CVI_AE_Register(ViPipe, &stAeLib);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("AE Algo register failed!, error: %d\n",	s32Ret);
            return s32Ret;
        }
        s32Ret = CVI_AWB_Register(ViPipe, &stAwbLib);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("AWB Algo register failed!, error: %d\n", s32Ret);
            return s32Ret;
        }
        s32Ret = CVI_ISP_SetBindAttr(ViPipe, &stBindAttr);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("Bind Algo failed with %d!\n", s32Ret);
        }
        s32Ret = CVI_ISP_MemInit(ViPipe);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("Init Ext memory failed with %#x!\n", s32Ret);
            return s32Ret;
        }
        s32Ret = CVI_ISP_SetPubAttr(ViPipe, &stPubAttr);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("SetPubAttr failed with %#x!\n", s32Ret);
            return s32Ret;
        }
        s32Ret = CVI_ISP_SetStatisticsConfig(ViPipe, &stsCfg);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("ISP Set Statistic failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_ISP_Init(ViPipe);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("ISP Init failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        //Run ISP
        s32Ret = CVI_ISP_Run(ViPipe);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("ISP Run failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    } else {
        s32Ret = CVI_ISP_Init(ViPipe);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("ISP Init failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }

    return s32Ret;
}

static int stop_isp(VI_PIPE ViPipe)
{
    //Param init
    CVI_S32 s32Ret;
    VI_DEV_ATTR_S stViDevAttr;

    memset(&stViDevAttr, 0, sizeof(VI_DEV_ATTR_S));

    s32Ret = CVI_VI_GetDevAttr(ViPipe, &stViDevAttr);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_VI_GetDevAttr failed!\n");
    }

    if (stViDevAttr.enInputDataType != VI_DATA_TYPE_YUV) {
        ALG_LIB_S stAeLib, stAwbLib;
        stAeLib.s32Id = stAwbLib.s32Id = ViPipe;
        strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);
        strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, ALG_LIB_NAME_SIZE_MAX);
        //Stop ISP
        s32Ret = CVI_ISP_Exit(ViPipe);
        if (s32Ret != CVI_SUCCESS) {
            CVI_TRACE_MSG("ISP Exit failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_AE_UnRegister(ViPipe, &stAeLib);
        if (s32Ret) {
            CVI_TRACE_MSG("AE Algo unRegister failed!, error: %d\n", s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_AWB_UnRegister(ViPipe, &stAwbLib);
        if (s32Ret) {
            CVI_TRACE_MSG("AWB Algo unRegister failed!, error: %d\n", s32Ret);
            return s32Ret;
        }
    }

    return s32Ret;
}

extern CVI_S32 isp_mgr_buf_get_shared_buf_paddr(VI_PIPE ViPipe, CVI_U64 *paddr);

static CVI_S32 MSG_ISP_Init(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    ISP_PUB_ATTR_S stPubAttr;
    CVI_U64 u64PhyAddr = 0x00;

    memset(&stPubAttr, 0, sizeof(ISP_PUB_ATTR_S));
    CVI_ISP_GetPubAttr(ViPipe, &stPubAttr);

    if ((int)(stPubAttr.f32FrameRate * 100) == 0) {
        start_isp(ViPipe);
    }

    s32Ret = isp_mgr_buf_get_shared_buf_paddr(ViPipe, &u64PhyAddr);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &u64PhyAddr, sizeof(CVI_U64));
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_Exit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U64 u64PhyAddr = 0x00;

    UNUSED(pstMsg);

    s32Ret = isp_mgr_buf_get_shared_buf_paddr(ViPipe, &u64PhyAddr);

    if (s32Ret == CVI_SUCCESS) {
        stop_isp(ViPipe);
    }
    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_GetVDTimeout(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    ISP_VD_TYPE_E enIspVDType = (ISP_VD_TYPE_E) pstMsg->as32PrivData[0];
    CVI_U32 u32MilliSec = pstMsg->as32PrivData[1];

    s32Ret = CVI_ISP_GetVDTimeOut(ViPipe, enIspVDType, u32MilliSec);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_SetSmartInfo(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U8 TimeOut = pstMsg->as32PrivData[0];

    s32Ret = CVI_ISP_SetSmartInfo(ViPipe, (ISP_SMART_INFO_S *) pstMsg->pBody, TimeOut);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_GetSmartInfo(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

    ISP_SMART_INFO_S info;

    s32Ret = CVI_ISP_GetSmartInfo(ViPipe, &info);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &info, sizeof(ISP_SMART_INFO_S));
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_GetIonInfo(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    CVI_S32 total;
    CVI_S32 used;
    CVI_S32 mfree;
    CVI_S32 peak;
    CVI_S32 dummy;

    s32Ret = CVI_ISP_GetIonInfo(&total, &used, &mfree, &peak);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_ISP_GetAERawReplayFrmNum Failed : %#x!\n", s32Ret);
    }

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &dummy, sizeof(dummy));
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
    }

    respMsg->as32PrivData[0] = total;
    respMsg->as32PrivData[1] = used;
    respMsg->as32PrivData[2] = mfree;
    respMsg->as32PrivData[3] = peak;

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);
    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetLogBufSize(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;

    s32Ret = CVI_ISP_GetAELogBufSize(ViPipe, &bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, bufSize, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetLogBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;
    CVI_U64 u64TempPhyAddr;
    CVI_U8 *pbuf;

    bufSize = pstMsg->as32PrivData[0];
    memcpy(&u64TempPhyAddr, pstMsg->pBody, sizeof(CVI_U64));

    pbuf = (CVI_U8 *) u64TempPhyAddr;

    s32Ret = CVI_ISP_GetAELogBuf(ViPipe, pbuf, bufSize);

    CVI_SYS_IonFlushCache(u64TempPhyAddr, (CVI_VOID *) pbuf, bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetBinBufSize(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;

    s32Ret = CVI_ISP_GetAEBinBufSize(ViPipe, &bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, bufSize, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AESetRawDumpFrameID(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 fid = pstMsg->as32PrivData[0];
    CVI_U16 frmNum = pstMsg->as32PrivData[1];

    s32Ret = CVI_ISP_AESetRawDumpFrameID(ViPipe, fid, frmNum);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AE_GetRawReplayExpBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;
    CVI_U32 return_bufSize;
    CVI_U64 u64TempPhyAddr;
    CVI_U8 *pbuf;

    bufSize = pstMsg->as32PrivData[0];

    memcpy(&u64TempPhyAddr, pstMsg->pBody, sizeof(CVI_U64));

    pbuf = (CVI_U8 *) u64TempPhyAddr;

    s32Ret = CVI_ISP_AEGetRawReplayExpBuf(ViPipe, pbuf, &return_bufSize);

    CVI_SYS_IonFlushCache(u64TempPhyAddr, (CVI_VOID *) pbuf, bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &u64TempPhyAddr, sizeof(CVI_U64));
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    respMsg->as32PrivData[0] = return_bufSize;

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);
    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AESetRawReplayMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_BOOL bMode = pstMsg->as32PrivData[0];

    CVI_ISP_AESetRawReplayMode(ViPipe, bMode);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AESetRawReplayExposure(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

    CHECK_MSG_SIZE(ISP_EXP_INFO_S, pstMsg->u32BodyLen);
    s32Ret = CVI_ISP_AESetRawReplayExposure(ViPipe, (ISP_EXP_INFO_S *)pstMsg->pBody);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_ISP_AESetRawReplayExposure Failed : %#x!\n", s32Ret);
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

static CVI_S32 MSG_ISP_AEGetRawReplayFrmNum(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    CVI_U8 bootfrmNum;
    CVI_U8 ispDgainPeriodNum;

    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);

    s32Ret = CVI_ISP_GetAERawReplayFrmNum(ViPipe, &bootfrmNum, &ispDgainPeriodNum);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("CVI_ISP_GetAERawReplayFrmNum Failed : %#x!\n", s32Ret);
    }

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
    }

    respMsg->as32PrivData[0] = bootfrmNum;
    respMsg->as32PrivData[1] = ispDgainPeriodNum;

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);
    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetBinBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;
    CVI_U64 u64TempPhyAddr;
    CVI_U8 *pbuf;

    bufSize = pstMsg->as32PrivData[0];
    memcpy(&u64TempPhyAddr, pstMsg->pBody, sizeof(CVI_U64));

    pbuf = (CVI_U8 *) u64TempPhyAddr;

    s32Ret = CVI_ISP_GetAEBinBuf(ViPipe, pbuf, bufSize);

    CVI_SYS_IonFlushCache(u64TempPhyAddr, (CVI_VOID *) pbuf, bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AWBGetLogBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;
    CVI_U64 u64TempPhyAddr;
    CVI_U8 *pbuf;

    bufSize = pstMsg->as32PrivData[0];
    memcpy(&u64TempPhyAddr, pstMsg->pBody, sizeof(CVI_U64));

    pbuf = (CVI_U8 *) u64TempPhyAddr;

    s32Ret = CVI_ISP_GetAWBSnapLogBuf(ViPipe, pbuf, bufSize);

    CVI_SYS_IonFlushCache(u64TempPhyAddr, (CVI_VOID *) pbuf, bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AWBGetBinSize(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    //VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;

    bufSize = CVI_ISP_GetAWBDbgBinSize();

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, bufSize, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AWBGetBinBuf(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 bufSize;
    CVI_U64 u64TempPhyAddr;
    CVI_U8 *pbuf;

    bufSize = pstMsg->as32PrivData[0];
    memcpy(&u64TempPhyAddr, pstMsg->pBody, sizeof(CVI_U64));

    pbuf = (CVI_U8 *) u64TempPhyAddr;

    s32Ret = CVI_ISP_GetAWBDbgBinBuf(ViPipe, pbuf, bufSize);

    CVI_SYS_IonFlushCache(u64TempPhyAddr, (CVI_VOID *) pbuf, bufSize);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetFrameID(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 frameID;

    s32Ret = CVI_ISP_GetFrameID(ViPipe, &frameID);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, frameID, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetFps(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_FLOAT fps;

    s32Ret = CVI_ISP_QueryFps(ViPipe, &fps);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, fps * 100, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AEGetLVX100(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_S16 s16Lv;

    s32Ret = CVI_ISP_GetCurrentLvX100(ViPipe, &s16Lv);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s16Lv, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AESetFastBootExposure(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U32 expLine = pstMsg->as32PrivData[0];
    CVI_U32 again = pstMsg->as32PrivData[1];
    CVI_U32 dgain = pstMsg->as32PrivData[2];
    CVI_U32 ispdgain = pstMsg->as32PrivData[3];

    s32Ret = CVI_ISP_SetFastBootExposure(ViPipe, expLine, again, dgain, ispdgain);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AESetAeSimMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

    CVI_BOOL bMode = pstMsg->as32PrivData[0];
    CVI_AE_SetAeSimMode(bMode);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}


static CVI_S32 MSG_ISP_AWBSetSimMode(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;

    CVI_BOOL bMode = pstMsg->as32PrivData[0];
    CVI_ISP_SetAwbSimMode(bMode);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AWBGetRunStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_S32 dummy;
	CVI_BOOL bState;

    bState = CVI_ISP_GetAwbRunStatus(ViPipe);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &dummy, sizeof(dummy));
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("call CVI_IPCMSG_CreateRespMessage fail\n");
    }

    respMsg->as32PrivData[0] = bState;

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);
    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_AWBSetRunStatus(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_BOOL bState = pstMsg->as32PrivData[0];
    CVI_ISP_SetAwbRunStatus(ViPipe, bState);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_SetFMWState(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    ISP_FMW_STATE_E enState = (ISP_FMW_STATE_E) pstMsg->as32PrivData[0];

    s32Ret = CVI_ISP_SetFMWState(ViPipe, enState);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_GetFMWState(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    ISP_FMW_STATE_E enState;

    s32Ret = CVI_ISP_GetFMWState(ViPipe, &enState);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, enState, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_MediaVideoInit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    CVI_BOOL bIsRawReplayMode = pstMsg->as32PrivData[0];
    s32Ret = CVI_ISP_MediaVideoInit(bIsRawReplayMode);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_MediaVideoDeinit(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    CVI_BOOL bIsRawReplayMode = pstMsg->as32PrivData[0];
    s32Ret = CVI_ISP_MediaVideoDeinit(bIsRawReplayMode);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_SetBypassFrm(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U8 bypassNum = pstMsg->as32PrivData[0];

    s32Ret = CVI_ISP_SetBypassFrm(ViPipe, bypassNum);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static CVI_S32 MSG_ISP_GetBypassFrm(CVI_S32 siId, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
    CVI_S32 s32Ret;
    CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
    VI_PIPE ViPipe = GET_DEV_ID(pstMsg->u32Module);
    CVI_U8 bypassNum;

    s32Ret = CVI_ISP_GetBypassFrm(ViPipe, &bypassNum);

    respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
    if (respMsg == CVI_NULL) {
        CVI_TRACE_MSG("create respMsg error\n");
    }

    respMsg->as32PrivData[0] = bypassNum;

    s32Ret = CVI_IPCMSG_SendOnly(siId, respMsg);
    if (s32Ret != CVI_SUCCESS) {
        CVI_TRACE_MSG("send msg fail,ret:%x\n", s32Ret);
        CVI_IPCMSG_DestroyMessage(respMsg);
        return s32Ret;
    }

    CVI_IPCMSG_DestroyMessage(respMsg);

    return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stIspCmdTable[] = {
    { MSG_CMD_ISP_SENSOR_INIT, MSG_ISP_SnsInit},
    { MSG_CMD_ISP_SENSOR_EXIT, MSG_ISP_SnsExit},
    { MSG_CMD_ISP_INIT, MSG_ISP_Init},
    { MSG_CMD_ISP_EXIT, MSG_ISP_Exit},
    { MSG_CMD_ISP_SET_FMW_STATE, MSG_ISP_SetFMWState},
    { MSG_CMD_ISP_GET_FMW_STATE, MSG_ISP_GetFMWState},
    { MSG_CMD_ISP_GET_VD_TIMEOUT, MSG_ISP_GetVDTimeout},
    { MSG_CMD_ISP_SET_SMART_INFO, MSG_ISP_SetSmartInfo},
    { MSG_CMD_ISP_GET_SMART_INFO, MSG_ISP_GetSmartInfo},
    { MSG_CMD_ISP_GET_ION_INFO, MSG_ISP_GetIonInfo},
    { MSG_CMD_ISP_AE_GET_BUF_SIZE, MSG_ISP_AEGetLogBufSize},
    { MSG_CMD_ISP_AE_GET_BUF, MSG_ISP_AEGetLogBuf},
    { MSG_CMD_ISP_AE_SET_RAW_DUMP_FID,  MSG_ISP_AESetRawDumpFrameID},
    { MSG_CMD_ISP_AE_GET_RAW_REPLAY_EXP_BUF, MSG_ISP_AE_GetRawReplayExpBuf},
    { MSG_CMD_ISP_AE_SET_RAW_REPLAY_MODE, MSG_ISP_AESetRawReplayMode},
    { MSG_CMD_ISP_AE_SET_RAW_REPLAY_EXPOSURE, MSG_ISP_AESetRawReplayExposure},
    { MSG_CMD_ISP_AE_GET_RAW_REPLAY_FRM_NUM, MSG_ISP_AEGetRawReplayFrmNum},
    { MSG_CMD_ISP_AE_GET_BIN_SIZE, MSG_ISP_AEGetBinBufSize},
    { MSG_CMD_ISP_AE_GET_BIN_BUF, MSG_ISP_AEGetBinBuf},
    { MSG_CMD_ISP_AE_GET_FRAME_ID, MSG_ISP_AEGetFrameID},
    { MSG_CMD_ISP_AE_GET_FPS, MSG_ISP_AEGetFps},
    { MSG_CMD_ISP_AE_GET_LVX100, MSG_ISP_AEGetLVX100},
    { MSG_CMD_ISP_AE_SET_FASTBOOT_EXPOSURE, MSG_ISP_AESetFastBootExposure},
    { MSG_CMD_ISP_AE_SET_SIM_MODE, MSG_ISP_AESetAeSimMode},
    { MSG_CMD_ISP_AWB_GET_LOG_BUF, MSG_ISP_AWBGetLogBuf},
    { MSG_CMD_ISP_AWB_GET_BIN_SIZE, MSG_ISP_AWBGetBinSize},
    { MSG_CMD_ISP_AWB_GET_BIN_BUF, MSG_ISP_AWBGetBinBuf},
    { MSG_CMD_ISP_AWB_SET_SIM_MODE, MSG_ISP_AWBSetSimMode},
    { MSG_CMD_ISP_AWB_GET_RUN_STATUS, MSG_ISP_AWBGetRunStatus},
    { MSG_CMD_ISP_AWB_SET_RUN_STATUS, MSG_ISP_AWBSetRunStatus},
    { MSG_CMD_ISP_MEDIA_VIDEO_INIT, MSG_ISP_MediaVideoInit},
    { MSG_CMD_ISP_MEDIA_VIDEO_DEINIT, MSG_ISP_MediaVideoDeinit},
    { MSG_CMD_ISP_SET_BYPASS_FRM, MSG_ISP_SetBypassFrm},
    { MSG_CMD_ISP_GET_BYPASS_FRM, MSG_ISP_GetBypassFrm},
};

MSG_SERVER_MODULE_S g_stModuleIsp = {
    CVI_ID_ISP,
    "isp",
    sizeof(g_stIspCmdTable) / sizeof(MSG_MODULE_CMD_S),
    &g_stIspCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetIspMod(CVI_VOID)
{
    return &g_stModuleIsp;
}


