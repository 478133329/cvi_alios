#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <inttypes.h>

#include "cvi_buffer.h"
#include "cvi_base.h"
#include "cvi_mw_base.h"
#include "cvi_vi.h"
#include "cvi_vb.h"
#include "cvi_sys.h"
#include "cvi_gdc.h"
#include "cvi_errno.h"
#include "vi_ioctl.h"
#include <vi_isp.h>

//TODO gdc sns_crtl.h

#include "gdc_mesh.h"
#include "cvi_sns_ctrl.h"

//#include "cvi_rpc.h"
#include "dump_register.h"

#include <cvi_vi_ctx.h>

#define CHECK_VI_PIPEID_VALID(x)						\
	do {									\
		if ((x) > (VI_MAX_PIPE_NUM - 1)) {				\
			CVI_TRACE_VI(CVI_DBG_ERR, " invalid pipe-id(%d)\n", x);	\
			return CVI_ERR_VI_INVALID_PIPEID;			\
		}								\
	} while (0)

#define CHECK_VI_DEVID_VALID(x)							\
	do {									\
		if ((x) > (VI_MAX_DEV_NUM - 1)) {				\
			CVI_TRACE_VI(CVI_DBG_ERR, " invalid dev-id(%d)\n", x);	\
			return CVI_ERR_VI_INVALID_DEVID;			\
		}								\
	} while (0)

#define CHECK_VI_CHNID_VALID(x)							\
	do {									\
		if ((x) > (VI_MAX_CHN_NUM - 1)) {				\
			CVI_TRACE_VI(CVI_DBG_ERR, " invalid chn-id(%d)\n", x);	\
			return CVI_ERR_VI_INVALID_CHNID;			\
		}								\
	} while (0)

#define CHECK_VI_NULL_PTR(ptr)							\
	do {									\
		if (ptr == NULL) {						\
			CVI_TRACE_VI(CVI_DBG_ERR, " Invalid null pointer\n");	\
			return CVI_ERR_VI_INVALID_NULL_PTR;			\
		}								\
	} while (0)

#define CHECK_VI_PIPE_CREATED(pipe)						\
	do {									\
		if (gViCtx->isPipeCreated[pipe] == CVI_FALSE) {			\
			CVI_TRACE_VI(CVI_DBG_WARN, "Pipe(%d) is stop\n", pipe);	\
			return CVI_SUCCESS;				\
		}								\
	} while (0)

#define VDEV_CLOSED_CHK(_dev_type, _dev_id)					\
	struct vdev *d;								\
	d = get_dev_info(_dev_type, 0);					\
	if (IS_VDEV_CLOSED(d->state)) {						\
		CVI_TRACE_VI(CVI_DBG_ERR, "vi dev(0) state(%d) incorrect.", d->state);\
		return CVI_ERR_VI_SYS_NOTREADY;					\
	}

#define VIPIPE_TO_DEV(_pipe_id, _dev_id)					\
	do {									\
		if (_pipe_id == 0 || _pipe_id == 1)				\
			_dev_id = 0;						\
		else if (_pipe_id == 2 || _pipe_id == 3)			\
			_dev_id = 1;						\
	} while (0)

#define CHECK_VI_EXTCHNID_VALID(x)										\
	do {													\
		if (((x) < VI_EXT_CHN_START) || ((x) >= (VI_EXT_CHN_START + VI_MAX_EXT_CHN_NUM))) {		\
			CVI_TRACE_VI(CVI_DBG_ERR, " invalid extchn-id(%d)\n", x);				\
			return CVI_ERR_VI_INVALID_CHNID;							\
		}												\
	} while (0)

#define CHECK_VI_GDC_FMT(x)											\
	do {													\
		if (!GDC_SUPPORT_FMT(x)) {			\
			CVI_TRACE_VI(CVI_DBG_ERR, "invalid PixFormat(%d) for gdc.\n", (x));			\
			return CVI_ERR_VI_INVALID_PARA;								\
		}												\
	} while (0)

static inline CVI_S32 CHECK_VI_CTX_NULL_PTR(void *ptr)
{
	CVI_S32 ret = CVI_SUCCESS;

	if (ptr == NULL) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Call SetDevAttr first\n");
		ret = CVI_ERR_VI_FAILED_NOTCONFIG;
	}

	return ret;
}

#define MAX_DIS_NUM  3

typedef CVI_VOID (*pfnChnMirrorFlip) (VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eChnMirrorFlip);
static pfnChnMirrorFlip s_pfnDevMirrorFlip[VI_MAX_DEV_NUM];

struct vi_pm_s {
	VI_PM_OPS_S	stOps;
	CVI_VOID	*pvData;
};
static struct vi_pm_s apstViPm[VI_MAX_DEV_NUM] = { 0 };

static CVI_U32 dis_i_w_pos[VI_MAX_CHN_NUM] = { 0 }, dis_i_frm_num[VI_MAX_CHN_NUM][MAX_DIS_NUM] = { 0 };
static struct crop_size dis_i_data[VI_MAX_CHN_NUM][MAX_DIS_NUM] = { 0 };

static struct cvi_vi_ctx vi_ctx_bak;
//VI dma buffer addr
static CVI_U64 dmaBufpAddr;
static struct cvi_gdc_mesh g_vi_mesh[VI_MAX_CHN_NUM];

static struct cvi_vi_ctx *gViCtx;

struct vi_dbg_th_info_s {
	CVI_U8    th_enable;
	pthread_t vi_dbg_thread;
};
struct vi_dbg_th_info_s gViDbgTH;

/**************************************************************************
 *   Internal APIs for vi only
 **************************************************************************/
#if 0
static CVI_VOID *vi_dbg_handler(CVI_VOID *data)
{
	CVI_S32 fd = -1;
	fd_set rfds;
	//struct timeval tv;
	CVI_S32 ret = CVI_SUCCESS;
	UNUSED(data);

	prctl(PR_SET_NAME, "vi_dbg_handler");

	fd = get_vi_fd();

	gViDbgTH.th_enable = CVI_TRUE;

	while (gViDbgTH.th_enable) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		//tv.tv_sec = 10;
		//tv.tv_usec = 0;

		ret = select(fd + 1, &rfds, NULL, NULL, NULL);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_dbg_thread select error\n");
			break;
		}

		//Cat vi_dbg/mipi_rx if error
		if (FD_ISSET(fd, &rfds) && gViDbgTH.th_enable) {
			system("cat /proc/mipi-rx");
			system("cat /proc/cvitek/vi_dbg");
		}
	}
	CVI_TRACE_VI(CVI_DBG_INFO, "-\n");

	pthread_exit(NULL);
	return 0;
}
#endif

static CVI_S32 _vi_proc_mmap(void)
{
	struct vdev *d;
	struct vi_ext_control ec1;

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		return CVI_ERR_VI_SYS_NOTREADY;
	}

	if (gViCtx != NULL) {
		CVI_TRACE_VI(CVI_DBG_DEBUG, "vi proc has already done mmap\n");
		return CVI_SUCCESS;
	}

	memset(&ec1, 0, sizeof(ec1));
	ec1.id = VI_IOCTL_GET_SHARE_MEM;
	vi_ioctl(VI_IOC_G_CTRL, &ec1);

	gViCtx = ec1.ptr;
	if (!gViCtx) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi proc mmap fail!\n");
		return CVI_FAILURE;
	}

	memset(gViCtx, 0, sizeof(*gViCtx));

	return CVI_SUCCESS;
}

static CVI_S32 _vi_proc_unmap(void)
{
	if (gViCtx == NULL) {
		CVI_TRACE_VI(CVI_DBG_DEBUG, "VI proc no need to unmap\n");
		return CVI_SUCCESS;
	}

	gViCtx = NULL;

	return CVI_SUCCESS;
}

