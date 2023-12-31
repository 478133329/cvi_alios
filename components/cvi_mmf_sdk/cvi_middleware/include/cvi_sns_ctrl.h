#ifndef __CVI_SNS_CTRL_H__
#define __CVI_SNS_CTRL_H__

#include <cvi_comm_cif.h>
#include <cvi_errno.h>
#include <cvi_type.h>
#include "cvi_debug.h"
#include "cvi_comm_3a.h"
#include "cvi_comm_isp.h"
#include "cvi_ae_comm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef struct combo_dev_attr_s SNS_COMBO_DEV_ATTR_S;
typedef struct mclk_pll_s SNS_MCLK_ATTR_S;

typedef struct _SNS_EXP_MAX_S {
	CVI_U16 manual;
	CVI_U32 ratio[3];
	CVI_U32 IntTimeMax[4];
	CVI_U32 IntTimeMin[4];
	CVI_U32 LFMaxIntTime[4];
} SNS_EXP_MAX_S;

typedef struct _SNS_GAIN_S {
	CVI_U32 gain;
	CVI_U32 gainDb;
} SNS_GAIN_S;

typedef struct _SNS_ATTR_S {
	CVI_U16	u16Min;
	CVI_U16 u16Max;
	CVI_U16 u16Def;
	CVI_U16 u16Step;
} SNS_ATTR_S;

typedef struct _SNS_ATTR_LARGE_S {
	CVI_U32	u32Min;
	CVI_U32 u32Max;
	CVI_U32 u32Def;
	CVI_U32 u32Step;
} SNS_ATTR_LARGE_S;

typedef struct _ISP_SNS_STATE_S {
	CVI_BOOL     bInit;                  /* CVI_TRUE: Sensor init */
	CVI_BOOL     bSyncInit;              /* CVI_TRUE: Sync Reg init */
	CVI_U8       u8ImgMode;
	CVI_U8       u8Hdr;               /* CVI_TRUE: HDR enbale */
	WDR_MODE_E  enWDRMode;

	ISP_SNS_SYNC_INFO_S astSyncInfo[2]; /* [0]: Sensor reg info of cur-frame; [1]: Sensor reg info of pre-frame ; */

	CVI_U32      au32FL[2];              /* [0]: FullLines of cur-frame; [1]: Pre FullLines of pre-frame */
	CVI_U32      u32FLStd;               /* FullLines std */
	CVI_U32      au32WDRIntTime[4];
} ISP_SNS_STATE_S;

typedef enum _ISP_SNS_MIRRORFLIP_TYPE_E {
	ISP_SNS_NORMAL      = 0,
	ISP_SNS_MIRROR      = 1,
	ISP_SNS_FLIP        = 2,
	ISP_SNS_MIRROR_FLIP = 3,
	ISP_SNS_BUTT
} ISP_SNS_MIRRORFLIP_TYPE_E;

typedef enum _ISP_SNS_L2S_MODE_E {
	SNS_L2S_MODE_AUTO = 0,	/* sensor l2s distance varies by the inttime of sef. */
	SNS_L2S_MODE_FIX,	/* sensor l2s distance is fixed. */
} ISP_SNS_INTTIME_MODE_E;

typedef struct _MCLK_ATTR_S {
	CVI_U8 u8Mclk;
	CVI_BOOL bMclkEn;
} MCLK_ATTR_S;

typedef struct _RX_INIT_ATTR_S {
	CVI_U32 MipiDev;
	CVI_S16 as16LaneId[5];
	CVI_S8  as8PNSwap[5];
	MCLK_ATTR_S stMclkAttr;
} RX_INIT_ATTR_S;

typedef enum _SNS_BDG_MUX_MODE_E {
	SNS_BDG_MUX_NONE = 0,	/* sensor bridge mux is disabled */
	SNS_BDG_MUX_2,		/* sensor bridge mux 2 input */
	SNS_BDG_MUX_3,		/* sensor bridge mux 3 input */
	SNS_BDG_MUX_4,		/* sensor bridge mux 4 input */
} SNS_BDG_MUX_MODE_E;

