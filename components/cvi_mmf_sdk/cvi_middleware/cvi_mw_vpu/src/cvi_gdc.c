#include "aos/kernel.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <inttypes.h>
#include <unistd.h>

#include "cvi_buffer.h"
#include "cvi_base.h"
#include "cvi_sys.h"
#include "cvi_vb.h"
#include "cvi_mw_base.h"

#include "cvi_gdc.h"
#include "gdc_mesh.h"
#include "ldc_ioctl.h"

#define LDC_YUV_BLACK 0x808000
#define LDC_RGB_BLACK 0x0

#define CHECK_GDC_FORMAT(imgIn, imgOut)                                                                                \
	do {                                                                                                           \
		if (imgIn.stVFrame.enPixelFormat != imgOut.stVFrame.enPixelFormat) {                                   \
			CVI_TRACE_GDC(CVI_DBG_ERR, "in/out pixelformat(%d-%d) mismatch\n",                             \
				      imgIn.stVFrame.enPixelFormat, imgOut.stVFrame.enPixelFormat);                    \
			return CVI_ERR_GDC_ILLEGAL_PARAM;                                                              \
		}                                                                                                      \
		if (!GDC_SUPPORT_FMT(imgIn.stVFrame.enPixelFormat)) {                                                  \
			CVI_TRACE_GDC(CVI_DBG_ERR, "pixelformat(%d) unsupported\n", imgIn.stVFrame.enPixelFormat);     \
			return CVI_ERR_GDC_ILLEGAL_PARAM;                                                              \
		}                                                                                                      \
	} while (0)

static CVI_S32 gdc_rotation_check_size(ROTATION_E enRotation, const GDC_TASK_ATTR_S *pstTask)
{
	if (enRotation >= ROTATION_MAX) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "invalid rotation(%d).\n", enRotation);
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}

	if (enRotation == ROTATION_90 || enRotation == ROTATION_270 || enRotation == ROTATION_XY_FLIP) {
		if (pstTask->stImgOut.stVFrame.u32Width < pstTask->stImgIn.stVFrame.u32Height) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "rotation(%d) invalid: 'output width(%d) < input height(%d)'\n",
				      enRotation, pstTask->stImgOut.stVFrame.u32Width,
				      pstTask->stImgIn.stVFrame.u32Height);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
		if (pstTask->stImgOut.stVFrame.u32Height < pstTask->stImgIn.stVFrame.u32Width) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "rotation(%d) invalid: 'output height(%d) < input width(%d)'\n",
				      enRotation, pstTask->stImgOut.stVFrame.u32Height,
				      pstTask->stImgIn.stVFrame.u32Width);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
	} else {
		if (pstTask->stImgOut.stVFrame.u32Width < pstTask->stImgIn.stVFrame.u32Width) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "rotation(%d) invalid: 'output width(%d) < input width(%d)'\n",
				      enRotation, pstTask->stImgOut.stVFrame.u32Width,
				      pstTask->stImgIn.stVFrame.u32Width);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
		if (pstTask->stImgOut.stVFrame.u32Height < pstTask->stImgIn.stVFrame.u32Height) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "rotation(%d) invalid: 'output height(%d) < input height(%d)'\n",
				      enRotation, pstTask->stImgOut.stVFrame.u32Height,
				      pstTask->stImgIn.stVFrame.u32Height);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
	}

	return CVI_SUCCESS;
}

void gdcq_init(void)
{
}

CVI_S32 CVI_GDC_Suspend(void)
{
	if (cvi_gdc_suspend())
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

CVI_S32 CVI_GDC_Resume(void)
{
	if (cvi_gdc_resume())
		return CVI_FAILURE;

	return CVI_SUCCESS;
}

/**************************************************************************
 *   Public APIs.
 **************************************************************************/
CVI_S32 CVI_GDC_BeginJob(GDC_HANDLE *phHandle)
{
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, phHandle);

	struct gdc_handle_data cfg;

	memset(&cfg, 0, sizeof(cfg));
	if (cvi_gdc_begin_job(0, &cfg))
		return CVI_FAILURE;

	*phHandle = cfg.handle;

	return CVI_SUCCESS;
}

CVI_S32 CVI_GDC_EndJob(GDC_HANDLE hHandle)
{
	struct gdc_handle_data cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.handle = hHandle;
	return cvi_gdc_end_job(0, &cfg);
}

CVI_S32 CVI_GDC_CancelJob(GDC_HANDLE hHandle)
{
	struct gdc_handle_data cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.handle = hHandle;
	return cvi_gdc_cancel_job(0, &cfg);
}

