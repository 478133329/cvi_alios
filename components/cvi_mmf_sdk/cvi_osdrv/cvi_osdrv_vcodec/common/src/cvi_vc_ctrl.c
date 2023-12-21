#include "cvi_vc_ctrl.h"
#include "base_cb.h"
#include "base_ctx.h"
#include "vcodec_cb.h"


#define VC_REG_CTRL_BASE 0x0B030000
#define VC_REG_SBM_BASE 0x0B058000
#define VC_REG_REMAP_BASE 0x0B050000
#define SBM_IRQ_NUM 19

static struct cvi_vc_drv_context vc_drv_ctx = {0};
extern int request_irq(unsigned int irqn, void *handler, unsigned long flags, const char *name, void *priv);
extern CVI_S32 cvi_VENC_CB_SendFrame(CVI_S32 VpssGrp, CVI_S32 VpssChn, CVI_S32 VpssChn1,
									   const struct cvi_buffer *pstInFrmBuf,
									   const struct cvi_buffer *pstInFrmBuf1,
									   CVI_BOOL isOnline,
									   CVI_U32 sb_nb, CVI_S32 s32MilliSec);
extern CVI_S32 cvi_VENC_CB_SkipFrame(CVI_S32 VpssGrp, CVI_S32 VpssChn, CVI_U32 srcImgHeight, CVI_S32 s32MilliSec);
extern CVI_S32 cvi_VENC_CB_SnapJpgFrame(CVI_S32 VpssGrp, CVI_S32 VpssChn, CVI_U32 FrmCnt);

unsigned int cvi_vc_drv_read_vc_reg(REG_TYPE eRegType, unsigned long addr)
{
	unsigned int *reg_addr = NULL;
	// TODO: enable CCF?

	switch (eRegType) {
	case REG_CTRL:
 		reg_addr = (unsigned int *)(addr + VC_REG_CTRL_BASE);
		break;
	case REG_SBM:
		reg_addr = (unsigned int *)(addr + VC_REG_SBM_BASE);
		break;
	case REG_REMAP:
		reg_addr = (unsigned int *)(addr + VC_REG_REMAP_BASE);
		break;
	default:
		break;
	}

	if (!reg_addr)
		return 0;

	//printf("vc read, 0x%x, 0x%x\n", addr, *reg_addr);

	return *(volatile unsigned int *)reg_addr;
}

void cvi_vc_drv_write_vc_reg(REG_TYPE eRegType, unsigned long addr, unsigned int data)
{
	unsigned int *reg_addr = NULL;
	// TODO: enable CCF?

	switch (eRegType) {
		case REG_CTRL:
 		reg_addr = (unsigned int *)(addr + VC_REG_CTRL_BASE);
		break;
	case REG_SBM:
		reg_addr = (unsigned int *)(addr + VC_REG_SBM_BASE);
		break;
	case REG_REMAP:
		reg_addr = (unsigned int *)(addr + VC_REG_REMAP_BASE);
		break;
	default:
		break;
	}

	if (!reg_addr)
		return;

	//printf("vc write, 0x%x = 0x%x\n", addr, data);

	*(volatile unsigned int *)reg_addr = data;
}

void cvi_VENC_SBM_IrqEnable(void)
{
	unsigned int reg;

	// sbm interrupt enable (1: enable, 0: disable/clear)
	reg = ReadSbmRegister(0x08);
	// [13]: push0
	reg |= (0x1 << 13);
	WritSbmRegister(0x08, reg);
//	printf("%s %d reg_08:0x%x \n", __FUNCTION__, __LINE__, reg);

	return;
}

void cvi_VENC_SBM_IrqDisable(void)
{
	unsigned int reg;

	// printk("%s \n", __FUNCTION__);
	// sbm interrupt enable (1: enable, 0: disable/clear)
	reg = ReadSbmRegister(0x08);
	// [13]: push0
	reg &= ~(0x1 << 13);
	WritSbmRegister(0x08, reg);
//	printf("%s %d reg_08:0x%x \n", __FUNCTION__, __LINE__, reg);

	return;
}

int sbm_irq_handler(int irq, void *priv)
{
	struct cvi_vc_drv_context *pvctx = NULL;

	pvctx = &vc_drv_ctx;

	cvi_VENC_SBM_IrqDisable();

	SEM_POST(pvctx->s_sbm_interrupt_sem);

	return 0;
}

void wake_sbm_waitinng(void)
{
	struct cvi_vc_drv_context *pvctx = NULL;

	pvctx = &vc_drv_ctx;

	SEM_POST(pvctx->s_sbm_interrupt_sem);

	return;
}

