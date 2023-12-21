#include <vi_dbg_proc.h>
#include <aos/cli.h>
#include <string.h>
#include <stdio.h>

//one pipe use 832 byte
#define VI_DBG_BUF_SIZE			3072

/* Switch the output of proc.
 *
 * 0: VI debug info
 * 1: preraw0 reg-dump
 * 2: preraw1 reg-dump
 * 3: postraw reg-dump
 */
int proc_isp_mode;

static struct cvi_vi_dev *m_vdev;
/*************************************************************************
 *	Proc functions
 *************************************************************************/
static inline void _vi_dbg_proc_show(void)
{
	struct cvi_vi_dev *vdev = m_vdev;
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	enum cvi_isp_raw raw_max = ISP_PRERAW_MAX - 1;
	struct timespec ts1, ts2;
	u32 frmCnt1, frmCnt2, sofCnt1, sofCnt2;
	u32 frmCnt1_b, frmCnt2_b, sofCnt1_b, sofCnt2_b;
	u32 frmCnt1_c, frmCnt2_c, sofCnt1_c, sofCnt2_c;
	u64 t2 = 0, t1 = 0;
	char *buf = NULL;
	int pos = 0;
	char raw_char;

	if(vdev == NULL) {
		return;
	}
	buf = calloc(1,VI_DBG_BUF_SIZE);
	if (!buf) {
		printf("fail to malloc\n");
		return;
	}

	raw_max = gViCtx->total_dev_num;

	sofCnt1 = vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH0];
	if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) //YUV sensor
		frmCnt1 = vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0];
	else //RGB sensor
		frmCnt1 = vdev->postraw_frame_number[ISP_PRERAW_A];

	sofCnt1_b = vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH0];
	if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) //YUV sensor
		frmCnt1_b = vdev->pre_fe_frm_num[ISP_PRERAW_B][ISP_FE_CH0];
	else //RGB sensor
		frmCnt1_b = vdev->postraw_frame_number[ISP_PRERAW_B];


	sofCnt1_c = vdev->pre_fe_sof_cnt[ISP_PRERAW_C][ISP_FE_CH0];
	if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_C].is_yuv_bypass_path) //YUV sensor
		frmCnt1_c = vdev->pre_fe_frm_num[ISP_PRERAW_C][ISP_FE_CH0];
	else //RGB sensor
		frmCnt1_c = vdev->postraw_frame_number[ISP_PRERAW_C];

	clock_gettime(CLOCK_MONOTONIC, &ts1);
	t1 = ts1.tv_sec * 1000000 + ts1.tv_nsec / 1000;

	usleep(940 * 1000);
	do {
		sofCnt2 = vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH0];
		if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) //YUV sensor
			frmCnt2 = vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0];
		else //RGB sensor
			frmCnt2 = vdev->postraw_frame_number[ISP_PRERAW_A];

		sofCnt2_b = vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH0];
		if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) //YUV sensor
			frmCnt2_b = vdev->pre_fe_frm_num[ISP_PRERAW_B][ISP_FE_CH0];
		else //RGB sensor
			frmCnt2_b = vdev->postraw_frame_number[ISP_PRERAW_B];

		sofCnt2_c = vdev->pre_fe_sof_cnt[ISP_PRERAW_C][ISP_FE_CH0];
		if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_C].is_yuv_bypass_path) //YUV sensor
			frmCnt2_c = vdev->pre_fe_frm_num[ISP_PRERAW_C][ISP_FE_CH0];
		else //RGB sensor
			frmCnt2_c = vdev->postraw_frame_number[ISP_PRERAW_C];

		clock_gettime(CLOCK_MONOTONIC, &ts2);
		t2 = ts2.tv_sec * 1000000 + ts2.tv_nsec / 1000;
	} while ((t2 - t1) < 1000000);

	for (; raw_num < raw_max; raw_num++) {
		if (raw_num == ISP_PRERAW_A) {
			pos += sprintf(buf + pos, "[VI BE_Dbg_Info]\n");
			pos += sprintf(buf + pos, "VIPreBEDoneSts\t\t:0x%x\t\tVIPreBEDmaIdleStatus\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.be_sts.be_done_sts,
					ctx->isp_pipe_cfg[raw_num].dg_info.be_sts.be_dma_idle_sts);
			pos += sprintf(buf + pos, "[VI Post_Dbg_Info]\n");
			pos += sprintf(buf + pos, "VIIspTopStatus\t\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.post_sts.top_sts);
			pos += sprintf(buf + pos, "[VI DMA_Dbg_Info]\n");
			pos += sprintf(buf + pos, "VIWdma0ErrStatus\t:0x%x\tVIWdma0IdleStatus\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_0_err_sts,
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_0_idle);
			pos += sprintf(buf + pos, "VIWdma1ErrStatus\t:0x%x\tVIWdma1IdleStatus\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_1_err_sts,
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_1_idle);
			pos += sprintf(buf + pos, "VIRdmaErrStatus\t\t:0x%x\tVIRdmaIdleStatus\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.rdma_err_sts,
					ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.rdma_idle);
		}
		raw_char = 'A' + raw_num;

		pos += sprintf(buf + pos, "[VI ISP_PIPE_%c FE_Dbg_Info]\n", raw_char);
		pos += sprintf(buf + pos, "VIPreFERawDbgSts\t:0x%x\t\tVIPreFEDbgInfo\t\t:0x%x\n",
				ctx->isp_pipe_cfg[raw_num].dg_info.fe_sts.fe_idle_sts,
				ctx->isp_pipe_cfg[raw_num].dg_info.fe_sts.fe_done_sts);

		pos += sprintf(buf + pos, "[VI ISP_PIPE_%c]\n", raw_char);
		pos += sprintf(buf + pos, "VIOutImgWidth\t\t:%4d\n", ctx->isp_pipe_cfg[raw_num].post_img_w);
		pos += sprintf(buf + pos, "VIOutImgHeight\t\t:%4d\n", ctx->isp_pipe_cfg[raw_num].post_img_h);
		pos += sprintf(buf + pos, "VIInImgWidth\t\t:%4d\n", ctx->isp_pipe_cfg[raw_num].csibdg_width);
		pos += sprintf(buf + pos, "VIInImgHeight\t\t:%4d\n", ctx->isp_pipe_cfg[raw_num].csibdg_height);

		if (raw_num == ISP_PRERAW_A) {
			pos += sprintf(buf + pos, "VIDevFPS\t\t:%4d\n", sofCnt2 - sofCnt1);
			pos += sprintf(buf + pos, "VIFPS\t\t\t:%4d\n", frmCnt2 - frmCnt1);
		} else if (raw_num == ISP_PRERAW_B) {
			pos += sprintf(buf + pos, "VIDevFPS\t\t:%4d\n", sofCnt2_b - sofCnt1_b);
			pos += sprintf(buf + pos, "VIFPS\t\t\t:%4d\n", frmCnt2_b - frmCnt1_b);
		} else {
			pos += sprintf(buf + pos, "VIDevFPS\t\t:%4d\n", sofCnt2_c - sofCnt1_c);
			pos += sprintf(buf + pos, "VIFPS\t\t\t:%4d\n", frmCnt2_c - frmCnt1_c);
		}

		pos += sprintf(buf + pos, "VISofCh0Cnt\t\t:%4d\n", vdev->pre_fe_sof_cnt[raw_num][ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
			pos += sprintf(buf + pos, "VISofCh1Cnt\t\t:%4d\n", vdev->pre_fe_sof_cnt[raw_num][ISP_FE_CH1]);

		pos += sprintf(buf + pos, "VIPreFECh0Cnt\t\t:%4d\n", vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
			pos += sprintf(buf + pos, "VIPreFECh1Cnt\t\t:%4d\n", vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1]);

		pos += sprintf(buf + pos, "VIPreBECh0Cnt\t\t:%4d\n", vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
			pos += sprintf(buf + pos, "VIPreBECh1Cnt\t\t:%4d\n", vdev->pre_be_frm_num[raw_num][ISP_BE_CH1]);
		pos += sprintf(buf + pos, "VIPostCnt\t\t:%4d\n", vdev->postraw_frame_number[raw_num]);
		pos += sprintf(buf + pos, "VIDropCnt\t\t:%4d\n", vdev->drop_frame_number[raw_num]);
		pos += sprintf(buf + pos, "VIDumpCnt\t\t:%4d\n", vdev->dump_frame_number[raw_num]);

		pos += sprintf(buf + pos, "[VI ISP_PIPE_%c Csi_Dbg_Info]\n", raw_char);
		pos += sprintf(buf + pos, "VICsiIntStatus0\t\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_0);
		pos += sprintf(buf + pos, "VICsiIntStatus1\t\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_1);
		pos += sprintf(buf + pos, "VICsiCh0Dbg\t\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_chn_debug[ISP_FE_CH0]);
		pos += sprintf(buf + pos, "VICsiCh1Dbg\t\t:0x%x\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_chn_debug[ISP_FE_CH1]);
		pos += sprintf(buf + pos, "VICsiOverFlowCnt\t:%4d\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_fifo_of_cnt);

		pos += sprintf(buf + pos, "VICsiCh0WidthGTCnt\t:%4d\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			pos += sprintf(buf + pos, "VICsiCh1WidthGTCnt\t:%4d\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[ISP_FE_CH1]);
		}

		pos += sprintf(buf + pos, "VICsiCh0WidthLSCnt\t:%4d\n",
					ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			pos += sprintf(buf + pos, "VICsiCh1WidthLSCnt\t:%4d\n",
						ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[ISP_FE_CH1]);
		}

		pos += sprintf(buf + pos, "VICsiCh0HeightGTCnt\t:%4d\n",
						ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			pos += sprintf(buf + pos, "VICsiCh1HeightGTCnt\t:%4d\n",
						ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[ISP_FE_CH1]);
		}

		pos += sprintf(buf + pos, "VICsiCh0HeightLSCnt\t:%4d\n",
						ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[ISP_FE_CH0]);
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			pos += sprintf(buf + pos, "VICsiCh1HeightLSCnt\t:%4d\n",
							ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[ISP_FE_CH1]);
		}
	}
	printf(buf);
	free(buf);
}

static void vi_dbg_proc_show(int32_t argc, char **argv)
{
	if (proc_isp_mode == 0)
		_vi_dbg_proc_show();
#if 0
	else
		isp_register_dump(&isp_vdev->ctx, m, proc_isp_mode);
#endif
}
ALIOS_CLI_CMD_REGISTER(vi_dbg_proc_show,proc_vi_dbg,vi debug info);

int vi_dbg_proc_init(struct cvi_vi_dev *_vdev)
{
	int rc = 0;

	m_vdev = _vdev;
	return rc;
}

int vi_dbg_proc_remove(void)
{
	int rc = 0;

	m_vdev = NULL;
	return rc;
}