CVI_S32 CVI_GDC_AddCorrectionTask(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask,
				  const FISHEYE_ATTR_S *pstFishEyeAttr)
{
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstTask);
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstFishEyeAttr);
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(hHandle);

	CVI_TRACE_GDC(CVI_DBG_ERR, "not supported\n");
	return CVI_ERR_GDC_NOT_SUPPORT;
}

CVI_S32 CVI_GDC_AddRotationTask(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask, ROTATION_E enRotation)
{
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstTask);
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(hHandle);

	if (gdc_rotation_check_size(enRotation, pstTask) != CVI_SUCCESS) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "gdc_rotation_check_size fail\n");
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}

	struct gdc_task_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.handle = hHandle;
	memcpy(&attr.stImgIn, &pstTask->stImgIn, sizeof(attr.stImgIn));
	memcpy(&attr.stImgOut, &pstTask->stImgOut, sizeof(attr.stImgOut));
	//memcpy(attr.au64privateData, pstTask->au64privateData, sizeof(attr.au64privateData));
	//attr.reserved = pstTask->reserved;
	attr.enRotation = enRotation;
	return cvi_gdc_add_rotation_task(0, &attr);
}

CVI_S32 CVI_GDC_AddAffineTask(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask, const AFFINE_ATTR_S *pstAffineAttr)
{
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstTask);
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstAffineAttr);
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(hHandle);

	CVI_TRACE_GDC(CVI_DBG_ERR, "not supported\n");
	return CVI_ERR_GDC_NOT_SUPPORT;
}

CVI_S32 CVI_GDC_AddLDCTask(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask
	, const LDC_ATTR_S *pstLDCAttr, ROTATION_E enRotation)
{
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstTask);
	MOD_CHECK_NULL_PTR(CVI_ID_GDC, pstLDCAttr);
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(hHandle);

	if (enRotation != ROTATION_0) {
		if (gdc_rotation_check_size(enRotation, pstTask) != CVI_SUCCESS) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "gdc_rotation_check_size fail\n");
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
	}

	struct gdc_task_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.handle = hHandle;
	memcpy(&attr.stImgIn, &pstTask->stImgIn, sizeof(attr.stImgIn));
	memcpy(&attr.stImgOut, &pstTask->stImgOut, sizeof(attr.stImgOut));
	memcpy(attr.au64privateData, pstTask->au64privateData, sizeof(attr.au64privateData));
	attr.reserved = pstTask->reserved;
	attr.enRotation = enRotation;
	return cvi_gdc_add_ldc_task(0, &attr);
}

CVI_S32 CVI_GDC_AddCorrectionTaskCNV(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask
	, const FISHEYE_ATTR_S *pstFishEyeAttr, uint8_t *p_tbl, uint8_t *p_idl, uint32_t *tbl_param)
{
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(hHandle);
	UNUSED(pstFishEyeAttr);
	UNUSED(p_tbl);
	UNUSED(p_idl);
	UNUSED(tbl_param);

	CVI_TRACE_GDC(CVI_DBG_ERR, "not supported\n");
	return CVI_ERR_GDC_NOT_SUPPORT;
}

CVI_S32 CVI_GDC_AddCnvWarpTask(const float *pfmesh_data, GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask,
	const FISHEYE_ATTR_S *pstAffineAttr, CVI_BOOL *bReNew)
{
	CHECK_GDC_FORMAT(pstTask->stImgIn, pstTask->stImgOut);
	UNUSED(pfmesh_data);
	UNUSED(hHandle);
	UNUSED(pstAffineAttr);
	UNUSED(bReNew);

	CVI_TRACE_GDC(CVI_DBG_ERR, "not supported\n");
	return CVI_ERR_GDC_NOT_SUPPORT;
}

