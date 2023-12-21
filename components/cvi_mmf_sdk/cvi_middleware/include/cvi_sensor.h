/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: include/cvi_sensor.h
 * Description:
 *   MMF Programe Interface for sensor management moudle
 */

#ifndef __CVI_SENSOR_H__
#define __CVI_SENSOR_H__

#include <stdio.h>
#include <cvi_sns_ctrl.h>

typedef CVI_S32 (*AHD_Callback)(CVI_S32);

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

typedef struct _SNS_I2C_GPIO_INFO_S {
    CVI_S8 s8I2cDev;
    CVI_S32 s32I2cAddr;
    CVI_U32 u32Rst_port_idx;
    CVI_U32 u32Rst_pin;
    CVI_U32 u32Rst_pol;
} SNS_I2C_GPIO_INFO_S;

typedef enum _SNS_AHD_MODE_E {
    AHD_MODE_NONE,
    AHD_MODE_1280X720H_NTSC,
    AHD_MODE_1280X720H_PAL,
    AHD_MODE_1280X720P25,
    AHD_MODE_1280X720P30,
    AHD_MODE_1280X720P50,
    AHD_MODE_1280X720P60,
    AHD_MODE_1920X1080P25,
    AHD_MODE_1920X1080P30,
    AHD_MODE_2304X1296P25,
    AHD_MODE_2304X1296P30,
    AHD_MODE_BUIT,
} SNS_AHD_MODE_S;

CVI_VOID CVI_SENSOR_AHDRegisterDetect(AHD_Callback CB);
CVI_S32 CVI_SENSOR_EnableDetect(CVI_S32 ViPipe);
CVI_S32 CVI_SENSOR_GPIO_Init(VI_PIPE ViPipe, SNS_I2C_GPIO_INFO_S *pstGpioCfg);
CVI_S32 CVI_SENSOR_GetAhdStatus(VI_PIPE ViPipe, SNS_AHD_MODE_S *pstStatus);
CVI_S32 CVI_SENSOR_SetSnsType(VI_PIPE ViPipe, CVI_U32 SnsType);
CVI_S32 CVI_SENSOR_SetSnsRxAttr(VI_PIPE ViPipe, RX_INIT_ATTR_S *pstRxAttr);
CVI_S32 CVI_SENSOR_SetSnsI2c(VI_PIPE ViPipe, CVI_S32 astI2cDev, CVI_S32 s32I2cAddr);
CVI_S32 CVI_SENSOR_SetSnsIspAttr(VI_PIPE ViPipe, ISP_INIT_ATTR_S *pstInitAttr);
CVI_S32 CVI_SENSOR_RegCallback(VI_PIPE ViPipe, ISP_DEV IspDev);
CVI_S32 CVI_SENSOR_UnRegCallback(VI_PIPE ViPipe, ISP_DEV IspDev);
CVI_S32 CVI_SENSOR_SetSnsImgMode(VI_PIPE ViPipe, ISP_CMOS_SENSOR_IMAGE_MODE_S *stSnsrMode);
CVI_S32 CVI_SENSOR_SetSnsWdrMode(VI_PIPE ViPipe, WDR_MODE_E wdrMode);
CVI_S32 CVI_SENSOR_GetSnsRxAttr(VI_PIPE ViPipe, SNS_COMBO_DEV_ATTR_S *stDevAttr);
CVI_S32 CVI_SENSOR_SetSnsProbe(VI_PIPE ViPipe);
CVI_S32 CVI_SENSOR_SetSnsGpioInit(CVI_U32 devNo, CVI_U32 u32Rst_port_idx, CVI_U32 u32Rst_pin,
	CVI_U32 u32Rst_pol);
CVI_S32 CVI_SENSOR_RstSnsGpio(CVI_U32 devNo, CVI_U32 rstEnable);
CVI_S32 CVI_SENSOR_RstMipi(CVI_U32 devNo, CVI_U32 rstEnable);
CVI_S32 CVI_SENSOR_SetMipiAttr(VI_PIPE ViPipe, CVI_U32 SnsType);
CVI_S32 CVI_SENSOR_EnableSnsClk(CVI_U32 devNo, CVI_U32 clkEnable);
CVI_S32 CVI_SENSOR_SetSnsStandby(VI_PIPE ViPipe);
CVI_S32 CVI_SENSOR_SetSnsInit(VI_PIPE ViPipe);
CVI_S32 CVI_SENSOR_SetVIFlipMirrorCB(VI_PIPE ViPipe, VI_DEV ViDev);
CVI_S32 CVI_SENSOR_GetAeDefault(VI_PIPE ViPipe, AE_SENSOR_DEFAULT_S *stAeDefault);
CVI_S32 CVI_SENSOR_GetIspBlkLev(VI_PIPE ViPipe, ISP_CMOS_BLACK_LEVEL_S *stBlc);
CVI_S32 CVI_SENSOR_SetSnsFps(VI_PIPE ViPipe, CVI_U8 fps, AE_SENSOR_DEFAULT_S *stSnsDft);
CVI_S32 CVI_SENSOR_GetExpRatio(VI_PIPE ViPipe, SNS_EXP_MAX_S *stExpMax);
CVI_S32 CVI_SENSOR_SetDgainCalc(VI_PIPE ViPipe, SNS_GAIN_S *stDgain);
CVI_S32 CVI_SENSOR_SetAgainCalc(VI_PIPE ViPipe, SNS_GAIN_S *stAgain);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /*__CVI_SENSOR_H__ */