int sbm_wait_interrupt(int timeout)
{
	int ret = 0;
	struct cvi_vc_drv_context *pvctx = NULL;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	pvctx = &vc_drv_ctx;

	if (timeout > 0) {
		long ltimeout = timeout;

		ts.tv_nsec = ts.tv_nsec + ltimeout * 1000 * 1000;
		ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
		ts.tv_nsec %= (1000 * 1000 * 1000);
	} else
		ts.tv_sec += 2;

	ret = SEM_TIMEDWAIT(pvctx->s_sbm_interrupt_sem, ts);
	if (ret) {
		ret = -1;
		return ret;
	}

	return ret;
}

int vcodec_drv_cb(void *dev, enum ENUM_MODULES_ID caller, u32 cmd, void *arg)
{
	int ret = CVI_FAILURE;

	switch (cmd) {
	case VCODEC_CB_SEND_FRM:
	{
		struct venc_send_frm_info *pstFrmInfo = (struct venc_send_frm_info *)arg;

		ret = cvi_VENC_CB_SendFrame(pstFrmInfo->vpss_grp,
									pstFrmInfo->vpss_chn, pstFrmInfo->vpss_chn1,
									&pstFrmInfo->stInFrmBuf,
									&pstFrmInfo->stInFrmBuf1,
									pstFrmInfo->isOnline,
									pstFrmInfo->sb_nb,
									20000);
		break;
	}

	case VCODEC_CB_SKIP_FRM:
	{
		struct venc_send_frm_info *pstFrmInfo = (struct venc_send_frm_info *)arg;

		ret = cvi_VENC_CB_SkipFrame(pstFrmInfo->vpss_grp, pstFrmInfo->vpss_chn,
							  pstFrmInfo->stInFrmBuf.size.u32Height, 100);
		break;
	}

	case VCODEC_CB_SNAP_JPG_FRM:
	{
		struct venc_snap_frm_info *pstSkipInfo = (struct venc_snap_frm_info *)arg;

		ret = cvi_VENC_CB_SnapJpgFrame(pstSkipInfo->vpss_grp,
										pstSkipInfo->vpss_chn,
										pstSkipInfo->skip_frm_cnt);
		break;
	}

	case VCODEC_CB_OVERFLOW_CHECK:
	{
		struct cvi_vc_info *vc_info = (struct cvi_vc_info *)arg;

		unsigned int reg_00 = cvi_vc_drv_read_vc_reg(REG_SBM, 0x00);
		unsigned int reg_08 = cvi_vc_drv_read_vc_reg(REG_SBM, 0x08);
		unsigned int reg_88 = cvi_vc_drv_read_vc_reg(REG_SBM, 0x88);
		unsigned int reg_90 = cvi_vc_drv_read_vc_reg(REG_SBM, 0x90);
		unsigned int reg_94 = cvi_vc_drv_read_vc_reg(REG_SBM, 0x94);

		vc_info->enable = true;
		vc_info->reg_00 = reg_00;
		vc_info->reg_08 = reg_08;
		vc_info->reg_88 = reg_88;
		vc_info->reg_90 = reg_90;
		vc_info->reg_94 = reg_94;

		aos_debug_printf("vc check reg_00=0x%x, reg_08=0x%x, reg_88=0x%x, reg_90=0x%x, reg_94=0x%x\n",
			reg_00, reg_08, reg_88, reg_90, reg_94);

		ret = CVI_SUCCESS;
		break;
	}

	default:
		break;
	}

	return ret;
}

static int vcodec_drv_register_cb(struct cvi_vc_drv_context *ctx)
{
	struct base_m_cb_info reg_cb;

	reg_cb.module_id	= E_MODULE_VCODEC;
	reg_cb.dev		= (void *)ctx;
	reg_cb.cb		= vcodec_drv_cb;

	return base_reg_module_cb(&reg_cb);
}

void cvi_vc_drv_init(void)
{
	struct cvi_vc_drv_context *pvctx = NULL;

	pvctx = &vc_drv_ctx;

	if(pvctx->binit)
		return;

	pvctx->irq_num = SBM_IRQ_NUM;

	SEM_INIT(vc_drv_ctx.s_sbm_interrupt_sem);

	vcodec_drv_register_cb(pvctx);

	request_irq(pvctx->irq_num, sbm_irq_handler, 0, "VC_SBM", NULL);

	pvctx->binit = 1;

	return;
}


