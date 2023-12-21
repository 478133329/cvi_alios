#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <pthread.h>
#include <inttypes.h>
#include <aos/aos.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include <cvi_base.h>
#include <cvi_mw_base.h>
#include <aos/kernel.h>

#include "cvi_buffer.h"
#include "cvi_vpss.h"
#include "cvi_sys.h"
#include "cvi_gdc.h"
#include "cvi_vo.h"
#include "cvi_vb.h"
#include "gdc_mesh.h"
#include "vpss_ioctl.h"
#include "vpss_ctx.h"
#include "vpss_bin.h"

#define CHECK_VPSS_GRP_VALID(grp) do {										\
		if ((grp >= VPSS_MAX_GRP_NUM) || (grp < 0)) {							\
			CVI_TRACE_VPSS(CVI_DBG_ERR, "VpssGrp(%d) exceeds Max(%d)\n", grp, VPSS_MAX_GRP_NUM);	\
			return CVI_ERR_VPSS_ILLEGAL_PARAM;							\
		}												\
	} while (0)

#define CHECK_VPSS_GRP_CREATED(grp) do {								\
		if (!vpssCtx[grp] || !vpssCtx[grp]->isCreated) {							\
			CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) isn't created yet.\n", grp);		\
			return CVI_ERR_VPSS_UNEXIST;							\
		}											\
	} while (0)

#define CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn) do {								\
		if (CVI_SYS_GetVPSSMode() == VPSS_MODE_SINGLE) {						\
			if ((VpssChn >= VPSS_MAX_CHN_NUM) || (VpssChn < 0)) {					\
				CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) invalid for VPSS-Single.\n",	\
						VpssGrp, VpssChn);						\
				return CVI_ERR_VPSS_ILLEGAL_PARAM;						\
			}											\
		} else {											\
			if ((VpssChn >= vpssCtx[VpssGrp]->chnNum) || (VpssChn < 0)) {				\
				CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) invalid for VPSS-Dual(%d).\n"	\
						, VpssGrp, VpssChn, vpssCtx[VpssGrp]->stGrpAttr.u8VpssDev);	\
				return CVI_ERR_VPSS_ILLEGAL_PARAM;						\
			}											\
		}												\
	} while (0)

#define CHECK_YUV_PARAM(fmt, w, h) do {										\
		if (fmt == PIXEL_FORMAT_YUV_PLANAR_422) {							\
			if (w & 0x01) {										\
				CVI_TRACE_VPSS(CVI_DBG_ERR, "YUV_422 width(%d) should be even.\n", w);		\
				return CVI_ERR_VPSS_ILLEGAL_PARAM;						\
			}											\
		} else if ((fmt == PIXEL_FORMAT_YUV_PLANAR_420)							\
			|| (fmt == PIXEL_FORMAT_NV12)								\
			|| (fmt == PIXEL_FORMAT_NV21)) {							\
			if (w & 0x01) {										\
				CVI_TRACE_VPSS(CVI_DBG_ERR, "YUV_420 width(%d) should be even.\n", w);		\
				return CVI_ERR_VPSS_ILLEGAL_PARAM;						\
			}											\
			if (h & 0x01) {										\
				CVI_TRACE_VPSS(CVI_DBG_ERR, "YUV_420 height(%d) should be even.\n", h);		\
				return CVI_ERR_VPSS_ILLEGAL_PARAM;						\
			}											\
		}												\
	} while (0)

#define CHECK_VPSS_GRP_FMT(grp, fmt) do {									\
		if (!VPSS_GRP_SUPPORT_FMT(fmt)) {								\
			CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) enPixelFormat(%d) unsupported\n"			\
				, grp, fmt);									\
			return CVI_ERR_VPSS_ILLEGAL_PARAM;							\
		}												\
	} while (0)

#define CHECK_VPSS_CHN_FMT(grp, chn, fmt) do {									\
		if (!VPSS_CHN_SUPPORT_FMT(fmt)) {								\
			CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) enPixelFormat(%d) unsupported\n"		\
				, grp, chn, fmt);								\
			return CVI_ERR_VPSS_ILLEGAL_PARAM;							\
		}												\
	} while (0)

#define CHECK_VPSS_GDC_FMT(grp, chn, fmt)									\
	do {													\
		if (!GDC_SUPPORT_FMT(fmt)) {		\
			CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) invalid PixFormat(%d) for GDC.\n"		\
					, grp, chn, (fmt));							\
			return CVI_ERR_VPSS_ILLEGAL_PARAM;							\
		}												\
	} while (0)

#define FRC_INVALID(ctx, VpssChn)	\
	(ctx->stChnCfgs[VpssChn].stChnAttr.stFrameRate.s32DstFrameRate <= 0 ||		\
		ctx->stChnCfgs[VpssChn].stChnAttr.stFrameRate.s32SrcFrameRate <= 0 ||		\
		ctx->stChnCfgs[VpssChn].stChnAttr.stFrameRate.s32DstFrameRate >=		\
		ctx->stChnCfgs[VpssChn].stChnAttr.stFrameRate.s32SrcFrameRate)

struct _vpss_gdc_cb_param {
	MMF_CHN_S chn;
	enum GDC_USAGE usage;
};

struct _vpss_rgnex_job_info {
	MMF_CHN_S chn;
	struct cvi_rgn_ex_cfg rgn_ex_cfg;
	PIXEL_FORMAT_E enPixelFormat;
	CVI_U32 bytesperline[2];
};

struct cvi_stitch_ctx {
	CVI_BOOL bUse;
	CVI_BOOL bStart;
	VPSS_GRP VpssGrp;
	VB_POOL VbPool;
	CVI_STITCH_ATTR_S stStitchAttr;
	pthread_t thread;
	pthread_mutex_t lock;
	aos_timer_t timer;
	aos_event_t wait;
};

static struct cvi_stitch_ctx s_StitchCtx[VPSS_MAX_GRP_NUM];

static struct cvi_vpss_ctx *vpssCtx[VPSS_MAX_GRP_NUM] = { [0 ... VPSS_MAX_GRP_NUM - 1] = NULL};

struct cvi_gdc_mesh mesh[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];

static PROC_AMP_CTRL_S procamp_ctrls[PROC_AMP_MAX] = {
	{ .minimum = 0, .maximum = 100, .step = 1, .default_value = 50 },
	{ .minimum = 0, .maximum = 100, .step = 1, .default_value = 50 },
	{ .minimum = 0, .maximum = 100, .step = 1, .default_value = 50 },
	{ .minimum = 0, .maximum = 100, .step = 1, .default_value = 50 },
};

struct cvi_vpss_ctx **vpss_get_ctx(void)
{
	return vpssCtx;
}

/**************************************************************************
 *   Job related APIs.
 **************************************************************************/
void _vpss_GrpParamInit(VPSS_GRP VpssGrp)
{
	PROC_AMP_CTRL_S ctrl;

	for (CVI_U8 i = PROC_AMP_BRIGHTNESS; i < PROC_AMP_MAX; ++i) {
		CVI_VPSS_GetGrpProcAmpCtrl(VpssGrp, i, &ctrl);
		vpssCtx[VpssGrp]->proc_amp[i] = ctrl.default_value;
	}
	vpssCtx[VpssGrp]->scene = 0xff;
}

static CVI_S32 _vpss_update_rotation_mesh(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, ROTATION_E enRotation)
{

	struct cvi_gdc_mesh *pmesh = &mesh[VpssGrp][VpssChn];
	struct vpss_chn_rot_cfg cfg;

	// TODO: dummy settings
	pmesh->paddr = DEFAULT_MESH_PADDR;
	vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation = enRotation;

	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.enRotation = enRotation;
	return vpss_set_chn_rotation(&cfg);

	return CVI_SUCCESS;
}

static CVI_S32 _vpss_update_ldc_mesh(VPSS_GRP VpssGrp, VPSS_CHN VpssChn,
	const VPSS_LDC_ATTR_S *pstLDCAttr, ROTATION_E enRotation)
{
	CVI_U64 paddr, paddr_old;
	CVI_VOID *vaddr, *vaddr_old;
	struct cvi_gdc_mesh *pmesh = &mesh[VpssGrp][VpssChn];
	CVI_S32 s32Ret;

	if (!pstLDCAttr->bEnable) {
		pthread_mutex_lock(&pmesh->lock);
		vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stLDCAttr = *pstLDCAttr;
		pthread_mutex_unlock(&pmesh->lock);

		if (vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation != ROTATION_0)
			return _vpss_update_rotation_mesh(VpssGrp, VpssChn,
				vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation);
		else {
			struct vpss_chn_ldc_cfg cfg;

			cfg.VpssGrp = VpssGrp;
			cfg.VpssChn = VpssChn;
			cfg.enRotation = enRotation;
			cfg.stLDCAttr = *pstLDCAttr;
			cfg.meshHandle = paddr;
			return vpss_set_chn_ldc(&cfg);
		}
	}

	s32Ret = CVI_GDC_GenLDCMesh(ALIGN(vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.u32Width, DEFAULT_ALIGN),
					ALIGN(vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.u32Height, DEFAULT_ALIGN),
					&pstLDCAttr->stAttr, "vpss_mesh", &paddr, &vaddr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) gen mesh fail\n",
				VpssGrp, VpssChn);
		return s32Ret;
	}

	pthread_mutex_lock(&pmesh->lock);
	if (pmesh->paddr) {
		paddr_old = pmesh->paddr;
		vaddr_old = pmesh->vaddr;
	} else {
		paddr_old = 0;
		vaddr_old = NULL;
	}
	pmesh->paddr = paddr;
	pmesh->vaddr = vaddr;
	vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stLDCAttr = *pstLDCAttr;
	vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation = enRotation;
	pthread_mutex_unlock(&pmesh->lock);

	if (paddr_old && (paddr_old != DEFAULT_MESH_PADDR))
		CVI_SYS_IonFree(paddr_old, vaddr_old);

	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "Grp(%d) Chn(%d) mesh base(%#"PRIx64") vaddr(%p)\n"
		, VpssGrp, VpssChn, paddr, vaddr);