CVI_S32 CVI_GDC_GenLDCMesh(CVI_U32 u32Width, CVI_U32 u32Height, const LDC_ATTR_S *pstLDCAttr,
		const char *name, CVI_U64 *pu64PhyAddr, CVI_VOID **ppVirAddr)
{
	CVI_U64 paddr;
	CVI_VOID *vaddr;
	SIZE_S in_size, out_size;
	CVI_U32 mesh_1st_size = 0, mesh_2nd_size = 0, mesh_size = 0;
	ROTATION_E enRotation = ROTATION_0;

	if (pstLDCAttr->bAspect) {
		if (pstLDCAttr->s32XYRatio < 0 || pstLDCAttr->s32XYRatio > 100) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32XYRatio(%d).\n"
				      , pstLDCAttr->s32XYRatio);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
	} else {
		if (pstLDCAttr->s32XRatio < 0 || pstLDCAttr->s32XRatio > 100) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32XRatio(%d).\n"
				      , pstLDCAttr->s32XRatio);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
		if (pstLDCAttr->s32YRatio < 0 || pstLDCAttr->s32YRatio > 100) {
			CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32YRatio(%d).\n"
				      , pstLDCAttr->s32YRatio);
			return CVI_ERR_GDC_ILLEGAL_PARAM;
		}
	}
	if (pstLDCAttr->s32CenterXOffset < -511 || pstLDCAttr->s32CenterXOffset > 511) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32CenterXOffset(%d).\n"
			      , pstLDCAttr->s32CenterXOffset);
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}
	if (pstLDCAttr->s32CenterYOffset < -511 || pstLDCAttr->s32CenterYOffset > 511) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32CenterYOffset(%d).\n"
			      , pstLDCAttr->s32CenterYOffset);
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}
	if (pstLDCAttr->s32DistortionRatio < -300 || pstLDCAttr->s32DistortionRatio > 500) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "Invalid LDC s32DistortionRatio(%d).\n"
			      , pstLDCAttr->s32DistortionRatio);
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}
	if (!name) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "Please assign name for LDC mesh\n");
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}

	in_size.u32Width = ALIGN(u32Width, DEFAULT_ALIGN);
	in_size.u32Height = ALIGN(u32Height, DEFAULT_ALIGN);
	out_size.u32Width = in_size.u32Width;
	out_size.u32Height = in_size.u32Height;

	mesh_gen_get_size(in_size, out_size, &mesh_1st_size, &mesh_2nd_size);
	mesh_size = mesh_1st_size + mesh_2nd_size;

	CVI_TRACE_GDC(CVI_DBG_DEBUG, "W=%d, H=%d, mesh_size=%d\n",
			in_size.u32Width, in_size.u32Height,
			mesh_size);

	if (CVI_SYS_IonAlloc_Cached(&paddr, &vaddr, name, mesh_size + DEFAULT_ALIGN) != CVI_SUCCESS) {
		CVI_TRACE_GDC(CVI_DBG_ERR, " Can't acquire memory for LDC mesh.\n");
		return CVI_ERR_GDC_NOMEM;
	}

	if (mesh_gen_ldc(in_size, out_size, pstLDCAttr, ALIGN(paddr, DEFAULT_ALIGN)
		, ALIGN((CVI_U64)vaddr, DEFAULT_ALIGN), enRotation) != CVI_SUCCESS) {
		CVI_SYS_IonFree(paddr, vaddr);
		CVI_TRACE_GDC(CVI_DBG_ERR, "Can't generate ldc mesh.\n");
		return CVI_ERR_GDC_ILLEGAL_PARAM;
	}
	CVI_SYS_IonFlushCache(paddr, vaddr, mesh_size + DEFAULT_ALIGN);

	*pu64PhyAddr = paddr;
	*ppVirAddr = vaddr;

	return CVI_SUCCESS;
}

CVI_S32 CVI_GDC_SetBufWrapAttr(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask, const LDC_BUF_WRAP_S *pstBufWrap)
{
	struct ldc_buf_wrap_cfg *cfg;
	CVI_S32 s32Ret;

	cfg = aos_malloc(sizeof(*cfg));
	if (!cfg) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "gdc malloc fails.\n");
		return CVI_FAILURE;
	}

	memset(cfg, 0, sizeof(*cfg));
	cfg->handle = hHandle;
	memcpy(&cfg->stTask.stImgIn, &pstTask->stImgIn, sizeof(cfg->stTask.stImgIn));
	memcpy(&cfg->stTask.stImgOut, &pstTask->stImgOut, sizeof(cfg->stTask.stImgOut));
	memcpy(&cfg->stBufWrap, pstBufWrap, sizeof(cfg->stBufWrap));

	s32Ret = cvi_gdc_set_chn_buf_wrap(0, cfg);

	aos_free(cfg);

	return s32Ret;
}

CVI_S32 CVI_GDC_GetBufWrapAttr(GDC_HANDLE hHandle, const GDC_TASK_ATTR_S *pstTask, LDC_BUF_WRAP_S *pstBufWrap)
{
	struct ldc_buf_wrap_cfg *cfg;
	CVI_S32 s32Ret;

	cfg = aos_malloc(sizeof(*cfg));
	if (!cfg) {
		CVI_TRACE_GDC(CVI_DBG_ERR, "gdc malloc fails.\n");
		return CVI_FAILURE;
	}

	memset(cfg, 0, sizeof(*cfg));
	cfg->handle = hHandle;
	memcpy(&cfg->stTask, pstTask, sizeof(cfg->stTask));

	s32Ret = cvi_gdc_get_chn_buf_wrap(0, cfg);

	aos_free(cfg);

	if (s32Ret == CVI_SUCCESS)
		memcpy(pstBufWrap, &cfg->stBufWrap, sizeof(*pstBufWrap));

	return s32Ret;
}