static CVI_S32 _vi_chn_enable_mirror_flip(VI_CHN ViChn)
{
	ISP_SNS_MIRRORFLIP_TYPE_E eChnMirrorFlip;
	VI_DEV ViDev = 0;

	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (gViCtx->chnAttr[ViChn].bMirror && gViCtx->chnAttr[ViChn].bFlip)
		eChnMirrorFlip = ISP_SNS_MIRROR_FLIP;
	else if (gViCtx->chnAttr[ViChn].bMirror)
		eChnMirrorFlip = ISP_SNS_MIRROR;
	else if (gViCtx->chnAttr[ViChn].bFlip)
		eChnMirrorFlip = ISP_SNS_FLIP;
	else
		eChnMirrorFlip = ISP_SNS_NORMAL;

	if (eChnMirrorFlip != ISP_SNS_NORMAL && !s_pfnDevMirrorFlip[ViChn]) {
		CVI_TRACE_VI(CVI_DBG_ERR, "VI chn mirror/flip do not support this sensor.");
		return CVI_ERR_VI_NOT_SUPPORT;
	}

	if ((CVI_U32)ViChn < gViCtx->devAttr[0].chn_num)
		ViDev = 0;
	else
		ViDev = 1;

	if (s_pfnDevMirrorFlip[ViDev])
		s_pfnDevMirrorFlip[ViDev](ViDev, eChnMirrorFlip);

	return CVI_SUCCESS;
}

CVI_S32 _vi_update_rotation_mesh(VI_CHN ViChn, ROTATION_E enRotation)
{
	struct vi_chn_rot_cfg cfg;
	CVI_S32 fd = get_vi_fd();

	cfg.ViChn = ViChn;
	cfg.enRotation = enRotation;
	if (vi_sdk_set_chn_rotation(fd, &cfg) != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "VI Set Chn(%d) Rotation(%d) fail\n", ViChn, enRotation);
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

static CVI_S32 _vi_update_ldc_mesh(VI_PIPE ViPipe, VI_CHN ViChn,
	const VI_LDC_ATTR_S *pstLDCAttr, ROTATION_E enRotation)
{
	CVI_U64 paddr, paddr_old;
	CVI_VOID *vaddr, *vaddr_old;
	struct cvi_gdc_mesh *pmesh = &g_vi_mesh[ViChn];
	CVI_S32 s32Ret;

	if (!pstLDCAttr->bEnable) {
		pthread_mutex_lock(&pmesh->lock);
		gViCtx->stLDCAttr[ViChn] = *pstLDCAttr;
		pthread_mutex_unlock(&pmesh->lock);

		if (gViCtx->enRotation[ViChn] != ROTATION_0)
			return _vi_update_rotation_mesh(ViChn, gViCtx->enRotation[ViChn]);
		else
			return CVI_SUCCESS;
	}

	s32Ret = CVI_GDC_GenLDCMesh(ALIGN(gViCtx->chnAttr[ViChn].stSize.u32Width, DEFAULT_ALIGN)
		, ALIGN(gViCtx->chnAttr[ViChn].stSize.u32Height, DEFAULT_ALIGN)
		, &pstLDCAttr->stAttr, "vi_mesh", &paddr, &vaddr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Chn(%d) gen mesh fail\n", ViChn);
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
	//gViCtx->stLDCAttr[ViChn] = *pstLDCAttr;
	//gViCtx->enRotation[ViChn] = enRotation;
	pthread_mutex_unlock(&pmesh->lock);

	if (paddr_old && paddr_old != DEFAULT_MESH_PADDR)
		CVI_SYS_IonFree(paddr_old, vaddr_old);

	CVI_TRACE_VI(CVI_DBG_DEBUG, "ViPipe(%d) ViChn(%d) mesh base(%#"PRIx64") vaddr(%p)\n"
		, ViPipe, ViChn, paddr, vaddr);

	CVI_S32 fd = get_vi_fd();
	struct vi_chn_ldc_cfg cfg;

	cfg.ViChn = ViChn;
	cfg.enRotation = enRotation;
	cfg.stLDCAttr = *pstLDCAttr;
	cfg.meshHandle = paddr;
	if (vi_sdk_set_chn_ldc(fd, &cfg) != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "VI Set Chn(%d) LDC fail\n", ViChn);
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 _cvi_vi_freeIonBuf(void)
{
	CVI_S32 ret = CVI_SUCCESS;

	if (gViCtx->reserved_mem == 0 && dmaBufpAddr) {
		ret = CVI_SYS_IonFree(dmaBufpAddr, NULL);
		if (ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "Free Ion dmaBufAddr failed\n");
			return CVI_ERR_SYS_ILLEGAL_PARAM;
		}
	}

	CVI_TRACE_VI(CVI_DBG_DEBUG, "VI FreeIonBuf success\n");

	dmaBufpAddr = 0;
	return ret;
}

CVI_S32 _cvi_vi_getIonBuf(void)
{
	CVI_U64 pAddr = 0;
	CVI_S32 ret = CVI_SUCCESS;
	CVI_U32 size = 0;
	struct vdev *d;
	CVI_S32 BaseAlign = 0x1000;
	struct cvi_vi_dma_buf_info info = {.paddr = 0, .size = 0};

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		return CVI_ERR_VI_SYS_NOTREADY;
	}

	//ioctl to isp driver to get size
	ret = vi_get_dma_size(d->fd, &size);
	if (ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_get_dma_size ioctl failed\n");
		return CVI_ERR_VI_NOMEM;
	}

	if (size == 0) {
		for (int i = 0; i < gViCtx->total_dev_num; i++) {
			if (gViCtx->devAttr[i].enInputDataType == VI_DATA_TYPE_RGB) {
				CVI_TRACE_VI(CVI_DBG_ERR, "get dma size(%d) error\n", size);
				return CVI_ERR_VI_NOMEM;
			}
		}

		dmaBufpAddr = 0;
		CVI_TRACE_VI(CVI_DBG_DEBUG, "yuv sensor not need dma size\n");
		return CVI_SUCCESS;
	}

	if (gViCtx->reserved_mem) {
		if (gViCtx->reserved_size < size + BaseAlign) {
			CVI_TRACE_VI(CVI_DBG_ERR, "expect dma size > (%u) but now (%u)\n",
					size + BaseAlign, gViCtx->reserved_size);
			return CVI_ERR_VI_NOMEM;
		} else {
			pAddr = gViCtx->reserved_mem;
		}
	} else {
		ret = CVI_SYS_IonAlloc_Cached(&pAddr, NULL, "VI_DMA_BUF", size + BaseAlign);
		if (ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "VI ion alloc size(%d) failed.\n", size);
			return CVI_ERR_VI_NOMEM;
		}
	}

	//set paddr and size to isp driver
	info.paddr = ALIGN(pAddr, 0x1000);
	info.size  = size;
	ret = vi_set_dma_buf_info(d->fd, &info);
	if (ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_set_dma_buf_info ioctl failed\n");
		CVI_SYS_IonFree(pAddr, NULL);
		return CVI_ERR_VI_NOT_SUPPORT;
	}

	dmaBufpAddr = pAddr;

	CVI_TRACE_VI(CVI_DBG_INFO, "VI ion alloc size(%d) success\n", info.size);

	return ret;
}

/**************************************************************************
 *   Internal APIs for other modules
 **************************************************************************/