//#if defined(__CV181X__)


	struct vpss_chn_ldc_cfg cfg;

	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.enRotation = enRotation;
	cfg.stLDCAttr = *pstLDCAttr;
	cfg.meshHandle = paddr;
	return vpss_set_chn_ldc(&cfg);
//#endif

}

CVI_S32 CVI_VPSS_Suspend(void)
{
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "+\n");
	vpss_suspend();
	CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_Suspend called\n");
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "-\n");
	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_Resume(void)
{
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "+\n");
	vpss_resume();
	CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_Resume called\n");
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "-\n");
	return CVI_SUCCESS;
}

/**************************************************************************
 *   Public APIs.
 **************************************************************************/
CVI_S32 CVI_VPSS_CreateGrp(VPSS_GRP VpssGrp, const VPSS_GRP_ATTR_S *pstGrpAttr)
{
	struct vpss_crt_grp_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	if (vpssCtx[VpssGrp]) {
		CVI_TRACE_VPSS(CVI_DBG_WARN, "Grp(%d) is occupied\n", VpssGrp);
		return CVI_ERR_VPSS_EXIST;
	}

	cfg.VpssGrp = VpssGrp;
	memcpy(&cfg.stGrpAttr, pstGrpAttr, sizeof(cfg.stGrpAttr));

	if (vpss_create_grp(&cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) VpssDev(%d) create group fail\n",
				VpssGrp, pstGrpAttr->u8VpssDev);
		return CVI_FAILURE;
	}

	vpssCtx[VpssGrp] = calloc(sizeof(struct cvi_vpss_ctx), 1);
	if (!vpssCtx[VpssGrp]) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "VpssCtx malloc failed\n");
		vpss_destroy_grp(VpssGrp);
		return CVI_ERR_VPSS_NOMEM;
	}

	// for chn rotation, ldc mesh gen
	vpssCtx[VpssGrp]->isCreated = CVI_TRUE;
	vpssCtx[VpssGrp]->chnNum = (CVI_SYS_GetVPSSMode() == VPSS_MODE_SINGLE) ?
		VPSS_MAX_CHN_NUM : (pstGrpAttr->u8VpssDev == 0) ? 1 : VPSS_MAX_CHN_NUM - 1;
	_vpss_GrpParamInit(VpssGrp);

	for (CVI_U8 i = 0; i < vpssCtx[VpssGrp]->chnNum; ++i) {
		memset(&vpssCtx[VpssGrp]->stChnCfgs[i], 0, sizeof(vpssCtx[VpssGrp]->stChnCfgs[i]));
		pthread_mutex_init(&mesh[VpssGrp][i].lock, NULL);
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_DestroyGrp(VPSS_GRP VpssGrp)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	if (!vpssCtx[VpssGrp]) {
		CVI_TRACE_VPSS(CVI_DBG_WARN, "Grp(%d) has been destroyed\n", VpssGrp);
		return CVI_SUCCESS;
	}

	if (vpss_destroy_grp(VpssGrp) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) destroy group fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	for (CVI_U8 i = 0; i < vpssCtx[VpssGrp]->chnNum; ++i) {
		pthread_mutex_destroy(&mesh[VpssGrp][i].lock);
	}

	vpssCtx[VpssGrp]->isCreated = CVI_FALSE;
	free(vpssCtx[VpssGrp]);
	vpssCtx[VpssGrp] = NULL;

	return CVI_SUCCESS;
}

VPSS_GRP CVI_VPSS_GetAvailableGrp(void)
{
	VPSS_GRP grp = VPSS_INVALID_GRP;

	vpss_get_available_grp(&grp);

	return grp;
}

CVI_S32 CVI_VPSS_StartGrp(VPSS_GRP VpssGrp)
{
	struct vpss_str_grp_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	if (vpssCtx[VpssGrp]->isStarted) {
		CVI_TRACE_VPSS(CVI_DBG_WARN, "Grp(%d) already started.\n", VpssGrp);
		return CVI_SUCCESS;
	}

	cfg.VpssGrp = VpssGrp;
	if (vpss_start_grp(&cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) start group fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}
	vpssCtx[VpssGrp]->isStarted = CVI_TRUE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_StopGrp(VPSS_GRP VpssGrp)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	if (!vpssCtx[VpssGrp]->isStarted) {
		CVI_TRACE_VPSS(CVI_DBG_WARN, "Grp(%d) has been stopped\n", VpssGrp);
		return CVI_SUCCESS;
	}

	if (vpss_stop_grp(VpssGrp) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) stop group fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	vpssCtx[VpssGrp]->isStarted = CVI_FALSE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_ResetGrp(VPSS_GRP VpssGrp)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (vpss_reset_grp(VpssGrp) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) reset group fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}
	vpssCtx[VpssGrp]->isStarted = CVI_FALSE;
	_vpss_GrpParamInit(VpssGrp);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetGrpAttr(VPSS_GRP VpssGrp, VPSS_GRP_ATTR_S *pstGrpAttr)
{
	struct vpss_grp_attr cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;

	if (vpss_get_grp_attr(&cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) get grp attr fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	memcpy(pstGrpAttr, &cfg.stGrpAttr, sizeof(*pstGrpAttr));

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetGrpAttr(VPSS_GRP VpssGrp, const VPSS_GRP_ATTR_S *pstGrpAttr)
{
	struct vpss_grp_attr cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	memcpy(&cfg.stGrpAttr, pstGrpAttr, sizeof(cfg.stGrpAttr));

	if (vpss_set_grp_attr(&cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) set grp attr fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	vpssCtx[VpssGrp]->chnNum = (CVI_SYS_GetVPSSMode() == VPSS_MODE_SINGLE) ?
			VPSS_MAX_CHN_NUM : (pstGrpAttr->u8VpssDev == 0) ? 1 : VPSS_MAX_CHN_NUM - 1;
	CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) VpssDev(%d) u32MaxW(%d) u32MaxH(%d) PixelFmt(%d)\n",
		VpssGrp, pstGrpAttr->u8VpssDev, pstGrpAttr->u32MaxW, pstGrpAttr->u32MaxH, pstGrpAttr->enPixelFormat);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetGrpProcAmpCtrl(VPSS_GRP VpssGrp, PROC_AMP_E type, PROC_AMP_CTRL_S *ctrl)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, ctrl);
	CHECK_VPSS_GRP_VALID(VpssGrp);

	if (type >= PROC_AMP_MAX) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) ProcAmp type(%d) invalid.\n", VpssGrp, type);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	*ctrl = procamp_ctrls[type];
	return CVI_SUCCESS;
}

