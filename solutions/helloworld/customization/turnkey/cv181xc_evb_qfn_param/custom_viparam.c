/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_viparam.c
 * Description:
 *   ....
 */
#include "custom_param.h"

PARAM_CLASSDEFINE(PARAM_SNS_CFG_S,SENSORCFG,CTX,Sensor)[] = {
    {
#if defined (CONFIG_CHIP_cv1811c) || defined (CONFIG_CHIP_cv1801c) || defined (CONFIG_CHIP_cv1812cp)
        .enSnsType = CONFIG_SNS0_TYPE,
        .s32I2cAddr = -1,
        .s8I2cDev = 2,
        .u32Rst_port_idx = 2,//GPIOC_13
        .u32Rst_pin = 13,
        .u32Rst_pol = OF_GPIO_ACTIVE_LOW,
#endif
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || defined (CONFIG_CHIP_cv1811ha)
        .enSnsType = CONFIG_SNS0_TYPE,
        .s32I2cAddr = -1,
        .s8I2cDev = 3,
        .u32Rst_port_idx = 0,//GPIOA_2
        .u32Rst_pin = 2,
        .u32Rst_pol = OF_GPIO_ACTIVE_LOW,
#endif
#if defined (CONFIG_CHIP_cv1801b) || defined (CONFIG_CHIP_cv180zb)
        .enSnsType = CONFIG_SNS0_TYPE,
        .s32I2cAddr = -1,
        .s8I2cDev = 1,
        .u32Rst_port_idx = 2,//GPIOC_8
        .u32Rst_pin = 8,
        .u32Rst_pol = OF_GPIO_ACTIVE_LOW,
#endif
    }
};

PARAM_CLASSDEFINE(PARAM_ISP_CFG_S,ISPCFG,CTX,ISP)[] = {
    {
        .bMonoSet = {0},
        .bUseSingleBin = 0,
        .stPQBinDes[0] =
        {
            .pIspBinData = NULL,
            .u32IspBinDataLen = 0,
        },
    },
};

PARAM_CLASSDEFINE(PARAM_CHN_CFG_S,CHNCFG,CTX,CHN)[] = {
    {
        .enCompressMode = COMPRESS_MODE_TILE,
    },
    {
        .enCompressMode = COMPRESS_MODE_TILE,
    },
};

PARAM_VI_CFG_S g_stViCtx = {
    .u32WorkSnsCnt = 1,
    .pstSensorCfg = PARAM_CLASS(SENSORCFG,CTX,Sensor),
    .pstIspCfg = PARAM_CLASS(ISPCFG,CTX,ISP),
    .pstChnInfo = PARAM_CLASS(CHNCFG,CTX,CHN),
};

PARAM_VI_CFG_S * PARAM_GET_VI_CFG(void) {
    return &g_stViCtx;
}