CVI_S32 CVI_VI_Suspend(void)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_U8  i = 0;

	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	gViCtx->vi_stt = VI_SUSPEND;

	memcpy(&vi_ctx_bak, gViCtx, sizeof(struct cvi_vi_ctx));

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		s32Ret = CVI_VI_DisableChn(0, i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_DisableChn failed with %#x!\n", s32Ret);
			return s32Ret;
		}
		/* suspend sensor. */
		if (apstViPm[i].stOps.pfnSnsSuspend) {
			s32Ret = apstViPm[i].stOps.pfnSnsSuspend(apstViPm[i].pvData);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "sensor[%d] suspend failed with %#x!\n", i, s32Ret);
				return s32Ret;
			}
		}
		/* suspend mipi. */
		if (apstViPm[i].stOps.pfnMipiSuspend) {
			s32Ret = apstViPm[i].stOps.pfnMipiSuspend(apstViPm[i].pvData);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "mipi[%d] suspend failed with %#x!\n", i, s32Ret);
				return s32Ret;
			}
		}
	}

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		s32Ret = CVI_VI_DestroyPipe(i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_DestroyPipe failed with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		s32Ret  = CVI_VI_DisableDev(i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_DisableDev failed with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	//CVI_SYS_VI_Close();

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_Resume(void)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_U8  i = 0;
	VI_DEV_ATTR_S	    stViDevAttr;
	VI_PIPE_ATTR_S	    stPipeAttr;
	VI_CHN_ATTR_S	    stChnAttr;
	struct vdev *d;

	//CVI_SYS_VI_Open();

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi dev(%d) state(%d) incorrect.", 0, d->state);
		return CVI_ERR_VI_SYS_NOTREADY;
	}

	s32Ret = vi_set_clk(d->fd, 1);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Set isp clk ioctl\n");
		return CVI_FAILURE;
	}

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		/* resume mipi. */
		if (apstViPm[i].stOps.pfnMipiResume) {
			s32Ret = apstViPm[i].stOps.pfnMipiResume(apstViPm[i].pvData);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "mipi[%d] resume failed with %#x!\n", i, s32Ret);
				return s32Ret;
			}
		}
		/* resume sensor. */
		if (apstViPm[i].stOps.pfnSnsResume) {
			s32Ret = apstViPm[i].stOps.pfnSnsResume(apstViPm[i].pvData);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "sensor[%d] resume failed with %#x!\n", i, s32Ret);
				return s32Ret;
			}
		}

		memcpy(&stViDevAttr, &vi_ctx_bak.devAttr[i], sizeof(VI_DEV_ATTR_S));

		s32Ret = CVI_VI_SetDevAttr(i, &stViDevAttr);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_SetDevAttr failed with %#x!\n", s32Ret);
			return s32Ret;
		}

		s32Ret = CVI_VI_EnableDev(i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_EnableDev failed with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		memcpy(&stPipeAttr, &vi_ctx_bak.pipeAttr[i], sizeof(VI_PIPE_ATTR_S));

		s32Ret = CVI_VI_CreatePipe(i, &stPipeAttr);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_CreatePipe failed with %#x!\n", s32Ret);
			return s32Ret;
		}

		s32Ret = CVI_VI_StartPipe(i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_StartPipe failed with %#x!\n", s32Ret);
			return s32Ret;
		}
	}

	for (i = 0; i < vi_ctx_bak.total_dev_num; i++) {
		memcpy(&stChnAttr, &vi_ctx_bak.chnAttr[i], sizeof(VI_CHN_ATTR_S));

		s32Ret = CVI_VI_SetChnAttr(0, i, &stChnAttr);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_SetChnAttr failed with %#x!\n", s32Ret);
			return CVI_FAILURE;
		}

		s32Ret = CVI_VI_EnableChn(0, i);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_VI_EnableChn failed with %#x!\n", s32Ret);
			return CVI_FAILURE;
		}
	}

	//gViCtx->vi_stt = vi_prc_ctx->vi_stt = VI_RUNNING;

	return CVI_SUCCESS;
}

CVI_VOID CVI_VI_SetMotionLV(struct mlv_info mlevel_i)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	struct vdev *d;
	struct mlv_info_s mlv_i_tmp;

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		s32Ret = CVI_FAILURE;
	}

	if (s32Ret == CVI_SUCCESS) {
		mlv_i_tmp.sensor_num = mlevel_i.sensor_num;
		mlv_i_tmp.frm_num    = mlevel_i.frm_num;
		mlv_i_tmp.mlv        = mlevel_i.mlv;
		memcpy(mlv_i_tmp.mtable, mlevel_i.mtable, MO_TBL_SIZE);

		s32Ret = vi_sdk_set_motion_lv(d->fd, &mlv_i_tmp);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_motion_lv ioctl failed\n");
		}
	}
}

CVI_VOID CVI_VI_SET_DIS_INFO(struct dis_info dis_i)
{
	CVI_U32 frm_num = dis_i.frm_num;
	CVI_U8 snr_num = dis_i.sensor_num;
	CVI_U8 pos = dis_i_w_pos[snr_num] % MAX_DIS_NUM;
	CVI_U8 i;

	for (i = 0; i < MAX_DIS_NUM; i++) {
		if (dis_i_frm_num[snr_num][i] == frm_num) {
			dis_i_data[snr_num][i] = dis_i.dis_i;
			return;
		}
	}

	dis_i_frm_num[snr_num][pos] = frm_num;

	dis_i_data[snr_num][pos] = dis_i.dis_i;

	dis_i_w_pos[snr_num]++;
}

CVI_S32 CVI_VI_SetBypassFrm(CVI_U32 snr_num, CVI_U8 bypass_num)
{
	CHECK_VI_CHNID_VALID(snr_num);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	gViCtx->bypass_frm[snr_num] = bypass_num;

	return CVI_SUCCESS;
}

/**************************************************************************
 *   Public APIs.
 **************************************************************************/