static void _vpss_proamp_2_csc(const CVI_S32 proc_amp[PROC_AMP_MAX], struct vpss_grp_csc_cfg *csc_cfg)
{
	// for grp proc-amp.
	float h = (float)(proc_amp[PROC_AMP_HUE] - 50) * PI / 360;
	float b_off = (proc_amp[PROC_AMP_BRIGHTNESS] - 50) * 2.56;
	float C_gain = 1 + (proc_amp[PROC_AMP_CONTRAST] - 50) * 0.02;
	float S = 1 + (proc_amp[PROC_AMP_SATURATION] - 50) * 0.02;
	float A = cos(h) * C_gain * S;
	float B = sin(h) * C_gain * S;
	float C_diff, c_off, tmp;
	CVI_U8 sub_0_l, add_0_l, add_1_l, add_2_l;

	if (proc_amp[PROC_AMP_CONTRAST] > 50)
		C_diff = 256 / C_gain;
	else
		C_diff = 256 * C_gain;
	c_off = 128 - (C_diff/2);

	if (b_off < 0) {
		sub_0_l = abs(proc_amp[PROC_AMP_BRIGHTNESS] - 50) * 2.56;
		add_0_l = 0;
		add_1_l = 0;
		add_2_l = 0;
	} else {
		sub_0_l = 0;
		if ((C_gain * b_off) > 255) {
			add_0_l = 255;
		} else {
			add_0_l = C_gain * b_off;
		}
		add_1_l = add_0_l;
		add_2_l = add_0_l;
	}

	if (proc_amp[PROC_AMP_CONTRAST] > 50) {
		csc_cfg->sub[0] = sub_0_l + c_off;
		csc_cfg->add[0] = add_0_l;
		csc_cfg->add[1] = add_1_l;
		csc_cfg->add[2] = add_2_l;
	} else {
		csc_cfg->sub[0] = sub_0_l;
		csc_cfg->add[0] = add_0_l + c_off;
		csc_cfg->add[1] = add_1_l + c_off;
		csc_cfg->add[2] = add_2_l + c_off;
	}
	csc_cfg->sub[1] = 128;
	csc_cfg->sub[2] = 128;

	csc_cfg->coef[0][0] = C_gain * BIT(10);
	tmp = B * -1.402;
	csc_cfg->coef[0][1] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	tmp = A * 1.402;
	csc_cfg->coef[0][2] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	csc_cfg->coef[1][0] = C_gain * BIT(10);
	tmp = A * -0.344 + B * 0.714;
	csc_cfg->coef[1][1] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	tmp = B * -0.344 + A * -0.714;
	csc_cfg->coef[1][2] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	csc_cfg->coef[2][0] = C_gain * BIT(10);
	tmp = A * 1.772;
	csc_cfg->coef[2][1] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	tmp = B * 1.772;
	csc_cfg->coef[2][2] = (tmp >= 0) ? tmp * BIT(10) : (CVI_U16)((-tmp) * BIT(10)) | BIT(13);
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "coef[0][0]: %#4x coef[0][1]: %#4x coef[0][2]: %#4x\n"
		, csc_cfg->coef[0][0], csc_cfg->coef[0][1]
		, csc_cfg->coef[0][2]);
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "coef[1][0]: %#4x coef[1][1]: %#4x coef[1][2]: %#4x\n"
		, csc_cfg->coef[1][0], csc_cfg->coef[1][1]
		, csc_cfg->coef[1][2]);
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "coef[2][0]: %#4x coef[2][1]: %#4x coef[2][2]: %#4x\n"
		, csc_cfg->coef[2][0], csc_cfg->coef[2][1]
		, csc_cfg->coef[2][2]);
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "sub[0]: %3d sub[1]: %3d sub[2]: %3d\n"
		, csc_cfg->sub[0], csc_cfg->sub[1], csc_cfg->sub[2]);
	CVI_TRACE_VPSS(CVI_DBG_DEBUG, "add[0]: %3d add[1]: %3d add[2]: %3d\n"
		, csc_cfg->add[0], csc_cfg->add[1], csc_cfg->add[2]);
}

CVI_S32 CVI_VPSS_GetGrpProcAmp(VPSS_GRP VpssGrp, PROC_AMP_E type, CVI_S32 *value)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, value);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (type >= PROC_AMP_MAX) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) ProcAmp type(%d) invalid.\n", VpssGrp, type);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	*value = vpssCtx[VpssGrp]->proc_amp[type];
	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetAllProcAmp(struct vpss_all_proc_amp_cfg *cfg)
{
	CVI_U8 i, j;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, cfg);

	for (i = 0; i < VPSS_MAX_GRP_NUM; i++) {
		if (vpssCtx[i]) {
			for (j = PROC_AMP_BRIGHTNESS; j < PROC_AMP_MAX; ++j)
				cfg->proc_amp[i][j] = vpssCtx[i]->proc_amp[j];
		}
	}

	return CVI_SUCCESS;
}


CVI_S32 CVI_VPSS_SetGrpProcAmp(VPSS_GRP VpssGrp, PROC_AMP_E type, const CVI_S32 value)
{
	PROC_AMP_CTRL_S ctrl;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (type >= PROC_AMP_MAX) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) ProcAmp type(%d) invalid.\n", VpssGrp, type);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	CVI_VPSS_GetGrpProcAmpCtrl(VpssGrp, type, &ctrl);
	if ((value > ctrl.maximum) || (value < ctrl.minimum)) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) new value(%d) out of range(%d ~ %d).\n"
			, VpssGrp, value, ctrl.minimum, ctrl.maximum);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	vpssCtx[VpssGrp]->proc_amp[type] = value;
//#if defined(__CV181X__)

	struct vpss_grp_csc_cfg csc_cfg;

	memset(&csc_cfg, 0, sizeof(csc_cfg));
	csc_cfg.VpssGrp = VpssGrp;
	_vpss_proamp_2_csc(vpssCtx[VpssGrp]->proc_amp, &csc_cfg);

	if (vpss_set_grp_csc(&csc_cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) set group csc fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}
//#endif

	return CVI_SUCCESS;
}

static CVI_VOID _vpss_check_normalize(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CHN_ATTR_S *pstChnAttr)
{
	if (pstChnAttr->stNormalize.bEnable) {
		for (CVI_U8 i = 0; i < 3; ++i) {
			if (pstChnAttr->stNormalize.factor[i] >= 1.0f) {
				vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.stNormalize.factor[i] = 1.0f - 1.0f/8192;
				CVI_TRACE_VPSS(CVI_DBG_WARN, "factor%d replaced with max value 8191/8192\n", i);
			}
			if (pstChnAttr->stNormalize.factor[i] < (1.0f/8192)) {
				vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.stNormalize.factor[i] = (1.0f/8192);
				CVI_TRACE_VPSS(CVI_DBG_WARN, "factor%d replaced with min value 1/8192\n", i);
			}
			if (pstChnAttr->stNormalize.mean[i] > 255.0f) {
				vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.stNormalize.mean[i] = 255.0f;
				CVI_TRACE_VPSS(CVI_DBG_WARN, "mean%d replaced with max value 255\n", i);
			}
			if (pstChnAttr->stNormalize.mean[i] < 0) {
				vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.stNormalize.mean[i] = 0;
				CVI_TRACE_VPSS(CVI_DBG_WARN, "mean%d replaced with min value 0\n", i);
			}
		}
	}
}