typedef struct _ISP_INIT_ATTR_S {
	CVI_U32 u32ExpTime;
	CVI_U32 u32AGain;
	CVI_U32 u32DGain;
	CVI_U32 u32ISPDGain;
	CVI_U32 u32Exposure;
	CVI_U32 u32LinesPer500ms;
	CVI_U32 u32PirisFNO;
	CVI_U16 u16WBRgain;
	CVI_U16 u16WBGgain;
	CVI_U16 u16WBBgain;
	CVI_U16 u16SampleRgain;
	CVI_U16 u16SampleBgain;
	CVI_U16 u16UseHwSync;
	ISP_SNS_GAIN_MODE_E enGainMode;
	ISP_SNS_INTTIME_MODE_E enL2SMode;
	SNS_BDG_MUX_MODE_E enSnsBdgMuxMode;
} ISP_INIT_ATTR_S;

typedef struct _ISP_SNS_OBJ_S {
	CVI_S32 (*pfnRegisterCallback)(VI_PIPE ViPipe, ALG_LIB_S *, ALG_LIB_S *);
	CVI_S32 (*pfnUnRegisterCallback)(VI_PIPE ViPipe, ALG_LIB_S *, ALG_LIB_S *);
	CVI_S32 (*pfnSetBusInfo)(VI_PIPE ViPipe, ISP_SNS_COMMBUS_U unSNSBusInfo);
	CVI_VOID (*pfnStandby)(VI_PIPE ViPipe);
	CVI_VOID (*pfnRestart)(VI_PIPE ViPipe);
	CVI_VOID (*pfnMirrorFlip)(VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eSnsMirrorFlip);
	CVI_S32 (*pfnWriteReg)(VI_PIPE ViPipe, CVI_S32 s32Addr, CVI_S32 s32Data);
	CVI_S32 (*pfnReadReg)(VI_PIPE ViPipe, CVI_S32 s32Addr);
	CVI_S32 (*pfnSetInit)(VI_PIPE ViPipe, ISP_INIT_ATTR_S *);
	CVI_S32 (*pfnPatchRxAttr)(RX_INIT_ATTR_S *);
	CVI_VOID (*pfnPatchI2cAddr)(CVI_S32 s32I2cAddr);
	CVI_S32 (*pfnGetRxAttr)(VI_PIPE ViPipe, SNS_COMBO_DEV_ATTR_S *);
	CVI_S32 (*pfnExpSensorCb)(ISP_SENSOR_EXP_FUNC_S *);
	CVI_S32 (*pfnExpAeCb)(AE_SENSOR_EXP_FUNC_S *);
	CVI_S32 (*pfnSnsProbe)(VI_PIPE ViPipe);
} ISP_SNS_OBJ_S;

typedef struct _SNS_AHD_OBJ_S {
    CVI_S32 (*pfnAhdInit)(VI_PIPE ViPipe, bool isFirstInit);
    CVI_S32 (*pfnAhdDeinit)(VI_PIPE ViPipe);
    CVI_S32 (*pfnGetAhdMode)(VI_PIPE ViPipe);
    CVI_S32 (*pfnSetAhdMode)(VI_PIPE ViPipe, CVI_S32 astAhdMode);
    CVI_S32 (*pfnSetAhdBusInfo)(VI_PIPE ViPipe, CVI_S32 astI2cDev);
} SNS_AHD_OBJ_S;

#define CMOS_CHECK_POINTER(ptr)\
	do {\
		if (ptr == CVI_NULL) {\
			syslog(LOG_ERR, "Null Pointer!\n");\
			return CVI_ERR_VI_INVALID_NULL_PTR;\
		} \
	} while (0)

#define CMOS_CHECK_POINTER_VOID(ptr)\
	do {\
		if (ptr == CVI_NULL) {\
			syslog(LOG_ERR, "Null Pointer!\n");\
			return;\
		} \
	} while (0)

#define SENSOR_FREE(ptr)\
	do {\
		if (ptr != CVI_NULL) {\
			free(ptr);\
			ptr = CVI_NULL;\
		} \
	} while (0)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* __CVI_SNS_CTRL_H__ */