CVI_S32 CVI_VI_SetDevNum(CVI_U32 devNum)
{
	UNUSED(devNum);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetDevNum(CVI_U32 *devNum)
{
	CHECK_VI_NULL_PTR(devNum);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*devNum = gViCtx->total_dev_num;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_EnablePatt(VI_PIPE ViPipe)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

	CHECK_VI_PIPEID_VALID(ViPipe);

	fd = get_vi_fd();

	s32Ret = vi_sdk_enable_pattern(fd, 1);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "isp_set_pattern ioctl\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetDevAttr(VI_DEV ViDev, const VI_DEV_ATTR_S *pstDevAttr)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;
	VI_DEV_ATTR_S devAttr;

	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pstDevAttr);

	fd = get_vi_fd();

	if (ViDev == 0 && (gViCtx == NULL)) {
		if (_vi_proc_mmap() != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "_vi_proc_mmap failed.\n");
			return CVI_ERR_VI_BUF_EMPTY;
		}

		for (int i = 0; i < VI_MAX_CHN_NUM; ++i) {
			gViCtx->enRotation[i] = ROTATION_0;
			gViCtx->stLDCAttr[i].bEnable = CVI_FALSE;
		}

		for (int i = 0; i < VI_MAX_CHN_NUM; ++i)
			for (int j = 0; j < VI_MAX_EXTCHN_BIND_PER_CHN; ++j)
				gViCtx->chn_bind[i][j] = VI_INVALID_CHN;

		if (pstDevAttr->phy_addr != 0 && pstDevAttr->phy_size != 0) {
			gViCtx->reserved_mem = pstDevAttr->phy_addr;
			gViCtx->reserved_size = pstDevAttr->phy_size;
		}
	}

	devAttr = *pstDevAttr;

	s32Ret = vi_sdk_set_dev_attr(fd, ViDev, &devAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "setDevAttr ioctl failed\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetDevAttr(VI_DEV ViDev, VI_DEV_ATTR_S *pstDevAttr)
{
	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pstDevAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*pstDevAttr = gViCtx->devAttr[ViDev];

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_EnableDev(VI_DEV ViDev)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

	CHECK_VI_DEVID_VALID(ViDev);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	fd = get_vi_fd();

	if (gViCtx->devAttr[ViDev].stSize.u32Width == 0 &&
		gViCtx->devAttr[ViDev].stSize.u32Height == 0) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Call SetDevAttr first\n");
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	}

	s32Ret = vi_sdk_enable_dev(fd, ViDev);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "enable_dev ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_DisableDev(VI_DEV ViDev)
{
	CVI_S32 ret = CVI_SUCCESS;
	CVI_U8 i = 0;
	CVI_U8 all_dev_disabled = true;

	CHECK_VI_DEVID_VALID(ViDev);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (gViCtx->isDevEnable[ViDev] == CVI_FALSE) {
		CVI_TRACE_VI(CVI_DBG_INFO, "vi dev(%d) is already disabled.", ViDev);
		return CVI_SUCCESS;
	}

	gViCtx->isDevEnable[ViDev] = false;

	for (i = 0; i < gViCtx->total_dev_num; i++) {
		if (gViCtx->isDevEnable[i]) {
			all_dev_disabled = false;
			break;
		}
	}

	if (all_dev_disabled && gViCtx->vi_stt != VI_SUSPEND) {
		ret = _vi_proc_unmap();
		if (ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "_vi_proc_unmap failed.\n");
			return CVI_ERR_VI_FAILED_NOT_DISABLED;
		}
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetDevTimingAttr(VI_DEV ViDev, const VI_DEV_TIMING_ATTR_S *pstTimingAttr)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;
	VI_DEV_TIMING_ATTR_S stTimingAttr;

	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pstTimingAttr);

	fd = get_vi_fd();

	stTimingAttr = *pstTimingAttr;
	s32Ret = vi_sdk_set_dev_timing_attr(fd, ViDev, &stTimingAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_dev_timing_attr ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetDevTimingAttr(VI_DEV ViDev, VI_DEV_TIMING_ATTR_S *pstTimingAttr)
{
	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pstTimingAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (gViCtx->isDevEnable[ViDev] == CVI_FALSE) {
		CVI_TRACE_VI(CVI_DBG_ERR, "EnableDev first\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	if (gViCtx->stTimingAttr[ViDev].bEnable == CVI_FALSE) {
		CVI_TRACE_VI(CVI_DBG_ERR, "SetDevTimingAttr first\n");
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	}

	*pstTimingAttr = gViCtx->stTimingAttr[ViDev];
	return CVI_SUCCESS;
}

/* 2 for vi pipe */
CVI_S32 CVI_VI_CreatePipe(VI_PIPE ViPipe, const VI_PIPE_ATTR_S *pstPipeAttr)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;
	VI_PIPE_ATTR_S stPipeAttr;

	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstPipeAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	fd = get_vi_fd();

	if (gViCtx->isPipeCreated[ViPipe] == CVI_TRUE) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Pipe(%d) has been created\n", ViPipe);
		return CVI_ERR_VI_PIPE_EXIST;
	}

	stPipeAttr = *pstPipeAttr;

	s32Ret = vi_sdk_create_pipe(fd, ViPipe, &stPipeAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_create_pipe ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_DestroyPipe(VI_PIPE ViPipe)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	gViCtx->isPipeCreated[ViPipe] = false;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_StartPipe(VI_PIPE ViPipe)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

	CHECK_VI_PIPEID_VALID(ViPipe);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	fd = get_vi_fd();

	s32Ret = vi_sdk_start_pipe(fd, ViPipe);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_start_pipe ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return s32Ret;
}

CVI_S32 CVI_VI_StopPipe(VI_PIPE ViPipe)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetPipeAttr(VI_PIPE ViPipe, const VI_PIPE_ATTR_S *pstPipeAttr)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstPipeAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	gViCtx->pipeAttr[ViPipe] = *pstPipeAttr;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetPipeAttr(VI_PIPE ViPipe, VI_PIPE_ATTR_S *pstPipeAttr)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstPipeAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	if (gViCtx->pipeAttr[ViPipe].u32MaxW == 0 &&
		gViCtx->pipeAttr[ViPipe].u32MaxH == 0) {
		CVI_TRACE_VI(CVI_DBG_ERR, "SetPipeAttr first\n");
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	}

	*pstPipeAttr = gViCtx->pipeAttr[ViPipe];

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetPipeDumpAttr(VI_PIPE ViPipe, const VI_DUMP_ATTR_S *pstDumpAttr)
{
#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_setpipedumpattr(ViPipe, pstDumpAttr);
	}
#endif

	VDEV_CLOSED_CHK(VDEV_TYPE_ISP, 0);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstDumpAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	memset(&gViCtx->dumpAttr[ViPipe], 0, sizeof(VI_DUMP_ATTR_S));
	memcpy(&gViCtx->dumpAttr[ViPipe], pstDumpAttr, sizeof(VI_DUMP_ATTR_S));

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetPipeDumpAttr(VI_PIPE ViPipe, VI_DUMP_ATTR_S *pstDumpAttr)
{
#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_getpipedumpattr(ViPipe, pstDumpAttr);
	}
#endif

	VDEV_CLOSED_CHK(VDEV_TYPE_ISP, 0);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstDumpAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	memcpy(pstDumpAttr, &gViCtx->dumpAttr[ViPipe], sizeof(VI_DUMP_ATTR_S));

	return CVI_SUCCESS;
}

// get bayer from preraw
CVI_S32 CVI_VI_GetPipeFrame(VI_PIPE ViPipe, VIDEO_FRAME_INFO_S *pstFrameInfo, CVI_S32 s32MilliSec)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_getpipeframe(ViPipe, pstFrameInfo, s32MilliSec);
	}
#endif

	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstFrameInfo);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (gViCtx->dumpAttr[ViPipe].bEnable == CVI_FALSE) {
		CVI_TRACE_VI(CVI_DBG_ERR, "SetPipeDumpAttr first\n");
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	}

	if (gViCtx->dumpAttr[ViPipe].enDumpType == VI_DUMP_TYPE_RAW) {
		fd = get_vi_fd();

		s32Ret = vi_sdk_get_pipe_frame(fd, ViPipe, pstFrameInfo, s32MilliSec);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_pipe_frame ioctl failed\n");
			return CVI_ERR_VI_FAILED_NOT_ENABLED;
		}
	} else if (gViCtx->dumpAttr[ViPipe].enDumpType == VI_DUMP_TYPE_YUV) {
		//Use the getchnframe to get yuv frame
	} else if (gViCtx->dumpAttr[ViPipe].enDumpType == VI_DUMP_TYPE_IR) {
		CVI_TRACE_VI(CVI_DBG_ERR, "IR dump is not supported.\n");
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_ReleasePipeFrame(VI_PIPE ViPipe, const VIDEO_FRAME_INFO_S *pstFrameInfo)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	VIDEO_FRAME_INFO_S stVideoFrame;

#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_releasepipeframe(ViPipe, pstFrameInfo);
	}