/* Chn Settings */
CVI_S32 CVI_VPSS_SetChnAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CHN_ATTR_S *pstChnAttr)
{
	struct vpss_chn_attr attr = {.VpssGrp = VpssGrp, .VpssChn = VpssChn};

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstChnAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	// Handle float poing in user space
	_vpss_check_normalize(VpssGrp, VpssChn, pstChnAttr);

	// for chn rotation, mesh gen
	vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr = *pstChnAttr;
	memcpy(&attr.stChnAttr,  pstChnAttr, sizeof(attr.stChnAttr));

	if (vpss_set_chn_attr(&attr) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) set chn attr fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_CHN_ATTR_S *pstChnAttr)
{

	struct vpss_chn_attr attr = {.VpssGrp = VpssGrp, .VpssChn = VpssChn};

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstChnAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	if (vpss_get_chn_attr(&attr) != CVI_SUCCESS)
		return CVI_FAILURE;

	*pstChnAttr = attr.stChnAttr;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_EnableChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
	struct vpss_en_chn_cfg cfg = {.VpssGrp = VpssGrp, .VpssChn = VpssChn};

	// for chn rotation, mesh gen
	struct VPSS_CHN_CFG *chn_cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	chn_cfg = &vpssCtx[VpssGrp]->stChnCfgs[VpssChn];
	chn_cfg->isEnabled = CVI_TRUE;

	if (vpss_enable_chn(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_DisableChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
	struct vpss_en_chn_cfg cfg = {.VpssGrp = VpssGrp, .VpssChn = VpssChn};
	struct cvi_gdc_mesh *pmesh = &mesh[VpssGrp][VpssChn];

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	if (vpss_disable_chn(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	if (pmesh->paddr && (pmesh->paddr != DEFAULT_MESH_PADDR)) {
		CVI_SYS_IonFree(pmesh->paddr, pmesh->vaddr);
		pmesh->paddr = 0;
		pmesh->vaddr = NULL;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetChnCrop(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CROP_INFO_S *pstCropInfo)
{

	struct vpss_chn_crop_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstCropInfo);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.stCropInfo = *pstCropInfo;

	if (vpss_set_chn_crop(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnCrop(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_CROP_INFO_S *pstCropInfo)
{
	struct vpss_chn_crop_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstCropInfo);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_chn_crop(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*pstCropInfo = cfg.stCropInfo;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_ShowChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
	struct vpss_en_chn_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_show_chn(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) Chn(%d)\n", VpssGrp, VpssChn);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_HideChn(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{

	struct vpss_en_chn_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_hide_chn(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) Chn(%d)\n", VpssGrp, VpssChn);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetGrpCrop(VPSS_GRP VpssGrp, VPSS_CROP_INFO_S *pstCropInfo)
{
	struct vpss_grp_crop_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstCropInfo);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;

	if (vpss_get_grp_crop(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*pstCropInfo = cfg.stCropInfo;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetGrpCrop(VPSS_GRP VpssGrp, const VPSS_CROP_INFO_S *pstCropInfo)
{

	struct vpss_grp_crop_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstCropInfo);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.stCropInfo = *pstCropInfo;

	if (vpss_set_grp_crop(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

//TBD
CVI_S32 CVI_VPSS_GetGrpFrame(VPSS_GRP VpssGrp, VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVideoFrame);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	CVI_TRACE_VPSS(CVI_DBG_ERR, "Not support get group frame\n", VpssGrp);
	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_ReleaseGrpFrame(VPSS_GRP VpssGrp, const VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVideoFrame);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SendFrame(VPSS_GRP VpssGrp, const VIDEO_FRAME_INFO_S *pstVideoFrame, CVI_S32 s32MilliSec)
{
	struct vpss_snd_frm_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVideoFrame);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	cfg.VpssGrp = VpssGrp;
	memcpy(&cfg.stVideoFrame, pstVideoFrame, sizeof(cfg.stVideoFrame));
	cfg.s32MilliSec = s32MilliSec;

	if (vpss_send_frame(&cfg) != CVI_SUCCESS)
		return CVI_ERR_VPSS_BUSY;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SendChnFrame(VPSS_GRP VpssGrp, VPSS_CHN VpssChn
	, const VIDEO_FRAME_INFO_S *pstVideoFrame, CVI_S32 s32MilliSec)
{
	struct vpss_chn_frm_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVideoFrame);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	memcpy(&cfg.stVideoFrame, pstVideoFrame, sizeof(cfg.stVideoFrame));
	cfg.s32MilliSec = s32MilliSec;

	if (vpss_send_chn_frame(&cfg) != CVI_SUCCESS)
		return CVI_ERR_VPSS_BUSY;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnFrame(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VIDEO_FRAME_INFO_S *pstFrameInfo,
				CVI_S32 s32MilliSec)
{
	struct vpss_chn_frm_cfg cfg;
	CVI_S32 ret;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstFrameInfo);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.s32MilliSec = s32MilliSec;

	ret = vpss_get_chn_frame(&cfg);
	if (ret == CVI_SUCCESS)
		memcpy(pstFrameInfo, &cfg.stVideoFrame, sizeof(*pstFrameInfo));

	return ret;
}

CVI_S32 CVI_VPSS_ReleaseChnFrame(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	struct vpss_chn_frm_cfg cfg;
	CVI_S32 ret;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVideoFrame);
	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	memcpy(&cfg.stVideoFrame, pstVideoFrame, sizeof(cfg.stVideoFrame));

	ret = vpss_release_chn_frame(&cfg);
	if (ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) release chn frame fail\n", VpssGrp, VpssChn);
		return ret;
	}

	return CVI_SUCCESS;
}


CVI_S32 CVI_VPSS_SetModParam(const VPSS_MOD_PARAM_S *pstModParam)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstModParam);

	//todo: how to use vpssmod param...
	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetModParam(VPSS_MOD_PARAM_S *pstModParam)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstModParam);

	pstModParam->u32VpssSplitNodeNum = 1;
	pstModParam->u32VpssVbSource = 0; //vb from common vb pool
	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetChnRotation(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, ROTATION_E enRotation)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);
	CHECK_VPSS_GDC_FMT(VpssGrp, VpssChn, vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.enPixelFormat);

	struct cvi_gdc_mesh *pmesh = &mesh[VpssGrp][VpssChn];

	if (enRotation >= ROTATION_MAX) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) invalid rotation(%d).\n", VpssGrp, VpssChn, enRotation);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	} else if (enRotation == vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "rotation(%d) not changed.\n", enRotation);
		return CVI_SUCCESS;
	} else if (!vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stLDCAttr.bEnable && enRotation == ROTATION_0) {
		pthread_mutex_lock(&pmesh->lock);
		vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation = enRotation;
		pthread_mutex_unlock(&pmesh->lock);
		return CVI_SUCCESS;
	}

	if (vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stLDCAttr.bEnable)
		return _vpss_update_ldc_mesh(VpssGrp, VpssChn,
			&vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stLDCAttr, enRotation);
	else
		return _vpss_update_rotation_mesh(VpssGrp, VpssChn, enRotation);
}

CVI_S32 CVI_VPSS_GetChnRotation(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, ROTATION_E *penRotation)
{
	struct vpss_chn_rot_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_chn_rotation(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*penRotation = cfg.enRotation;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetChnAlign(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CVI_U32 u32Align)
{
	struct vpss_chn_align_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.u32Align = u32Align;

	if (vpss_set_chn_align(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnAlign(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CVI_U32 *pu32Align)
{
	struct vpss_chn_align_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pu32Align);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_chn_align(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*pu32Align = cfg.u32Align;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetChnScaleCoefLevel(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_SCALE_COEF_E enCoef)
{
	struct vpss_chn_coef_level_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.enCoef = enCoef;

	if (vpss_set_coef_level(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnScaleCoefLevel(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_SCALE_COEF_E *penCoef)
{
	struct vpss_chn_coef_level_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, penCoef);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_coef_level(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*penCoef = cfg.enCoef;

	return CVI_SUCCESS;
}

/* CVI_VPSS_SetChnYRatio: Modify the y ratio of chn output. Only work for yuv format.
 *
 * @param VpssGrp: The Vpss Grp to work.
 * @param VpssChn: The Vpss Chn to work.
 * @param YRatio: Output's Y will be sacled by this ratio.
 * @return: CVI_SUCCESS if OK.
 */
CVI_S32 CVI_VPSS_SetChnYRatio(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CVI_FLOAT YRatio)
{

	struct vpss_chn_yratio_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.YRatio = (CVI_U32)(YRatio * 100);

	if (vpss_set_chn_yratio(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetChnYRatio(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CVI_FLOAT *pYRatio)
{
	struct vpss_chn_yratio_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pYRatio);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_chn_yratio(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	*pYRatio = (1.0f * cfg.YRatio) / 100.0;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetChnLDCAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_LDC_ATTR_S *pstLDCAttr)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);
	CHECK_VPSS_GDC_FMT(VpssGrp, VpssChn, vpssCtx[VpssGrp]->stChnCfgs[VpssChn].stChnAttr.enPixelFormat);

	return _vpss_update_ldc_mesh(VpssGrp, VpssChn, pstLDCAttr, vpssCtx[VpssGrp]->stChnCfgs[VpssChn].enRotation);
}

CVI_S32 CVI_VPSS_GetChnLDCAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_LDC_ATTR_S *pstLDCAttr)
{

	struct vpss_chn_ldc_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstLDCAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;

	if (vpss_get_chn_ldc(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	memcpy(pstLDCAttr, &cfg.stLDCAttr, sizeof(*pstLDCAttr));

	return CVI_SUCCESS;
}

/* CVI_VPSS_SetGrpParamfromBin: Apply the settings of scene from bin
 *
 * @param VpssGrp: the vpss grp to apply
 * @param scene: the scene of settings stored in bin to use
 * @return: result of the API
 */
CVI_S32 CVI_VPSS_SetGrpParamfromBin(VPSS_GRP VpssGrp, CVI_U8 scene)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	VPSS_BIN_DATA *pBinData;

	if (scene > VPSS_MAX_GRP_NUM) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "scene(%d) is over max(%d)\n", scene, VPSS_MAX_GRP_NUM);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}
	if (get_loadbin_state()) {
		pBinData = get_vpssbindata_addr();

		memcpy(vpssCtx[VpssGrp]->proc_amp, pBinData[scene].proc_amp, sizeof(vpssCtx[VpssGrp]->proc_amp));
		vpssCtx[VpssGrp]->scene = scene;
		CVI_TRACE_VPSS(CVI_DBG_INFO, "PqBin is exist, vpss grp param use pqbin value in alios !!\n");

		struct vpss_grp_csc_cfg csc_cfg;

		memset(&csc_cfg, 0, sizeof(csc_cfg));
		csc_cfg.VpssGrp = VpssGrp;
		_vpss_proamp_2_csc(vpssCtx[VpssGrp]->proc_amp, &csc_cfg);

		if (vpss_set_grp_csc(&csc_cfg) != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) set group csc fail\n", VpssGrp);
			return CVI_FAILURE;
		}

	} else {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "PqBin is not find, vpss grp param use default !!\n");
	}

	return CVI_SUCCESS;
}

/* CVI_VPSS_SetGrpParamfromLinuxBin: Apply the settings of scene from bin in linux
 *
 * @param VpssGrp: the vpss grp to apply
 * @param scene: the scene of settings stored in bin to use
 * @return: result of the API
 */
CVI_S32 CVI_VPSS_SetGrpParam_fromLinuxBin(VPSS_GRP VpssGrp, struct vpss_proc_amp_cfg *bin_cfg)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	memcpy(vpssCtx[VpssGrp]->proc_amp, bin_cfg->proc_amp, sizeof(vpssCtx[VpssGrp]->proc_amp));

	if (bin_cfg->scene > VPSS_MAX_GRP_NUM) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "scene(%d) is over max(%d)\n", bin_cfg->scene, VPSS_MAX_GRP_NUM);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	vpssCtx[VpssGrp]->scene = bin_cfg->scene;
	CVI_TRACE_VPSS(CVI_DBG_INFO, "PqBin is exist, vpss grp param use pqbin value from Linux!\n");

	struct vpss_grp_csc_cfg csc_cfg;

	memset(&csc_cfg, 0, sizeof(csc_cfg));
	csc_cfg.VpssGrp = VpssGrp;
	_vpss_proamp_2_csc(vpssCtx[VpssGrp]->proc_amp, &csc_cfg);

	if (vpss_set_grp_csc(&csc_cfg) != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_INFO, "Grp(%d) set group csc fail\n",
				VpssGrp);
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetBinScene(VPSS_GRP VpssGrp, CVI_U8 *scene)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	*scene = vpssCtx[VpssGrp]->scene;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_AttachVbPool(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VB_POOL hVbPool)
{
	struct vpss_vb_pool_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.hVbPool = hVbPool;
	return vpss_attach_vbpool(&cfg);
}

CVI_S32 CVI_VPSS_DetachVbPool(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
	struct vpss_vb_pool_cfg cfg;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	memset(&cfg, 0, sizeof(cfg));
	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	return vpss_detach_vbpool(&cfg);
}

CVI_S32 CVI_VPSS_GetRegionLuma(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VIDEO_REGION_INFO_S *pstRegionInfo,
								CVI_U64 *pu64LumaData, CVI_S32 s32MilliSec)
{
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstRegionInfo);
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pu64LumaData);

	CVI_S32 ret = 0;
	VIDEO_FRAME_INFO_S stVideoFrame;
	CVI_U8 *pstVirAddr;
	SIZE_S stSize;
	CVI_U32 u32X, u32Y, u32Num;
	CVI_U32 u32MainStride;
	CVI_S32 s32StartX, s32StartY;

	s32StartX = pstRegionInfo->pstRegion->s32X;
	s32StartY = pstRegionInfo->pstRegion->s32Y;
	stSize.u32Width = pstRegionInfo->pstRegion->u32Width;
	stSize.u32Height = pstRegionInfo->pstRegion->u32Height;
	if ((s32StartX < 0) || (s32StartY < 0)) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "region info(%d %d %d %d) invalid.\n"
					, s32StartX, s32StartY, stSize.u32Width, stSize.u32Height);
		return CVI_ERR_VPSS_ILLEGAL_PARAM;
	}

	if (!vpssCtx[VpssGrp]->isStarted) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) not yet started.\n", VpssGrp);
		return CVI_ERR_VPSS_NOTREADY;
	}
	if (!vpssCtx[VpssGrp]->stChnCfgs[VpssChn].isEnabled) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) not yet enabled.\n", VpssGrp, VpssChn);
		return CVI_ERR_VPSS_NOTREADY;
	}

	ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stVideoFrame, s32MilliSec);
	if (ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) get buf fail\n", VpssGrp, VpssChn);
		return CVI_ERR_VPSS_BUF_EMPTY;
	}

	if (!IS_FMT_YUV(stVideoFrame.stVFrame.enPixelFormat)) {
		ret = CVI_ERR_VPSS_NOT_SUPPORT;
		CVI_TRACE_VPSS(CVI_DBG_ERR, "only support yuv-fmt(%d).\n"
					, stVideoFrame.stVFrame.enPixelFormat);
		goto release_blk;
	}

	pstVirAddr = (CVI_U8 *)stVideoFrame.stVFrame.u64PhyAddr[0];

	if (pstVirAddr == NULL) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "stVideoFrame error .\n");
		ret = CVI_FAILURE;
		goto release_blk;
	}

	u32MainStride = stVideoFrame.stVFrame.u32Stride[0];

	for (CVI_U32 i = 0; i < pstRegionInfo->u32RegionNum; ++i) {
		s32StartX = pstRegionInfo->pstRegion[i].s32X;
		s32StartY = pstRegionInfo->pstRegion[i].s32Y;
		stSize.u32Width = pstRegionInfo->pstRegion[i].u32Width;
		stSize.u32Height = pstRegionInfo->pstRegion[i].u32Height;
		if ((s32StartX < 0) || (s32StartY < 0)) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "region(%d) info(%d %d %d %d).\n"
						, i, s32StartX, s32StartY, stSize.u32Width, stSize.u32Height);
			ret = CVI_ERR_VPSS_ILLEGAL_PARAM;
			goto release_blk;
		}

		if ((s32StartX + stSize.u32Width > stVideoFrame.stVFrame.u32Width) ||
			(s32StartY + stSize.u32Height > stVideoFrame.stVFrame.u32Height) ||
			((CVI_U32)s32StartX >= stVideoFrame.stVFrame.u32Width) ||
			((CVI_U32)s32StartY >= stVideoFrame.stVFrame.u32Height) ||
			(stSize.u32Width > stVideoFrame.stVFrame.u32Width) ||
			(stSize.u32Height > stVideoFrame.stVFrame.u32Height)) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "region(%d) size(%d %d %d %d) out of range.\n"
						, i, s32StartX, s32StartY, stSize.u32Width, stSize.u32Height);
			ret = CVI_ERR_VPSS_ILLEGAL_PARAM;
			goto release_blk;
		}

		u32Num = 0;
		pu64LumaData[i] = 0;

		for (u32Y = s32StartY; u32Y < s32StartY + stSize.u32Height; u32Y += stSize.u32Height - 1) {
			for (u32X = s32StartX; u32X < s32StartX + stSize.u32Width; u32X += stSize.u32Width - 1) {
				pu64LumaData[i] += *(pstVirAddr + u32X + u32Y * u32MainStride);
				u32Num++;
			}
		}

		pu64LumaData[i] = pu64LumaData[i] / u32Num;
	}

