/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_sysparam.c
 * Description:
 *   ....
 */
#include "custom_param.h"
#include "board_config.h"

PARAM_CLASSDEFINE(PARAM_VB_CFG_S,VBPOOL,CTX,VB)[] = {
/*    {
        .u16width = 1920,
        .u16height = 1080,
        .u8VbBlkCnt = 2,
        .fmt = PIXEL_FORMAT_YUV_PLANAR_420,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
    },
    #if 0
    {
        .u16width = PANEL_WIDTH,
        .u16height = PANEL_HEIGHT,
        .u8VbBlkCnt = 1,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
    },*/
    {
        .u16width = 2560,
        .u16height = 1440,
        .u8VbBlkCnt = 4,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
    },
    {
        .u16width = 1280,
        .u16height = 720,
        .u8VbBlkCnt = 3,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
    },
};

PARAM_SYS_CFG_S  g_stSysCtx = {
    .u8VbPoolCnt = 2,
    //.stVPSSMode.enMode = VPSS_MODE_DUAL,
    .u8ViCnt = 2,
    .stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_ONLINE,
    .stVIVPSSMode.aenMode[1] = VI_OFFLINE_VPSS_ONLINE,
    .stVPSSMode.enMode = VPSS_MODE_DUAL,
    .stVPSSMode.ViPipe[0] = 0,
    .stVPSSMode.aenInput[0] = VPSS_INPUT_MEM,
    .stVPSSMode.ViPipe[1] = 0,
    .stVPSSMode.aenInput[1] = VPSS_INPUT_ISP,
    .pstVbPool = PARAM_CLASS(VBPOOL,CTX,VB),
};

PARAM_SYS_CFG_S * PARAM_GET_SYS_CFG(void) {
    return &g_stSysCtx;
}