#endif

	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstFrameInfo);

	fd = get_vi_fd();

	stVideoFrame = *pstFrameInfo;
	s32Ret = vi_sdk_release_pipe_frame(fd, ViPipe, &stVideoFrame);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_release_pipe_frame ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_StartSmoothRawDump(const VI_SMOOTH_RAW_DUMP_INFO_S *pstDumpInfo)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

	CHECK_VI_NULL_PTR(pstDumpInfo);
	CHECK_VI_NULL_PTR(pstDumpInfo->phy_addr_list);
	CHECK_VI_PIPEID_VALID(pstDumpInfo->ViPipe);
	CHECK_VI_PIPE_CREATED(pstDumpInfo->ViPipe);

	VI_PIPE ViPipe = pstDumpInfo->ViPipe;
	CVI_U8 frm_num = 0;
	CVI_U64 phy_addr = 0;
	struct cvi_vip_isp_smooth_raw_param param;
	struct cvi_vip_isp_raw_blk *raw_blk = CVI_NULL;

	fd = get_vi_fd();

	if (pstDumpInfo->u8BlkCnt < 2) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Need two ring buffer at least, now is %d\n", pstDumpInfo->u8BlkCnt);
		return CVI_ERR_VI_INVALID_PARA;
	}

	if ((gViCtx->devAttr[ViPipe].stWDRAttr.enWDRMode == WDR_MODE_2To1_LINE) ||
		(gViCtx->devAttr[ViPipe].stWDRAttr.enWDRMode == WDR_MODE_2To1_FRAME) ||
		(gViCtx->devAttr[ViPipe].stWDRAttr.enWDRMode == WDR_MODE_2To1_FRAME_FULL_RATE)) {
		frm_num = 2;
	} else {
		frm_num = 1;
	}

	frm_num = (pstDumpInfo->u8BlkCnt) * frm_num;

	raw_blk = calloc(frm_num, sizeof(struct cvi_vip_isp_raw_blk));
	if (raw_blk == CVI_NULL) {
		CVI_TRACE_VI(CVI_DBG_ERR, "malloc raw_blk failed\n");
		return CVI_ERR_VI_NOMEM;
	}

	for (CVI_U8 i = 0; i < frm_num; i++) {
		phy_addr = *(pstDumpInfo->phy_addr_list + i);
		if (phy_addr == 0) {
			if (raw_blk != CVI_NULL) {
				free(raw_blk);
				raw_blk = CVI_NULL;
			}
			CVI_TRACE_VI(CVI_DBG_ERR, "phy_addr is invalid\n");
			return CVI_ERR_VI_INVALID_PARA;
		}

		(raw_blk + i)->raw_dump.phy_addr = phy_addr;
		CVI_TRACE_VI(CVI_DBG_DEBUG, "i=%d, phy_paddr(%#"PRIx64")\n", i, (raw_blk+i)->raw_dump.phy_addr);

		// set rawdump crop info
		(raw_blk + i)->crop_x = pstDumpInfo->stCropRect.s32X;
		(raw_blk + i)->crop_y = pstDumpInfo->stCropRect.s32Y;
		(raw_blk + i)->src_w = pstDumpInfo->stCropRect.u32Width;
		(raw_blk + i)->src_h = pstDumpInfo->stCropRect.u32Height;
		CVI_TRACE_VI(CVI_DBG_DEBUG, "i (%d), crop_x(%d), crop_y(%d), src_w(%d), src_h(%d)\n",
			i, (raw_blk + i)->crop_x, (raw_blk + i)->crop_y,
			(raw_blk + i)->src_w, (raw_blk + i)->src_h);
	}

	param.raw_num = ViPipe;
	param.frm_num = frm_num;
	param.raw_blk = raw_blk;

	s32Ret = vi_sdk_start_smooth_rawdump(fd, ViPipe, &param);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_start_smooth_rawdump ioctl failed\n");
		free(raw_blk);
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	if (raw_blk != CVI_NULL) {
		free(raw_blk);
		raw_blk = CVI_NULL;
	}

	return s32Ret;
}