release_blk:
	if (CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stVideoFrame) != CVI_SUCCESS)
		return CVI_FAILURE;

	return ret;
}

CVI_S32 CVI_VPSS_TriggerSnapFrame(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CVI_U32 u32FrameCnt)
{

	struct vpss_snap_cfg cfg = {.VpssGrp = VpssGrp, .VpssChn = VpssChn, .frame_cnt = u32FrameCnt};

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	if (vpss_trigger_snap_frame(&cfg) != CVI_SUCCESS)
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

//#if defined(__CV181X__)
CVI_S32 CVI_VPSS_SetChnBufWrapAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CHN_BUF_WRAP_S *pstVpssChnBufWrap)
{

	struct vpss_chn_wrap_cfg cfg;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVpssChnBufWrap);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.wrap = *pstVpssChnBufWrap;
	return vpss_set_chn_wrap(&cfg);
}

CVI_S32 CVI_VPSS_GetChnBufWrapAttr(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_CHN_BUF_WRAP_S *pstVpssChnBufWrap)
{
	struct vpss_chn_wrap_cfg cfg;
	CVI_S32 ret;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstVpssChnBufWrap);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);
	CHECK_VPSS_CHN_VALID(VpssGrp, VpssChn);

	cfg.VpssGrp = VpssGrp;
	cfg.VpssChn = VpssChn;
	cfg.wrap = *pstVpssChnBufWrap;

	ret = vpss_get_chn_wrap(&cfg);
	if (ret == CVI_SUCCESS)
		memcpy(pstVpssChnBufWrap, &cfg.wrap, sizeof(*pstVpssChnBufWrap));

	return ret;
}

CVI_U32 CVI_VPSS_GetWrapBufferSize(CVI_U32 u32Width, CVI_U32 u32Height, PIXEL_FORMAT_E enPixelFormat,
	CVI_U32 u32BufLine, CVI_U32 u32BufDepth)
{
	CVI_U32 u32BufSize;
	VB_CAL_CONFIG_S stCalConfig;

	if (u32Width < 64 || u32Height < 64) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "width(%d) or height(%d) too small\n", u32Width, u32Height);
		return 0;
	}
	if (u32BufLine != 64 && u32BufLine != 128) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "u32BufLine(%d) invalid, only 64 or 128 lines\n",
				u32BufLine);
		return 0;
	}
	if (u32BufDepth < 2 || u32BufDepth > 32) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "u32BufDepth(%d) invalid, 2 ~ 32\n",
				u32BufDepth);
		return 0;
	}

	COMMON_GetPicBufferConfig(u32Width, u32Height, enPixelFormat, DATA_BITWIDTH_8
		, COMPRESS_MODE_NONE, DEFAULT_ALIGN, &stCalConfig);

	u32BufSize = stCalConfig.u32VBSize / u32Height;
	u32BufSize *= u32BufLine * u32BufDepth;
	CVI_TRACE_VPSS(CVI_DBG_INFO, "width(%d), height(%d), u32BufSize=%d\n",
		u32Width, u32Height, u32BufSize);

	return u32BufSize;
}

static CVI_S32 set_window_attr(VPSS_GRP VpssGrp, RECT_S *pstDstRect,
			VIDEO_FRAME_INFO_S *cur_frame, CVI_BOOL bFilled)
{
	CVI_S32 s32Ret;
	VPSS_GRP_ATTR_S stGrpAttr;
	VPSS_CHN_ATTR_S stChnAttr;

	s32Ret = CVI_VPSS_GetGrpAttr(VpssGrp, &stGrpAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_GetGrpAttr failed\n");
		return s32Ret;
	}
	stGrpAttr.u32MaxW = cur_frame->stVFrame.u32Width;
	stGrpAttr.u32MaxH = cur_frame->stVFrame.u32Height;
	stGrpAttr.enPixelFormat = cur_frame->stVFrame.enPixelFormat;
	s32Ret = CVI_VPSS_SetGrpAttr(VpssGrp, &stGrpAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SetGrpAttr failed\n");
		return s32Ret;
	}

	s32Ret = CVI_VPSS_GetChnAttr(VpssGrp, 0, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_GetChnAttr failed\n");
		return s32Ret;
	}

	//clean doneq buffers
	if (bFilled) {
		stChnAttr.u32Depth = 0;
		s32Ret = CVI_VPSS_SetChnAttr(VpssGrp, 0, &stChnAttr);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SetChnAttr failed\n");
			return s32Ret;
		}
	}

	stChnAttr.u32Depth = 1;
	stChnAttr.stAspectRatio.enMode	= ASPECT_RATIO_MANUAL;
	stChnAttr.stAspectRatio.stVideoRect.s32X	= pstDstRect->s32X;
	stChnAttr.stAspectRatio.stVideoRect.s32Y	= pstDstRect->s32Y;
	stChnAttr.stAspectRatio.stVideoRect.u32Width  = pstDstRect->u32Width;
	stChnAttr.stAspectRatio.stVideoRect.u32Height = pstDstRect->u32Height;

	if (bFilled) {
		// fill bg color is the there this is the first window
		stChnAttr.stAspectRatio.bEnableBgColor = CVI_TRUE;
		stChnAttr.stAspectRatio.u32BgColor = 0x0;
	} else{
		stChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
		stChnAttr.stAspectRatio.u32BgColor = 0x0;
	}

	s32Ret = CVI_VPSS_SetChnAttr(VpssGrp, 0, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SetChnAttr failed\n");
		return s32Ret;
	}
	return CVI_SUCCESS;
}

static CVI_S32 update_src_frame(CVI_STITCH_ATTR_S *pstStitchAttr, CVI_BOOL isFirst,
		VIDEO_FRAME_INFO_S *pstFrameSrc)
{
	CVI_S32 i, s32Ret = CVI_SUCCESS;
	CVI_STITCH_CHN_S *pstStitchChn;
	VIDEO_FRAME_INFO_S stFrame;

	for (i = 0; i < pstStitchAttr->u8ChnNum; i++) {
		pstStitchChn = &pstStitchAttr->astStitchChn[i];

		//skip
		if (pstStitchChn->stStitchSrc.VpssGrp == -1)
			continue;
		//get source frame
		s32Ret = CVI_VPSS_GetChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
			pstStitchChn->stStitchSrc.VpssChn, &stFrame, isFirst ? 1000 : 0);
		if (s32Ret == CVI_SUCCESS) {
			if (!isFirst)
				CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
					pstStitchChn->stStitchSrc.VpssChn, &pstFrameSrc[i]);
			memcpy(&pstFrameSrc[i], &stFrame, sizeof(stFrame));
		} else if (isFirst) {
			break;
		}
	}

	if (isFirst && s32Ret) {
		for (--i; i >= 0; --i) {
			pstStitchChn = &pstStitchAttr->astStitchChn[i];

			CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
				pstStitchChn->stStitchSrc.VpssChn, &pstFrameSrc[i]);
		}
	}

	return isFirst ? s32Ret : CVI_SUCCESS;
}

static CVI_VOID release_src_frame(CVI_STITCH_ATTR_S *pstStitchAttr,
		VIDEO_FRAME_INFO_S *pstFrameSrc)
{
	CVI_S32 i;
	CVI_STITCH_CHN_S *pstStitchChn;

	for (i = 0; i < pstStitchAttr->u8ChnNum; i++) {
		pstStitchChn = &pstStitchAttr->astStitchChn[i];

		//skip
		if (pstStitchChn->stStitchSrc.VpssGrp == -1)
			continue;
		CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
			pstStitchChn->stStitchSrc.VpssChn, &pstFrameSrc[i]);
	}
}