CVI_S32 CVI_VI_StopSmoothRawDump(const VI_SMOOTH_RAW_DUMP_INFO_S *pstDumpInfo)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

	CHECK_VI_NULL_PTR(pstDumpInfo);
	CHECK_VI_PIPEID_VALID(pstDumpInfo->ViPipe);
	CHECK_VI_PIPE_CREATED(pstDumpInfo->ViPipe);

	VI_PIPE ViPipe = pstDumpInfo->ViPipe;
	struct cvi_vip_isp_smooth_raw_param param;

	fd = get_vi_fd();

	param.raw_num = ViPipe;

	s32Ret = vi_sdk_stop_smooth_rawdump(fd, ViPipe, &param);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_stop_smooth_rawdump ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetSmoothRawDump(VI_PIPE ViPipe, VIDEO_FRAME_INFO_S *pstVideoFrame, CVI_S32 s32MilliSec)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

	CHECK_VI_NULL_PTR(pstVideoFrame);

	fd = get_vi_fd();

	s32Ret = vi_sdk_get_smooth_rawdump(fd, ViPipe, pstVideoFrame, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_smooth_rawdump ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_PutSmoothRawDump(VI_PIPE ViPipe, const VIDEO_FRAME_INFO_S *pstVideoFrame)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	VIDEO_FRAME_INFO_S stVideoFrame[2];

	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstVideoFrame);

	fd = get_vi_fd();

	stVideoFrame[0] = pstVideoFrame[0];
	stVideoFrame[1] = pstVideoFrame[1];
	s32Ret = vi_sdk_put_smooth_rawdump(fd, ViPipe, stVideoFrame);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_put_smooth_rawdump ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_QueryPipeStatus(VI_PIPE ViPipe, VI_PIPE_STATUS_S *pstStatus)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstStatus);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;
	CHECK_VI_PIPE_CREATED(ViPipe);

	pstStatus->bEnable = gViCtx->is_enable[0];
	pstStatus->stSize.u32Height = gViCtx->pipeAttr[ViPipe].u32MaxH;
	pstStatus->stSize.u32Width = gViCtx->pipeAttr[ViPipe].u32MaxW;
	pstStatus->u32FrameRate = gViCtx->pipeAttr[ViPipe].stFrameRate.s32SrcFrameRate;
	pstStatus->u32IntCnt = 0;//todo
	pstStatus->u32LostFrame = 0;//todo
	pstStatus->u32VbFail = 0;//todo

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetPipeFrameSource(VI_PIPE ViPipe, const VI_PIPE_FRAME_SOURCE_E enSource)
{
	CVI_S32 fd = -1;
	CVI_U32 s32Ret = CVI_SUCCESS;
	VI_PIPE_FRAME_SOURCE_E src = enSource;

	CHECK_VI_PIPEID_VALID(ViPipe);

	if (enSource < 0 || enSource >= VI_PIPE_FRAME_SOURCE_BUTT) {
		CVI_TRACE_VI(CVI_DBG_ERR, "enSource(%d)is invalid\n", enSource);
		return CVI_ERR_VI_INVALID_PARA;
	}

	fd = get_vi_fd();

	s32Ret = vi_sdk_set_pipe_frm_src(fd, ViPipe, &src);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_pipe_frm_src ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetPipeFrameSource(VI_PIPE ViPipe, VI_PIPE_FRAME_SOURCE_E *penSource)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(penSource);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*penSource = gViCtx->enSource[ViPipe];

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SendPipeRaw(CVI_U32 u32PipeNum, VI_PIPE PipeId[], const VIDEO_FRAME_INFO_S *pstVideoFrame[],
			   CVI_S32 s32MilliSec)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

	CHECK_VI_NULL_PTR(PipeId);
	CHECK_VI_NULL_PTR(pstVideoFrame);

	UNUSED(s32MilliSec);

	fd = get_vi_fd();

	for (CVI_U32 i = 0; i < u32PipeNum; ++i) {
		VI_PIPE pipeid = PipeId[i];
		VIDEO_FRAME_INFO_S stVideoFrm = *pstVideoFrame[i];

		s32Ret = vi_sdk_send_pipe_raw(fd, pipeid, &stVideoFrm);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_send_pipe_raw ioctl failed\n");
			return CVI_ERR_VI_FAILED_NOT_ENABLED;
		}
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetPipeFd(VI_PIPE ViPipe)
{
	CVI_S32 fd = -1;

	CHECK_VI_PIPEID_VALID(ViPipe);

	fd = get_vi_fd();

	if (fd <= 0) {
		CVI_TRACE_VI(CVI_DBG_ERR, "Get pipe fd fail\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_CloseFd(void)
{
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetPipeCrop(VI_PIPE ViPipe, const CROP_INFO_S *pstCropInfo)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;
	VI_CROP_INFO_S cropInfo;

	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstCropInfo);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	fd = get_vi_fd();

	if (pstCropInfo->stRect.s32X % 2 || pstCropInfo->stRect.s32Y % 2 ||
		pstCropInfo->stRect.u32Width % 2 || pstCropInfo->stRect.u32Height % 2) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)_y(%d)_w(%d)_h(%d) must be multiple of 2.\n",
					pstCropInfo->stRect.s32X,
					pstCropInfo->stRect.s32Y,
					pstCropInfo->stRect.u32Width,
					pstCropInfo->stRect.u32Height);
		return CVI_ERR_VI_INVALID_PARA;
	}

	if (pstCropInfo->stRect.s32X < 0 || pstCropInfo->stRect.s32Y < 0) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)_y(%d) is invalid.\n",
					pstCropInfo->stRect.s32X,
					pstCropInfo->stRect.s32Y);
		return CVI_ERR_VI_INVALID_PARA;
	}

	if ((pstCropInfo->stRect.s32X + pstCropInfo->stRect.u32Width) >
		gViCtx->pipeAttr[ViPipe].u32MaxW ||
		(pstCropInfo->stRect.s32Y + pstCropInfo->stRect.u32Height) >
		gViCtx->pipeAttr[ViPipe].u32MaxH) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)+w(%d) or y(%d)+h(%d) is bigger than chn_w(%d)_h(%d)\n",
					pstCropInfo->stRect.s32X,
					pstCropInfo->stRect.u32Width,
					pstCropInfo->stRect.s32Y,
					pstCropInfo->stRect.u32Height,
					gViCtx->pipeAttr[ViPipe].u32MaxW,
					gViCtx->pipeAttr[ViPipe].u32MaxH);
		return CVI_ERR_VI_INVALID_PARA;
	}

	gViCtx->pipeCrop[ViPipe] = *pstCropInfo;
	if (gViCtx->pipeCrop[ViPipe].bEnable == CVI_TRUE) {
		cropInfo.bEnable = pstCropInfo->bEnable;
		cropInfo.stCropRect = pstCropInfo->stRect;
		s32Ret = vi_sdk_set_chn_crop(fd, ViPipe, 0, &cropInfo);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_chn_crop ioctl failed\n");
			return CVI_FAILURE;
		}
	}
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetPipeCrop(VI_PIPE ViPipe, CROP_INFO_S *pstCropInfo)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstCropInfo);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*pstCropInfo = gViCtx->pipeCrop[ViPipe];

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn, VI_CHN_ATTR_S *pstChnAttr)
{
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstChnAttr);

	fd = get_vi_fd();

	if (pstChnAttr->stFrameRate.s32SrcFrameRate != pstChnAttr->stFrameRate.s32DstFrameRate)
		CVI_TRACE_VI(CVI_DBG_WARN, "FrameRate ctrl, src(%d) dst(%d), not support yet.\n"
				, pstChnAttr->stFrameRate.s32SrcFrameRate, pstChnAttr->stFrameRate.s32DstFrameRate);

	s32Ret = vi_sdk_set_chn_attr(fd, ViPipe, ViChn, pstChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_chn_attr ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn, VI_CHN_ATTR_S *pstChnAttr)
{
	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstChnAttr);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*pstChnAttr = gViCtx->chnAttr[ViChn];
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

	CHECK_VI_PIPEID_VALID(ViPipe);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (ViChn >= (VI_MAX_CHN_NUM + VI_MAX_EXT_CHN_NUM)) {
		CVI_TRACE_VI(CVI_DBG_ERR, " invalid chn-id(%d)\n", ViChn);
		return CVI_ERR_VI_INVALID_CHNID;
	}

	fd = get_vi_fd();

	if (gViCtx->is_enable[ViChn] == CVI_TRUE) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi chn(%d) is already enabled.\n", ViChn);
		return CVI_ERR_VI_FAILED_NOT_DISABLED;
	}

	if (ViChn >= VI_EXT_CHN_START) {
		VI_EXT_CHN_ATTR_S *pstExtChnAttr = &gViCtx->stExtChnAttr[ViChn - VI_EXT_CHN_START];

		gViCtx->is_enable[ViChn] = CVI_TRUE;
		gViCtx->chn_bind[pstExtChnAttr->s32BindChn][0] = ViChn;
	} else {
		if (ViChn < VI_MAX_CHN_NUM)
			_vi_chn_enable_mirror_flip(ViChn);

		if (gViCtx->total_dev_num != ViChn + 1)
			return CVI_SUCCESS;

		s32Ret = _cvi_vi_getIonBuf();
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "VI getIonBuf is failed\n");
			return CVI_ERR_SYS_NOMEM;
		}

		s32Ret = vi_sdk_enable_chn(fd, ViPipe, ViChn);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_enable_chn ioctl failed\n");
			return CVI_ERR_VI_FAILED_NOT_ENABLED;
		}

		{
			//struct sched_param param;
			//pthread_attr_t attr;

			//param.sched_priority = 85;

			//pthread_attr_init(&attr);
			//pthread_attr_setschedpolicy(&attr, SCHED_RR);
			//pthread_attr_setschedparam(&attr, &param);
			//pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

			//pthread_create(&gViDbgTH.vi_dbg_thread, &attr, (void *)vi_dbg_handler, NULL);
		}
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_U8  i = 0;

	CHECK_VI_PIPEID_VALID(ViPipe);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (ViChn >= (VI_MAX_CHN_NUM + VI_MAX_EXT_CHN_NUM)) {
		CVI_TRACE_VI(CVI_DBG_ERR, " invalid chn-id(%d)\n", ViChn);
		return CVI_ERR_VI_INVALID_CHNID;
	}

	fd = get_vi_fd();

	if (gViCtx->is_enable[ViChn] == CVI_FALSE) {
		CVI_TRACE_VI(CVI_DBG_INFO, "vi chn(%d) is already disabled.", ViChn);
		return CVI_SUCCESS;
	}

	if (ViChn < VI_MAX_PHY_CHN_NUM) {
		CVI_U8 all_chn_disabled = true;

		gViCtx->is_enable[ViChn] = CVI_FALSE;

		for (i = 0; i < gViCtx->total_chn_num; i++) {
			if (gViCtx->is_enable[i]) {
				all_chn_disabled = false;
				break;
			}
		}

		if (all_chn_disabled) {
			gViDbgTH.th_enable = CVI_FALSE;

			s32Ret = vi_sdk_disable_chn(fd, ViPipe, ViChn);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_disable_chn ioctl failed\n");
				return CVI_ERR_VI_FAILED_NOT_ENABLED;
			}

			//pthread_join(gViDbgTH.vi_dbg_thread, NULL);

			s32Ret = _cvi_vi_freeIonBuf();
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "VI freeIonBuf is failed\n");
				return CVI_ERR_SYS_ILLEGAL_PARAM;
			}
		}
	} else if (ViChn >= VI_EXT_CHN_START) {
		VI_EXT_CHN_ATTR_S *pstExtChnAttr = &gViCtx->stExtChnAttr[ViChn - VI_EXT_CHN_START];

		gViCtx->is_enable[ViChn] = CVI_FALSE;
		gViCtx->chn_bind[pstExtChnAttr->s32BindChn][0] = VI_INVALID_CHN;
		//vi_ctx.stFishEyeAttr[ViChn - VI_EXT_CHN_START].bEnable = CVI_FALSE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetChnCrop(VI_PIPE ViPipe, VI_CHN ViChn, const VI_CROP_INFO_S  *pstCropInfo)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;
	VI_CROP_INFO_S cropInfo;

	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstCropInfo);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	fd = get_vi_fd();

	if (pstCropInfo->stCropRect.s32X % 2 || pstCropInfo->stCropRect.s32Y % 2 ||
		pstCropInfo->stCropRect.u32Width % 2 || pstCropInfo->stCropRect.u32Height % 2) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)_y(%d)_w(%d)_h(%d) must be multiple of 2.\n",
					pstCropInfo->stCropRect.s32X,
					pstCropInfo->stCropRect.s32Y,
					pstCropInfo->stCropRect.u32Width,
					pstCropInfo->stCropRect.u32Height);
		return CVI_ERR_VI_INVALID_PARA;
	}

	if (pstCropInfo->stCropRect.s32X < 0 || pstCropInfo->stCropRect.s32Y < 0) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)_y(%d) is invalid.\n",
					pstCropInfo->stCropRect.s32X,
					pstCropInfo->stCropRect.s32Y);
		return CVI_ERR_VI_INVALID_PARA;
	}

	if ((pstCropInfo->stCropRect.s32X + pstCropInfo->stCropRect.u32Width) >
		gViCtx->chnAttr[ViChn].stSize.u32Width ||
		(pstCropInfo->stCropRect.s32Y + pstCropInfo->stCropRect.u32Height) >
		gViCtx->chnAttr[ViChn].stSize.u32Height) {
		CVI_TRACE_VI(CVI_DBG_ERR, "crop_x(%d)+w(%d) or y(%d)+h(%d) is bigger than chn_w(%d)_h(%d)\n",
					pstCropInfo->stCropRect.s32X,
					pstCropInfo->stCropRect.u32Width,
					pstCropInfo->stCropRect.s32Y,
					pstCropInfo->stCropRect.u32Height,
					gViCtx->chnAttr[ViChn].stSize.u32Width,
					gViCtx->chnAttr[ViChn].stSize.u32Height);
		return CVI_ERR_VI_INVALID_PARA;
	}

	gViCtx->chnCrop[ViChn] = *pstCropInfo;

	if (gViCtx->chnCrop[ViChn].bEnable == CVI_TRUE) {
		cropInfo = *pstCropInfo;
		s32Ret = vi_sdk_set_chn_crop(fd, ViPipe, ViChn, &cropInfo);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_set_chn_crop ioctl failed\n");
			return CVI_FAILURE;
		}
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetChnCrop(VI_PIPE ViPipe, VI_CHN ViChn, VI_CROP_INFO_S  *pstCropInfo)
{
	CVI_U32 s32Ret = CVI_SUCCESS;
	CVI_S32 fd = -1;

#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_getchncrop(ViPipe, ViChn, pstCropInfo);
	}