static CVI_S32 stitch_proc_frc(struct cvi_stitch_ctx *pStitchCtx,
		VIDEO_FRAME_INFO_S *pstFrameSrc)
{
	CVI_S32 i, s32Ret;
	CVI_STITCH_ATTR_S *pstStitchAttr = &pStitchCtx->stStitchAttr;
	CVI_STITCH_CHN_S *pstStitchChn;
	VPSS_GRP VpssGrp = pStitchCtx->VpssGrp;
	CVI_BOOL bFilled = CVI_TRUE;
	VIDEO_FRAME_INFO_S stFrame_out;

	for (i = 0; i < pstStitchAttr->u8ChnNum; i++) {
		pstStitchChn = &pstStitchAttr->astStitchChn[i];

		//skip
		if (pstStitchChn->stStitchSrc.VpssGrp == -1)
			continue;

		//not stitch
		if ((pstStitchAttr->u8ChnNum == 1) &&
			(pstFrameSrc[i].stVFrame.u32Width == pstStitchAttr->stOutSize.u32Width) &&
			(pstFrameSrc[i].stVFrame.u32Height == pstStitchAttr->stOutSize.u32Height)) {
			CVI_VO_SendFrame(0, pstStitchAttr->VoChn, &pstFrameSrc[i], 1000);
			return CVI_SUCCESS;
		}

		if (!bFilled) {
			s32Ret = CVI_VPSS_SendChnFrame(VpssGrp, 0, &stFrame_out, 1000);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SendChnFrame failed\n");
				CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
				break;
			}
			CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
		}

		s32Ret = set_window_attr(VpssGrp, &pstStitchChn->stDstRect, &pstFrameSrc[i], bFilled);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "set_window_attr failed\n");
			break;
		}

		s32Ret = CVI_VPSS_SendFrame(VpssGrp, &pstFrameSrc[i], 1000);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SendFrame failed\n");
			break;
		}

		s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, 0, &stFrame_out, 1000);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "i(%d) Grp(%d) Chn(0) GetChnFrame failed\n", i, VpssGrp);
			break;
		}

		bFilled = CVI_FALSE;
	}

	if (!bFilled) {
		if (s32Ret == CVI_SUCCESS)
			CVI_VO_SendFrame(0, pstStitchAttr->VoChn, &stFrame_out, 1000);
		CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
	}

	return s32Ret;
}

static void wake_up_stitch(void *timer, void *arg)
{
	struct cvi_stitch_ctx *pStitchCtx = (struct cvi_stitch_ctx *)arg;

	aos_event_set(&pStitchCtx->wait, 0x1, AOS_EVENT_OR);
}

static void *vpss_stitch_handler_frc(void *arg)
{
	CVI_S32 interval_ms;
	CVI_S32 s32Ret;
	unsigned int actl_flags;
	struct cvi_stitch_ctx *pStitchCtx = (struct cvi_stitch_ctx *)arg;
	struct cvi_stitch_ctx stStitchCtx;
	CVI_VOID *buf;
	VIDEO_FRAME_INFO_S *pstFrameSrc;
	CVI_BOOL isFirst = CVI_TRUE;

	buf = malloc(sizeof(VIDEO_FRAME_INFO_S) * CVI_STITCH_CHN_MAX_NUM);
	if (buf == NULL) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "malloc failed.\n");
		return NULL;
	}
	pstFrameSrc = (VIDEO_FRAME_INFO_S *)buf;

	if (aos_event_new(&pStitchCtx->wait, 0)) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "aos_event_new failed.\n");
		free(buf);
		return NULL;
	}

	//start timer
	interval_ms = 1000 / pStitchCtx->stStitchAttr.s32OutFps;
	if (aos_timer_new(&pStitchCtx->timer, wake_up_stitch,
		arg, interval_ms, AOS_TIMER_REPEAT)) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "aos_timer_new failed.\n");
		aos_event_free(&pStitchCtx->wait);
		free(buf);
		return NULL;
	}
	if (aos_timer_start(&pStitchCtx->timer)) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "aos_timer_start failed.\n");
		aos_timer_free(&pStitchCtx->timer);
		aos_event_free(&pStitchCtx->wait);
		free(buf);
		return NULL;
	}

	while (pStitchCtx->bStart) {
		s32Ret = aos_event_get(&pStitchCtx->wait, 0x1, AOS_EVENT_OR_CLEAR, &actl_flags, 100);
		/* timeout */
		if (s32Ret < 0) {
			continue;
		}
		if (!pStitchCtx->bStart)
			break;

		memcpy(&stStitchCtx, pStitchCtx, sizeof(stStitchCtx));
		if (update_src_frame(&stStitchCtx.stStitchAttr, isFirst, pstFrameSrc))
			continue;

		isFirst = CVI_FALSE;
		stitch_proc_frc(&stStitchCtx, pstFrameSrc);
	}

	if (!isFirst)
		release_src_frame(&stStitchCtx.stStitchAttr, pstFrameSrc);

	if (aos_timer_is_valid(&pStitchCtx->timer)) {
		aos_timer_stop(&pStitchCtx->timer);
		aos_timer_free(&pStitchCtx->timer);
	}

	aos_event_free(&pStitchCtx->wait);
	free(buf);

	return NULL;
}

static CVI_S32 stitch_proc(struct cvi_stitch_ctx *pStitchCtx)
{
	CVI_S32 i, s32Ret;
	CVI_STITCH_ATTR_S stStitchAttr;
	CVI_STITCH_ATTR_S *pstStitchAttr = &stStitchAttr;
	CVI_STITCH_CHN_S *pstStitchChn;
	VPSS_GRP VpssGrp = pStitchCtx->VpssGrp;
	CVI_BOOL bFilled = CVI_TRUE;
	VIDEO_FRAME_INFO_S stFrame_src, stFrame_out;

	pthread_mutex_lock(&pStitchCtx->lock);
	memcpy(pstStitchAttr, &pStitchCtx->stStitchAttr, sizeof(stStitchAttr));
	pthread_mutex_unlock(&pStitchCtx->lock);

	for (i = 0; i < pstStitchAttr->u8ChnNum; i++) {
		pstStitchChn = &pstStitchAttr->astStitchChn[i];

		//skip
		if (pstStitchChn->stStitchSrc.VpssGrp == -1)
			continue;
		//get source frame
		s32Ret = CVI_VPSS_GetChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
			pstStitchChn->stStitchSrc.VpssChn, &stFrame_src, 1000);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "Grp(%d) Chn(%d) GetChnFrame failed\n",
				pstStitchChn->stStitchSrc.VpssGrp,
				pstStitchChn->stStitchSrc.VpssChn);
			break;
		}

		//not stitch
		if ((pstStitchAttr->u8ChnNum == 1) &&
			(stFrame_src.stVFrame.u32Width == pstStitchAttr->stOutSize.u32Width) &&
			(stFrame_src.stVFrame.u32Height == pstStitchAttr->stOutSize.u32Height)) {
			CVI_VO_SendFrame(0, pstStitchAttr->VoChn, &stFrame_src, 1000);
			CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
				pstStitchChn->stStitchSrc.VpssChn, &stFrame_src);
			return CVI_SUCCESS;
		}

		if (!bFilled) {
			s32Ret = CVI_VPSS_SendChnFrame(VpssGrp, 0, &stFrame_out, 1000);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SendChnFrame failed\n");
				CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
					pstStitchChn->stStitchSrc.VpssChn, &stFrame_src);
				CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
				break;
			}
			CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
		}
		s32Ret = set_window_attr(VpssGrp, &pstStitchChn->stDstRect, &stFrame_src, bFilled);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "set_window_attr failed\n");
			CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
				pstStitchChn->stStitchSrc.VpssChn, &stFrame_src);
			break;
		}
		s32Ret = CVI_VPSS_SendFrame(VpssGrp, &stFrame_src, 1000);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VPSS_SendFrame failed\n");
			CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
				pstStitchChn->stStitchSrc.VpssChn, &stFrame_src);
			break;
		}
		CVI_VPSS_ReleaseChnFrame(pstStitchChn->stStitchSrc.VpssGrp,
			pstStitchChn->stStitchSrc.VpssChn, &stFrame_src);

		s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, 0, &stFrame_out, 1000);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "i(%d) Grp(%d) Chn(0) GetChnFrame failed\n", i, VpssGrp);
			break;
		}

		bFilled = CVI_FALSE;
	}

	if (!bFilled) {
		if (s32Ret == CVI_SUCCESS)
			CVI_VO_SendFrame(0, pstStitchAttr->VoChn, &stFrame_out, 1000);
		CVI_VPSS_ReleaseChnFrame(VpssGrp, 0, &stFrame_out);
	}

	return s32Ret;
}

static void *vpss_stitch_handler(void *arg)
{
	struct cvi_stitch_ctx *pStitchCtx = (struct cvi_stitch_ctx *)arg;

	while (pStitchCtx->bStart) {
		stitch_proc(pStitchCtx);
		usleep(10000);
	}

	return NULL;
}