#endif

	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstCropInfo);

	fd = get_vi_fd();

	s32Ret = vi_sdk_get_chn_crop(fd, ViPipe, ViChn, pstCropInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_chn_crop ioctl failed\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetChnFrame(VI_PIPE ViPipe, VI_CHN ViChn, VIDEO_FRAME_INFO_S *pstFrameInfo, CVI_S32 s32MilliSec)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;

#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_getchnframe(ViPipe, ViChn, pstFrameInfo, s32MilliSec);
	}
#endif
	if (ViChn >= (VI_MAX_CHN_NUM + VI_MAX_EXT_CHN_NUM)) {
		CVI_TRACE_VI(CVI_DBG_ERR, " invalid chn-id(%d)\n", ViChn);
		return CVI_ERR_VI_INVALID_CHNID;
	}
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstFrameInfo);

	fd = get_vi_fd();

	s32Ret = vi_sdk_get_chn_frame(fd, ViPipe, ViChn, pstFrameInfo, s32MilliSec);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_chn_frame ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_ReleaseChnFrame(VI_PIPE ViPipe, VI_CHN ViChn, const VIDEO_FRAME_INFO_S *pstFrameInfo)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	VIDEO_FRAME_INFO_S stFrameInfo;

#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_releasechnframe(ViPipe, ViChn, pstFrameInfo);
	}
#endif
	CHECK_VI_NULL_PTR(pstFrameInfo);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);

	fd = get_vi_fd();

	stFrameInfo = *pstFrameInfo;
	s32Ret = vi_sdk_release_chn_frame(fd, ViPipe, ViChn, &stFrameInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_release_chn_frame ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_QueryChnStatus(VI_PIPE ViPipe, VI_CHN ViChn, VI_CHN_STATUS_S *pstChnStatus)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_NULL_PTR(pstChnStatus);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*pstChnStatus = gViCtx->chnStatus[ViChn];
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetChnRotation(VI_PIPE ViPipe, VI_CHN ViChn, const ROTATION_E enRotation)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_GDC_FMT(gViCtx->chnAttr[ViChn].enPixelFormat);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	struct cvi_gdc_mesh *pmesh = &g_vi_mesh[ViChn];

	if (enRotation == ROTATION_180) {
			CVI_TRACE_VI(CVI_DBG_ERR, "do not support rotation 180.\n");
			return CVI_ERR_VI_NOT_SUPPORT;
	} else if (enRotation >= ROTATION_MAX) {
		CVI_TRACE_VI(CVI_DBG_ERR, "invalid rotation(%d).\n", enRotation);
		return CVI_ERR_VI_INVALID_PARA;
	} else if (enRotation == gViCtx->enRotation[ViChn]) {
		CVI_TRACE_VI(CVI_DBG_INFO, "rotation(%d) not changed.\n", enRotation);
		return CVI_SUCCESS;
	} else if (!gViCtx->stLDCAttr[ViChn].bEnable && enRotation == ROTATION_0) {
		pthread_mutex_lock(&pmesh->lock);
		gViCtx->enRotation[ViChn] = enRotation;
		pthread_mutex_unlock(&pmesh->lock);
		return CVI_SUCCESS;
	}

	if (gViCtx->stLDCAttr[ViChn].bEnable)
		return _vi_update_ldc_mesh(ViPipe, ViChn, &gViCtx->stLDCAttr[ViChn], enRotation);
	else
		return _vi_update_rotation_mesh(ViChn, enRotation);
}

CVI_S32 CVI_VI_GetChnRotation(VI_PIPE ViPipe, VI_CHN ViChn, ROTATION_E *penRotation)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*penRotation = gViCtx->enRotation[ViChn];
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetChnLDCAttr(VI_PIPE ViPipe, VI_CHN ViChn, const VI_LDC_ATTR_S *pstLDCAttr)
{
#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_setchnldcattr(ViPipe, ViChn, pstLDCAttr);
	}
#endif

	CHECK_VI_NULL_PTR(pstLDCAttr);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_GDC_FMT(gViCtx->chnAttr[ViChn].enPixelFormat);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	return _vi_update_ldc_mesh(ViPipe, ViChn, pstLDCAttr, gViCtx->enRotation[ViChn]);
}

CVI_S32 CVI_VI_GetChnLDCAttr(VI_PIPE ViPipe, VI_CHN ViChn, VI_LDC_ATTR_S *pstLDCAttr)
{
#ifdef RPC_MULTI_PROCESS
	if (!isMaster()) {
		return rpc_client_vi_getchnldcattr(ViPipe, ViChn, pstLDCAttr);
	}
#endif

	CHECK_VI_NULL_PTR(pstLDCAttr);
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	*pstLDCAttr = gViCtx->stLDCAttr[ViChn];
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_RegChnFlipMirrorCallBack(VI_PIPE ViPipe, VI_DEV ViDev, void *pvData)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pvData);

	s_pfnDevMirrorFlip[ViDev] = (pfnChnMirrorFlip)pvData;
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_UnRegChnFlipMirrorCallBack(VI_PIPE ViPipe, VI_DEV ViDev)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_DEVID_VALID(ViDev);

	s_pfnDevMirrorFlip[ViDev] = CVI_NULL;
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_SetChnFlipMirror(VI_PIPE ViPipe, VI_CHN ViChn, CVI_BOOL bFlip, CVI_BOOL bMirror)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (!gViCtx->is_enable[ViChn]) {
		CVI_TRACE_VI(CVI_DBG_ERR, "ViChn(%d) is not enable.\n", ViChn);
		return CVI_FAILURE;
	}

	gViCtx->chnAttr[ViChn].bFlip   = bFlip;
	gViCtx->chnAttr[ViChn].bMirror = bMirror;

	return _vi_chn_enable_mirror_flip(ViChn);
}

CVI_S32 CVI_VI_GetChnFlipMirror(VI_PIPE ViPipe, VI_CHN ViChn, CVI_BOOL *pbFlip, CVI_BOOL *pbMirror)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_CHNID_VALID(ViChn);
	CHECK_VI_NULL_PTR(pbFlip);
	CHECK_VI_NULL_PTR(pbMirror);
	if (CHECK_VI_CTX_NULL_PTR(gViCtx) != CVI_SUCCESS)
		return CVI_ERR_VI_FAILED_NOTCONFIG;

	if (!gViCtx->is_enable[ViChn]) {
		CVI_TRACE_VI(CVI_DBG_ERR, "ViChn(%d) is not enable.\n", ViChn);
		return CVI_FAILURE;
	}

	*pbFlip = gViCtx->chnAttr[ViChn].bFlip;
	*pbMirror = gViCtx->chnAttr[ViChn].bMirror;

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetRgbMapLeBuf(VI_PIPE ViPipe, void *pstRgbMapBuf)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstRgbMapBuf);
	struct vdev *d;

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		s32Ret = CVI_FAILURE;
	}
	fd = d->fd;
	struct cvi_vip_memblock *castBuf = (struct cvi_vip_memblock *)pstRgbMapBuf;
	castBuf->raw_num = ViPipe;

	s32Ret = vi_get_rgbmap_le_buf(fd, castBuf);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_rgbmap_le_buf ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_GetRgbMapSeBuf(VI_PIPE ViPipe, void *pstRgbMapBuf)
{
	CVI_S32 fd = -1;
	CVI_S32 s32Ret = CVI_SUCCESS;
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(pstRgbMapBuf);
	struct vdev *d;

	d = get_dev_info(VDEV_TYPE_ISP, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi state(%d) incorrect.", d->state);
		s32Ret = CVI_FAILURE;
	}
	fd = d->fd;
	struct cvi_vip_memblock *castBuf = (struct cvi_vip_memblock *)pstRgbMapBuf;
	castBuf->raw_num = ViPipe;

	s32Ret = vi_get_rgbmap_se_buf(fd, castBuf);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_rgbmap_se_buf ioctl failed\n");
		return CVI_ERR_VI_FAILED_NOT_ENABLED;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_DumpHwRegisterToFile(VI_PIPE ViPipe, void *fp, VI_DUMP_REGISTER_TABLE_S *pstRegTbl)
{
	CHECK_VI_PIPEID_VALID(ViPipe);
	CHECK_VI_NULL_PTR(fp);
	CHECK_VI_NULL_PTR(pstRegTbl);

	CVI_S32 s32Ret = CVI_SUCCESS;
	CVI_U32 chip;

	CVI_SYS_GetChipId(&chip);

	if (IS_CHIP_CV181X(chip) || IS_CHIP_CV180X(chip)) {
		s32Ret = dump_hw_register(ViPipe, fp, pstRegTbl);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "dump_register fail\n");
			return s32Ret;
		}
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_RegPmCallBack(VI_DEV ViDev, VI_PM_OPS_S *pstPmOps, void *pvData)
{
	CHECK_VI_DEVID_VALID(ViDev);
	CHECK_VI_NULL_PTR(pstPmOps);
	CHECK_VI_NULL_PTR(pvData);

	memcpy(&apstViPm[ViDev].stOps, pstPmOps, sizeof(VI_PM_OPS_S));
	apstViPm[ViDev].pvData = pvData;
	return CVI_SUCCESS;
}

CVI_S32 CVI_VI_UnRegPmCallBack(VI_DEV ViDev)
{
	CHECK_VI_DEVID_VALID(ViDev);

	memset(&apstViPm[ViDev].stOps, 0, sizeof(VI_PM_OPS_S));
	apstViPm[ViDev].pvData = NULL;
	return CVI_SUCCESS;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_Trig_AHD(VI_PIPE ViPipe, CVI_U8 u8AHDSignal)
{
	UNUSED(ViPipe);
	UNUSED(u8AHDSignal);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_SetMipiBindDev(VI_DEV ViDev, MIPI_DEV MipiDev)
{
	UNUSED(ViDev);
	UNUSED(MipiDev);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_GetMipiBindDev(VI_DEV ViDev, MIPI_DEV *pMipiDev)
{
	UNUSED(ViDev);
	UNUSED(pMipiDev);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_SetDevBindPipe(VI_DEV ViDev, const VI_DEV_BIND_PIPE_S *pstDevBindPipe)
{
	UNUSED(ViDev);
	UNUSED(pstDevBindPipe);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_GetDevBindPipe(VI_DEV ViDev, VI_DEV_BIND_PIPE_S *pstDevBindPipe)
{
	UNUSED(ViDev);
	UNUSED(pstDevBindPipe);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_SetChnEarlyInterrupt(VI_PIPE ViPipe, VI_CHN ViChn, const VI_EARLY_INTERRUPT_S *pstEarlyInterrupt)
{
	UNUSED(pstEarlyInterrupt);
	UNUSED(ViPipe);
	UNUSED(ViChn);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_GetChnEarlyInterrupt(VI_PIPE ViPipe, VI_CHN ViChn, VI_EARLY_INTERRUPT_S *pstEarlyInterrupt)
{
	UNUSED(pstEarlyInterrupt);
	UNUSED(ViPipe);
	UNUSED(ViChn);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_SetExtChnAttr(VI_PIPE ViPipe, VI_CHN ViChn, const VI_EXT_CHN_ATTR_S *pstExtChnAttr)
{
	UNUSED(pstExtChnAttr);
	UNUSED(ViPipe);
	UNUSED(ViChn);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_GetExtChnAttr(VI_PIPE ViPipe, VI_CHN ViChn, VI_EXT_CHN_ATTR_S *pstExtChnAttr)
{
	UNUSED(pstExtChnAttr);
	UNUSED(ViPipe);
	UNUSED(ViChn);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_SetDevAttrEx(VI_DEV ViDev, const VI_DEV_ATTR_EX_S *pstDevAttrEx)
{
	UNUSED(ViDev);
	UNUSED(pstDevAttrEx);

	return CVI_ERR_VI_NOT_SUPPORT;
}

/**
 * @deprecated
 */
CVI_S32 CVI_VI_GetDevAttrEx(VI_DEV ViDev, VI_DEV_ATTR_EX_S *pstDevAttrEx)
{
	UNUSED(ViDev);
	UNUSED(pstDevAttrEx);

	return CVI_ERR_VI_NOT_SUPPORT;
}