CVI_S32 CVI_VPSS_CreateStitch(VPSS_GRP VpssGrp, const CVI_STITCH_ATTR_S *pstStitchAttr)
{
	CVI_S32 s32Ret;
	VPSS_GRP_ATTR_S stGrpAttr;
	VPSS_CHN_ATTR_S stChnAttr;

	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstStitchAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);

	if (s_StitchCtx[VpssGrp].bUse)
		return CVI_SUCCESS;
	if (!pstStitchAttr->s32OutFps) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "s32OutFps cannot be 0\n");
		return CVI_FAILURE;
	}
	memset(&s_StitchCtx[VpssGrp], 0, sizeof(struct cvi_stitch_ctx));

	stGrpAttr.u32MaxW = pstStitchAttr->stOutSize.u32Width; //invalid
	stGrpAttr.u32MaxH = pstStitchAttr->stOutSize.u32Height; //invalid
	stGrpAttr.enPixelFormat = pstStitchAttr->enOutPixelFormat; //invalid
	stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
	stGrpAttr.stFrameRate.s32DstFrameRate = -1;
	stGrpAttr.u8VpssDev = 0;
	s32Ret = CVI_VPSS_CreateGrp(VpssGrp, &stGrpAttr);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	stChnAttr.u32Width = pstStitchAttr->stOutSize.u32Width;
	stChnAttr.u32Height = pstStitchAttr->stOutSize.u32Height;
	stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
	stChnAttr.enPixelFormat = pstStitchAttr->enOutPixelFormat;
	stChnAttr.stFrameRate.s32SrcFrameRate = -1;
	stChnAttr.stFrameRate.s32DstFrameRate = -1;
	stChnAttr.bMirror = CVI_FALSE;
	stChnAttr.bFlip = CVI_FALSE;
	stChnAttr.u32Depth = 1;
	stChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
	stChnAttr.stNormalize.bEnable = CVI_FALSE;
	s32Ret = CVI_VPSS_SetChnAttr(VpssGrp, 0, &stChnAttr);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	s32Ret = CVI_VPSS_EnableChn(VpssGrp, 0);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	s32Ret = CVI_VPSS_StartGrp(VpssGrp);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	if (pstStitchAttr->hVbPool == VB_INVALID_POOLID) {
		VB_POOL_CONFIG_S stVbPoolCfg;
		VB_POOL chnVbPool;
		CVI_U32 u32BlkSize = 0;

		u32BlkSize = COMMON_GetPicBufferSize(stChnAttr.u32Width, stChnAttr.u32Height, stChnAttr.enPixelFormat,
			DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);

		memset(&stVbPoolCfg, 0, sizeof(VB_POOL_CONFIG_S));
		stVbPoolCfg.u32BlkSize	= u32BlkSize;
		stVbPoolCfg.u32BlkCnt	= 1;
		stVbPoolCfg.enRemapMode = VB_REMAP_MODE_CACHED;
		chnVbPool = CVI_VB_CreatePool(&stVbPoolCfg);
		if (chnVbPool == VB_INVALID_POOLID) {
			CVI_TRACE_VPSS(CVI_DBG_ERR, "CVI_VB_CreatePool failed.\n", VpssGrp);
		} else {
			CVI_VPSS_AttachVbPool(VpssGrp, 0, chnVbPool);
			s_StitchCtx[VpssGrp].VbPool = chnVbPool;
		}
	} else {
		CVI_VPSS_AttachVbPool(VpssGrp, 0, pstStitchAttr->hVbPool);
	}

	pthread_mutex_init(&s_StitchCtx[VpssGrp].lock, NULL);
	s_StitchCtx[VpssGrp].stStitchAttr = *pstStitchAttr;
	s_StitchCtx[VpssGrp].VpssGrp = VpssGrp;
	s_StitchCtx[VpssGrp].bUse = CVI_TRUE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_DestroyStitch(VPSS_GRP VpssGrp)
{
	CVI_S32 s32Ret;

	CHECK_VPSS_GRP_VALID(VpssGrp);

	if (!s_StitchCtx[VpssGrp].bUse)
		return CVI_SUCCESS;

	s32Ret = CVI_VPSS_DisableChn(VpssGrp, 0);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	s32Ret = CVI_VPSS_StopGrp(VpssGrp);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	s32Ret = CVI_VPSS_DestroyGrp(VpssGrp);
	if (s32Ret != CVI_SUCCESS)
		return s32Ret;

	if (s_StitchCtx[VpssGrp].stStitchAttr.hVbPool == VB_INVALID_POOLID) {
		CVI_VB_DestroyPool(s_StitchCtx[VpssGrp].VbPool);
	}

	pthread_mutex_destroy(&s_StitchCtx[VpssGrp].lock);
	s_StitchCtx[VpssGrp].bUse = CVI_FALSE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_SetStitchAttr(VPSS_GRP VpssGrp, const CVI_STITCH_ATTR_S *pstStitchAttr)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstStitchAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (!s_StitchCtx[VpssGrp].bUse) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "vpss(%d), The stitch not created\n", VpssGrp);
		return CVI_FAILURE;
	}
	if (!pstStitchAttr->s32OutFps) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "s32OutFps cannot be 0\n");
		return CVI_FAILURE;
	}

	pthread_mutex_lock(&s_StitchCtx[VpssGrp].lock);
	s_StitchCtx[VpssGrp].stStitchAttr = *pstStitchAttr;
	pthread_mutex_unlock(&s_StitchCtx[VpssGrp].lock);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_GetStitchAttr(VPSS_GRP VpssGrp, CVI_STITCH_ATTR_S *pstStitchAttr)
{
	MOD_CHECK_NULL_PTR(CVI_ID_VPSS, pstStitchAttr);
	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (!s_StitchCtx[VpssGrp].bUse) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "vpss(%d), The stitch not created\n", VpssGrp);
		return CVI_FAILURE;
	}
	*pstStitchAttr = s_StitchCtx[VpssGrp].stStitchAttr;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_StartStitch(VPSS_GRP VpssGrp)
{
	CVI_S32 s32Ret;
	struct sched_param tsk;
	pthread_attr_t attr;
	char thread_name[32];
	struct cvi_stitch_ctx *pStitchCtx;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (!s_StitchCtx[VpssGrp].bUse) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "vpss(%d), The stitch not created\n", VpssGrp);
		return CVI_FAILURE;
	}
	pStitchCtx = &s_StitchCtx[VpssGrp];
	if (pStitchCtx->bStart)
		return CVI_SUCCESS;

	pStitchCtx->bStart = CVI_TRUE;

	//set vpss thread attr
	tsk.sched_priority = 45; //VIP_RT_PRIO;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	pthread_attr_setschedparam(&attr, &tsk);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

	s32Ret = pthread_create(&pStitchCtx->thread, &attr,
		pStitchCtx->stStitchAttr.s32OutFps < 0 ? vpss_stitch_handler : vpss_stitch_handler_frc,
		(void *)pStitchCtx);
	if (s32Ret != 0) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "failed to create stitch pthread.\n");
		pStitchCtx->bStart = CVI_FALSE;
		return CVI_FAILURE;
	}
	snprintf(thread_name, 31, "stitch-%d", VpssGrp);
	pthread_setname_np(pStitchCtx->thread, thread_name);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VPSS_StopStitch(VPSS_GRP VpssGrp)
{
	struct cvi_stitch_ctx *pStitchCtx;

	CHECK_VPSS_GRP_VALID(VpssGrp);
	CHECK_VPSS_GRP_CREATED(VpssGrp);

	if (!s_StitchCtx[VpssGrp].bUse) {
		CVI_TRACE_VPSS(CVI_DBG_ERR, "vpss(%d), The stitch not created\n", VpssGrp);
		return CVI_FAILURE;
	}
	pStitchCtx = &s_StitchCtx[VpssGrp];
	if (!pStitchCtx->bStart)
		return CVI_SUCCESS;

	pStitchCtx->bStart = CVI_FALSE;
	if ((pStitchCtx->stStitchAttr.s32OutFps > 0) &&
		aos_event_is_valid(&pStitchCtx->wait))
		aos_event_set(&pStitchCtx->wait, 0x1, AOS_EVENT_OR);
	pthread_join(pStitchCtx->thread, NULL);
	pStitchCtx->thread = NULL;

	return CVI_SUCCESS;
}

//#endif
