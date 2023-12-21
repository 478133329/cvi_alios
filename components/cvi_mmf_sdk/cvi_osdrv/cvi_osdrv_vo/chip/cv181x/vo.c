#include <common.h>
#include <vo.h>
#include <vb.h>
#include <vo_common.h>
#include <vo_core.h>
#include <vo_defines.h>
#include <vo_interfaces.h>
#include "mipi_tx.h"
#include <proc/vo_proc.h>
#include <cvi_buffer.h>
#include <pinctrl-mars.h>

#include <base_cb.h>
#include <vpss_cb.h>
#include <vo_cb.h>
#include <ldc_cb.h>
#include <rgn_cb.h>
#include "vo_rgn_ctrl.h"
#include "sys.h"

//include hearder from vpss
#include "dsi_phy.h"

/*******************************************************
 *  MACRO defines
 ******************************************************/
#define SEM_WAIT_TIMEOUT_MS  50
#define CVI_TRACE_VOOFILE
/*******************************************************
 *  Global variables
 ******************************************************/
//u32 vo_log_lv = CVI_VO_ERR | CVI_VO_WARN | CVI_VO_NOTICE;
static int smooth;
static int job_init;
static bool hide_vo;
struct cvi_vo_ctx *gVoCtx;
static struct cvi_vo_dev *gVdev;
static CVI_U8 i80_ctrl[I80_CTRL_MAX] = { 0x31, 0x75, 0xff };
static pthread_mutex_t vo_gdc_lock;
static atomic_t  dev_open_cnt;
static void *plogodata = NULL;

struct _vo_gdc_cb_param {
	MMF_CHN_S chn;
	enum GDC_USAGE usage;
};


const struct vo_disp_pattern patterns[CVI_VIP_PAT_MAX] = {
	{.type = SCL_PAT_TYPE_OFF,	.color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_SNOW, .color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_AUTO, .color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_FULL, .color = SCL_PAT_COLOR_RED},
	{.type = SCL_PAT_TYPE_FULL, .color = SCL_PAT_COLOR_GREEN},
	{.type = SCL_PAT_TYPE_FULL, .color = SCL_PAT_COLOR_BLUE},
	{.type = SCL_PAT_TYPE_FULL, .color = SCL_PAT_COLOR_BAR},
	{.type = SCL_PAT_TYPE_H_GRAD, .color = SCL_PAT_COLOR_WHITE},
	{.type = SCL_PAT_TYPE_V_GRAD, .color = SCL_PAT_COLOR_WHITE},
	{.type = SCL_PAT_TYPE_FULL, .color = SCL_PAT_COLOR_USR,
	.rgb = {0, 0, 0} },
};

/*******************************************************
 *  Internal APIs
 ******************************************************/
static int _vo_create_proc(struct cvi_vo_dev *vdev)
{
	int ret = 0;

	/* vo proc setup */
	vdev->shared_mem = malloc(VO_SHARE_MEM_SIZE);
	if (!vdev->shared_mem) {
		CVI_TRACE_VO(CVI_VO_ERR, "shared_mem malloc size(%d) failed\n", VO_SHARE_MEM_SIZE);
		return -ENOMEM;
	}
	memset(vdev->shared_mem, 0, VO_SHARE_MEM_SIZE);

	if (vo_proc_init(vdev->shared_mem) < 0) {
		CVI_TRACE_VO(CVI_VO_ERR, "vo proc init failed\n");
		return -EAGAIN;
	}

	return ret;
}

static int _vo_destroy_proc(struct cvi_vo_dev *vdev)
{
	int ret = 0;

	vo_proc_remove();

	free(vdev->shared_mem);
	vdev->shared_mem = NULL;

	return ret;
}
static u8 _gop_get_bpp(enum sclr_gop_format fmt)
{
	return (fmt == SCL_GOP_FMT_ARGB8888) ? 4 :
		(fmt == SCL_GOP_FMT_256LUT) ? 1 : 2;
}

int vo_set_interface(struct cvi_vo_dev *vdev, struct cvi_disp_intf_cfg *cfg)
{
	int rc = -1;

	if (smooth) {
		CVI_TRACE_VO(CVI_VO_DBG, "V4L2_CID_DV_VIP_DISP_INTF won't apply if smooth.\n");
		sclr_disp_reg_force_up();
		vdev->disp_interface = cfg->intf_type;

		rc = 0;
		return rc;
	}

	if (atomic_read(&vdev->disp_streamon) == 1) {
		CVI_TRACE_VO(CVI_VO_INFO, "V4L2_CID_DV_VIP_DISP_ONLINE can't be control if streaming.\n");
		rc = 0;

		return rc;
	}

	if (cfg->intf_type == CVI_VIP_DISP_INTF_DSI) {
		CVI_TRACE_VO(CVI_VO_INFO, "MIPI use mipi_tx to control.\n");

		rc = 0;
		return rc;
	} else if (cfg->intf_type == CVI_VIP_DISP_INTF_LVDS) {
		int i = 0;
		union sclr_lvdstx lvds_reg;
		bool data_en[LANE_MAX_NUM] = {false, false, false, false, false};

		for (i = 0; i < LANE_MAX_NUM; i++) {
			if ((cfg->lvds_cfg.lane_id[i] < 0) ||
				(cfg->lvds_cfg.lane_id[i] >= LANE_MAX_NUM)) {
				dphy_dsi_set_lane(i, DSI_LANE_MAX, false, false);
				continue;
			}
			dphy_dsi_set_lane(i, cfg->lvds_cfg.lane_id[i],
					  cfg->lvds_cfg.lane_pn_swap[i], false);
			if (cfg->lvds_cfg.lane_id[i] != MIPI_TX_LANE_CLK) {
				data_en[cfg->lvds_cfg.lane_id[i] - 1] = true;
			}
		}
		dphy_dsi_lane_en(true, data_en, false);

		sclr_disp_set_intf(SCLR_VO_INTF_LVDS);

		if (cfg->lvds_cfg.pixelclock == 0) {
			CVI_TRACE_VO(CVI_VO_ERR, "lvds pixelclock 0 invalid\n");
			return rc;
		}
		lvds_reg.b.out_bit = cfg->lvds_cfg.out_bits;
		lvds_reg.b.vesa_mode = cfg->lvds_cfg.mode;
		if (cfg->lvds_cfg.chn_num == 1)
			lvds_reg.b.dual_ch = 0;
		else if (cfg->lvds_cfg.chn_num == 2)
			lvds_reg.b.dual_ch = 1;
		else {
			lvds_reg.b.dual_ch = 0;
			CVI_TRACE_VO(CVI_VO_ERR, "invalid lvds chn_num(%d). Use 1 instead."
				, cfg->lvds_cfg.chn_num);
		}
		lvds_reg.b.vs_out_en = cfg->lvds_cfg.vs_out_en;
		lvds_reg.b.hs_out_en = cfg->lvds_cfg.hs_out_en;
		lvds_reg.b.hs_blk_en = cfg->lvds_cfg.hs_blk_en;
		lvds_reg.b.ml_swap = cfg->lvds_cfg.msb_lsb_data_swap;
		lvds_reg.b.ctrl_rev = cfg->lvds_cfg.serial_msb_first;
		lvds_reg.b.oe_swap = cfg->lvds_cfg.even_odd_link_swap;
		lvds_reg.b.en = cfg->lvds_cfg.enable;
		dphy_dsi_analog_setting(true);
		dphy_lvds_set_pll(cfg->lvds_cfg.pixelclock, cfg->lvds_cfg.chn_num);
		sclr_lvdstx_set(lvds_reg);
	} else if (cfg->intf_type == CVI_VIP_DISP_INTF_I80) {
		union sclr_bt_enc enc;
		union sclr_bt_sync_code sync;

		sclr_disp_set_intf(SCLR_VO_INTF_I80);
		enc.raw = 0;
		enc.b.fmt_sel = 2;
		enc.b.clk_inv = 1;
		sync.raw = 0;
		sync.b.sav_vld = 0x80;
		sync.b.sav_blk = 0xab;
		sync.b.eav_vld = 0x9d;
		sync.b.eav_blk = 0xb6;
		sclr_bt_set(enc, sync);
	} else if (cfg->intf_type == CVI_VIP_DISP_INTF_HW_MCU) {
		sclr_disp_set_mcu_disable(cfg->mcu_cfg.mode);
		sclr_disp_mux_sel(SCLR_VO_SEL_HW_MCU);
		sclr_disp_set_intf(SCLR_VO_INTF_HW_MCU);

		if (cfg->mcu_cfg.mode == MCU_MODE_RGB565) {
			dphy_dsi_set_pll(cfg->mcu_cfg.pixelclock * 4, 4, 24);
			vip_sys_clk_setting(0x10080);
		} else if (cfg->mcu_cfg.mode == MCU_MODE_RGB888) {
			dphy_dsi_set_pll(cfg->mcu_cfg.pixelclock * 6, 4, 24);
			vip_sys_clk_setting(0x10080);
		}

		//vo sel
		for (int i = 0; i < cfg->mcu_cfg.pins.pin_num; ++i) {
			sclr_top_vo_mux_sel(cfg->mcu_cfg.pins.d_pins[i].sel, cfg->mcu_cfg.pins.d_pins[i].mux);
		}

		hw_mcu_cmd_send(cfg->mcu_cfg.instrs.instr_cmd, cfg->mcu_cfg.instrs.instr_num);
		sclr_disp_set_mcu_en(cfg->mcu_cfg.mode);
	} else if (cfg->intf_type == CVI_VIP_DISP_INTF_BT) {
		union sclr_bt_enc enc;
		union sclr_bt_sync_code sync;

		if (cfg->bt_cfg.mode == BT_MODE_1120)
			sclr_disp_set_intf(SCLR_VO_INTF_BT1120);
		else if (cfg->bt_cfg.mode == BT_MODE_656)
			sclr_disp_set_intf(SCLR_VO_INTF_BT656);
		else if (cfg->bt_cfg.mode == BT_MODE_601)
			sclr_disp_set_intf(SCLR_VO_INTF_BT601);
		else {
			CVI_TRACE_VO(CVI_VO_ERR, "invalid bt-mode(%d)\n", cfg->bt_cfg.mode);
			//return rc;
		}

		if (cfg->bt_cfg.mode == BT_MODE_1120) {
			dphy_dsi_set_pll(cfg->bt_cfg.pixelclock, 4, 24);
			vip_sys_clk_setting(0x10010);
		} else if (cfg->bt_cfg.mode == BT_MODE_656) {
			dphy_dsi_set_pll(cfg->bt_cfg.pixelclock * 2, 4, 24);
			vip_sys_clk_setting(0x10000);
		} else if (cfg->bt_cfg.mode == BT_MODE_601) {
			dphy_dsi_set_pll(cfg->bt_cfg.pixelclock * 2, 4, 24);
			vip_sys_clk_setting(0x10000);
		}

		//vo sel
		for (int i = 0; i < cfg->bt_cfg.pins.pin_num; ++i) {
			sclr_top_vo_mux_sel(cfg->bt_cfg.pins.d_pins[i].sel, cfg->bt_cfg.pins.d_pins[i].mux);
		}

		//set csc value
		sclr_disp_set_out_csc(SCL_CSC_601_FULL_RGB2YUV);

		enc.raw = 0;
		enc.b.fmt_sel = cfg->bt_cfg.mode;
		sync.b.sav_vld = 0x80;
		sync.b.sav_blk = 0xab;
		sync.b.eav_vld = 0x9d;
		sync.b.eav_blk = 0xb6;
		sclr_bt_set(enc, sync);
	} else {
		CVI_TRACE_VO(CVI_VO_ERR, "invalid disp-intf(%d)\n", cfg->intf_type);
		return rc;
	}
	sclr_disp_reg_force_up();

	vdev->disp_interface = cfg->intf_type;

	rc = 0;
	return rc;
}

int vo_set_rgn_cfg(const u8 inst, const struct cvi_rgn_cfg *cfg, const struct sclr_size *size)
{
	u8 i, layer = 0;
	struct sclr_gop_cfg *gop_cfg = sclr_gop_get_cfg(inst, layer);
	struct sclr_gop_ow_cfg *ow_cfg;

	gop_cfg->gop_ctrl.raw &= ~0xfff;
	gop_cfg->gop_ctrl.b.hscl_en = cfg->hscale_x2;
	gop_cfg->gop_ctrl.b.vscl_en = cfg->vscale_x2;
	gop_cfg->gop_ctrl.b.colorkey_en = cfg->colorkey_en;
	gop_cfg->colorkey = cfg->colorkey;

	for (i = 0; i < cfg->num_of_rgn; ++i) {
		u8 bpp = _gop_get_bpp(cfg->param[i].fmt);

		ow_cfg = &gop_cfg->ow_cfg[i];
		gop_cfg->gop_ctrl.raw |= BIT(i);

		ow_cfg->fmt = cfg->param[i].fmt;
		ow_cfg->addr = cfg->param[i].phy_addr;
		ow_cfg->pitch = cfg->param[i].stride;
		if (cfg->param[i].rect.left < 0) {
			ow_cfg->start.x = 0;
			ow_cfg->addr -= bpp * cfg->param[i].rect.left;
			ow_cfg->img_size.w = cfg->param[i].rect.width + cfg->param[i].rect.left;
		} else if ((cfg->param[i].rect.left + cfg->param[i].rect.width) > size->w) {
			ow_cfg->start.x = cfg->param[i].rect.left;
			ow_cfg->img_size.w = size->w - cfg->param[i].rect.left;
			ow_cfg->mem_size.w = cfg->param[i].stride;
		} else {
			ow_cfg->start.x = cfg->param[i].rect.left;
			ow_cfg->img_size.w = cfg->param[i].rect.width;
			ow_cfg->mem_size.w = cfg->param[i].stride;
		}

		if (cfg->param[i].rect.top < 0) {
			ow_cfg->start.y = 0;
			ow_cfg->addr -= ow_cfg->pitch * cfg->param[i].rect.top;
			ow_cfg->img_size.h = cfg->param[i].rect.height + cfg->param[i].rect.top;
		} else if ((cfg->param[i].rect.top + cfg->param[i].rect.height) > size->h) {
			ow_cfg->start.y = cfg->param[i].rect.top;
			ow_cfg->img_size.h = size->h - cfg->param[i].rect.top;
		} else {
			ow_cfg->start.y = cfg->param[i].rect.top;
			ow_cfg->img_size.h = cfg->param[i].rect.height;
		}

		ow_cfg->end.x = ow_cfg->start.x + (ow_cfg->img_size.w << gop_cfg->gop_ctrl.b.hscl_en)
						- gop_cfg->gop_ctrl.b.hscl_en;
		ow_cfg->end.y = ow_cfg->start.y + (ow_cfg->img_size.h << gop_cfg->gop_ctrl.b.vscl_en)
						- gop_cfg->gop_ctrl.b.vscl_en;
		ow_cfg->mem_size.w = ALIGN(ow_cfg->img_size.w * bpp, GOP_ALIGNMENT);
		ow_cfg->mem_size.h = ow_cfg->img_size.h;
#if 0
		CVI_TRACE_VO(CVI_VO_INFO, "gop(%d) fmt(%d) rect(%d %d %d %d) addr(%lx) pitch(%d).\n", inst
			, ow_cfg->fmt, ow_cfg->start.x, ow_cfg->start.y, ow_cfg->img_size.w, ow_cfg->img_size.h
			, ow_cfg->addr, ow_cfg->pitch);
#endif
		sclr_gop_ow_set_cfg(inst, layer, i, ow_cfg, true);
	}

	sclr_gop_set_cfg(inst, layer, gop_cfg, true);

	return 0;
}


/*******************************************************
 *  File operations for core
 ******************************************************/
void vo_fill_disp_timing(struct sclr_disp_timing *timing,
		struct vo_bt_timings *bt_timing)
{
	timing->vtotal = VO_DV_BT_FRAME_HEIGHT(bt_timing) - 1;
	timing->htotal = VO_DV_BT_FRAME_WIDTH(bt_timing) - 1;
	timing->vsync_start = 1;
	timing->vsync_end = timing->vsync_start + bt_timing->vsync - 1;
	timing->vfde_start = timing->vmde_start =
		timing->vsync_start + bt_timing->vsync + bt_timing->vbackporch;
	timing->vfde_end = timing->vmde_end =
		timing->vfde_start + bt_timing->height - 1;
	timing->hsync_start = 1;
	timing->hsync_end = timing->hsync_start + bt_timing->hsync - 1;
	timing->hfde_start = timing->hmde_start =
		timing->hsync_start + bt_timing->hsync + bt_timing->hbackporch;
	timing->hfde_end = timing->hmde_end =
		timing->hfde_start + bt_timing->width - 1;
	timing->vsync_pol = bt_timing->polarities & VO_DV_VSYNC_POS_POL;
	timing->hsync_pol = bt_timing->polarities & VO_DV_HSYNC_POS_POL;
}

struct cvi_disp_buffer *vo_next_buf(struct cvi_vo_dev *vdev)
{
	unsigned long flags;
	struct cvi_disp_buffer *b = NULL;
	int i = 0;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	if (!list_empty(&vdev->rdy_queue))
		b = list_first_entry(&vdev->rdy_queue,
			struct cvi_disp_buffer, list);
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	for (i = 0; i < 3; i++) {
		CVI_TRACE_VO(CVI_VO_DBG, "qbuf->buf.planes[%d].addr=%llx\n", i, b->buf.planes[i].addr);
	}

	return b;
}

struct cvi_disp_buffer *vo_buf_remove(struct cvi_vo_dev *vdev)
{
	unsigned long flags;
	struct cvi_disp_buffer *b = NULL;

	if (atomic_read(&vdev->num_rdy) == 0)
		return b;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	if (!list_empty(&vdev->rdy_queue)) {
		b = list_first_entry(&vdev->rdy_queue,
			struct cvi_disp_buffer, list);
		list_del_init(&b->list);
		free(b);
		atomic_dec(&vdev->num_rdy);
	}
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	return b;
}

static void _vo_hw_enque(struct cvi_vo_dev *vdev)
{
	struct cvi_disp_buffer *b = NULL;
	int i = 0;
	struct sclr_disp_cfg *cfg;

	if (vdev->disp_online)
		return;
	b = vo_next_buf(vdev);
	if (!b) {
		CVI_TRACE_VO(CVI_VO_ERR, "Ready queue buffer empty\n");
		return;
	}

	for (i = 0; i < 3; i++) {
		CVI_TRACE_VO(CVI_VO_DBG, "b->buf.planes[%d].addr=%llx\n", i, b->buf.planes[i].addr);
	}

	cfg = sclr_disp_get_cfg();

	cfg->mem.addr0 = b->buf.planes[0].addr;
	cfg->mem.addr1 = b->buf.planes[1].addr;
	cfg->mem.addr2 = b->buf.planes[2].addr;
	cfg->mem.pitch_y = (b->buf.planes[0].bytesused > vdev->bytesperline[0])
			 ? b->buf.planes[0].bytesused
			 : vdev->bytesperline[0];
	cfg->mem.pitch_c = (b->buf.planes[1].bytesused > vdev->bytesperline[1])
			 ? b->buf.planes[1].bytesused
			 : vdev->bytesperline[1];

	sclr_disp_set_mem(&cfg->mem);

	if (vdev->disp_interface == CVI_VIP_DISP_INTF_I80) {
		sclr_disp_reg_force_up();
		sclr_i80_run();
	}
}

void vo_wake_up_th(struct cvi_vo_dev *vdev)
{
	CVI_TRACE_VO(CVI_VO_DBG, "wake up th when vb buffer done\n");
	vdev->vo_th[E_VO_TH_DISP].flag = 1;
	// sem_post(&vdev->vo_th[E_VO_TH_DISP].sem);
	aos_event_set(&vdev->vo_th[E_VO_TH_DISP].event, 0x01, AOS_EVENT_OR);
}

void vo_buf_queue(struct cvi_vo_dev *vdev, struct cvi_disp_buffer *b)
{
	unsigned long flags;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	list_add_tail(&b->list, &vdev->rdy_queue);
	atomic_inc(&vdev->num_rdy);
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);
}

static void vo_disp_buf_queue(struct cvi_vo_dev *vdev, struct cvi_disp_buffer *b)
{
	vo_buf_queue(vdev, b);

	if (atomic_read(&vdev->num_rdy) == 1) {
		if (gVoCtx->is_layer_enable[0]) {
			sclr_disp_enable_window_bgcolor(false);
		}

		_vo_hw_enque(vdev);
		if (vdev->disp_interface == CVI_VIP_DISP_INTF_I80) {
			vo_buf_remove((struct cvi_vo_dev *)vdev);

			vo_wake_up_th((struct cvi_vo_dev *)vdev);
		}
	}
}

CVI_S32 _vo_send_logo_from_ion(VO_LAYER VoLayer, VO_CHN VoChn, VIDEO_FRAME_INFO_S *pstVideoFrame,
		CVI_S32 s32MilliSec)
{
	UNUSED(VoLayer);
	UNUSED(VoChn);
	UNUSED(s32MilliSec);

	struct sclr_disp_cfg *cfg = sclr_disp_get_cfg();
	CVI_U32 u32Len;
	CVI_U64 u64PhyAddr;
	CVI_S32 S32Ret;

	u32Len = pstVideoFrame->stVFrame.u32Stride[0] * pstVideoFrame->stVFrame.u32Height * 3 / 2;//yuv420
	S32Ret = sys_ion_alloc(&u64PhyAddr, &plogodata, "CVI_LOGO", u32Len, true);
	if (S32Ret != CVI_SUCCESS) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys_ion_alloc failed.\n");
		return -1;
	}
	memcpy(plogodata, pstVideoFrame->stVFrame.pu8VirAddr[0],
		pstVideoFrame->stVFrame.u32Stride[0] * pstVideoFrame->stVFrame.u32Height);
	memcpy(plogodata + pstVideoFrame->stVFrame.u32Stride[0] * pstVideoFrame->stVFrame.u32Height,
		pstVideoFrame->stVFrame.pu8VirAddr[1],
		pstVideoFrame->stVFrame.u32Stride[1] * pstVideoFrame->stVFrame.u32Height / 2);
	csi_dcache_clean_range((uintptr_t *)plogodata, u32Len);

	sclr_disp_enable_window_bgcolor(false);

	u64PhyAddr = (CVI_U64)plogodata;
	cfg->mem.addr0 = u64PhyAddr;
	cfg->mem.addr1 = u64PhyAddr +
			pstVideoFrame->stVFrame.u32Stride[0] * pstVideoFrame->stVFrame.u32Height;
	cfg->mem.addr2 = 0;
	cfg->mem.pitch_y = pstVideoFrame->stVFrame.u32Stride[0];
	cfg->mem.pitch_c = pstVideoFrame->stVFrame.u32Stride[1];
	// cfg->mem.start_x = pstVideoFrame->stVFrame.u32Stride[0] - pstVideoFrame->stVFrame.u32Width;
	cfg->mem.start_x = 0;
	cfg->mem.start_y = 0;

	sclr_disp_set_mem(&cfg->mem);
	return CVI_SUCCESS;
}


static void _i80_package_eol(CVI_U8 buffer[3])
{
	// pull high i80-lane
	buffer[0] = 0xff;
	buffer[1] = i80_ctrl[I80_CTRL_EOF];
	buffer[2] = I80_OP_GO;
}

static void _i80_package_eof(CVI_U8 buffer[3])
{
	buffer[0] = 0x00;
	buffer[1] = i80_ctrl[I80_CTRL_EOF];
	buffer[2] = I80_OP_DONE;
}

static void _get_frame_rgb(PIXEL_FORMAT_E fmt, CVI_U8 **buf, CVI_U32 *stride, CVI_U16 x, CVI_U16 y,
	CVI_U8 *r, CVI_U8 *g, CVI_U8 *b)
{
	if (fmt == PIXEL_FORMAT_RGB_888) {
		CVI_U32 offset = 3 * x + stride[0] * y;

		*r = *(buf[0] + offset);
		*g = *(buf[0] + offset + 1);
		*b = *(buf[0] + offset + 2);
	} else if (fmt == PIXEL_FORMAT_BGR_888) {
		CVI_U32 offset = 3 * x + stride[0] * y;

		*b = *(buf[0] + offset);
		*g = *(buf[0] + offset + 1);
		*r = *(buf[0] + offset + 2);
	} else if (fmt == PIXEL_FORMAT_RGB_888_PLANAR) {
		CVI_U32 offset = x + stride[0] * y;

		*r = *(buf[0] + offset);
		*g = *(buf[1] + offset);
		*b = *(buf[2] + offset);
	} else if (fmt == PIXEL_FORMAT_BGR_888_PLANAR) {
		CVI_U32 offset = x + stride[0] * y;

		*b = *(buf[0] + offset);
		*g = *(buf[1] + offset);
		*r = *(buf[2] + offset);
	} else {
		*b = *g = *r = 0;
	}
}

CVI_U32 _MAKECOLOR(CVI_U8 r, CVI_U8 g, CVI_U8 b, VO_I80_FORMAT fmt)
{
	CVI_U8 r1, g1, b1;
	CVI_U8 r_len, g_len, b_len;

	switch (fmt) {
	case VO_I80_FORMAT_RGB444:
		r_len = 4;
		g_len = 4;
		b_len = 4;
		break;

	default:
	case VO_I80_FORMAT_RGB565:
		r_len = 5;
		g_len = 6;
		b_len = 5;
		break;

	case VO_I80_FORMAT_RGB666:
		r_len = 6;
		g_len = 6;
		b_len = 6;
		break;
	}

	r1 = r >> (8 - r_len);
	g1 = g >> (8 - g_len);
	b1 = b >> (8 - b_len);
	return (b1 | (g1 << b_len) | (r1 << (b_len + g_len)));
}


static void _i80_package_rgb(CVI_U8 r, CVI_U8 g, CVI_U8 b, CVI_U8 *buffer, CVI_U8 byte_cnt)
{
	CVI_U32 pixel, i, offset;

	pixel = _MAKECOLOR(r, g, b, gVoCtx->stPubAttr.sti80Cfg.fmt);

	for (i = 0, offset = 0; i < byte_cnt; ++i) {
		*(buffer + offset++) = pixel >> ((byte_cnt - i - 1) << 3);
		*(buffer + offset++) = i80_ctrl[I80_CTRL_DATA];
		*(buffer + offset++) = I80_OP_GO;
	}
}

static void _i80_package_frame(struct vb_s *in, CVI_U8 *buffer, CVI_U8 byte_cnt)
{
	CVI_U32 out_offset = 0;
	CVI_U16 line_data = (1 + in->buf.size.u32Width * byte_cnt) * 3;
	CVI_U16 padding = ALIGN(line_data, 32) - line_data;
	CVI_U8 *in_buf_vir[3] = { CVI_NULL, CVI_NULL, CVI_NULL };
	CVI_U8 r, g, b, i, y, x;
#if 0
	struct timespec time[2];

	clock_gettime(CLOCK_REALTIME, &time[0]);
#endif
	for (i = 0; i < 3; ++i) {
		if (in->buf.phy_addr[i] == 0 || in->buf.length[i] == 0)
			continue;
		//in_buf_vir[i] = CVI_SYS_MmapCache(in->buf.phy_addr[i], in->buf.length[i]);
		if (in_buf_vir[i] == CVI_NULL) {
			CVI_TRACE_VO(CVI_VO_INFO, "mmap for i80 transform failed.\n");
			goto ERR_I80_MMAP;
		}
	}

	for (y = 0; y < in->buf.size.u32Height; ++y) {
		for (x = 0; x < in->buf.size.u32Width; ++x) {
			_get_frame_rgb(gVoCtx->stLayerAttr.enPixFormat, in_buf_vir, in->buf.stride, x, y, &r, &g, &b);
			_i80_package_rgb(r, g, b, buffer + out_offset, byte_cnt);
			out_offset += byte_cnt * 3;
		}
		_i80_package_eol(buffer + out_offset);
		out_offset += 3;
		out_offset += padding;
	}
	// replace last eol with eof
	_i80_package_eof(buffer + out_offset - 3 - padding);

#if 0
	clock_gettime(CLOCK_REALTIME, &time[1]);
	CVI_TRACE_VO(CVI_DBG_INFO, "consumed %f ms\n", (float)get_diff_in_us(time[0], time[1])/1000);
#endif
ERR_I80_MMAP:
	for (i = 0; i < 3; ++i)
		if (in_buf_vir[i])
			CVI_TRACE_VO(CVI_VO_INFO, "CVI_SYS_Munmap\n");
			//CVI_SYS_Munmap(in_buf_vir[i], in->buf.length[i]);
}

static CVI_S32 _i80_transform_frame(VB_BLK blk_in, VB_BLK *blk_out)
{
	struct vb_s *vb_in, *vb_i80;
	CVI_U32 buf_size;
	CVI_U8 byte_cnt = (gVoCtx->stPubAttr.sti80Cfg.fmt == VO_I80_FORMAT_RGB666) ? 3 : 2;

	vb_in = (struct vb_s *)blk_in;

	buf_size = ALIGN((vb_in->buf.size.u32Width * byte_cnt + 1) * 3, 32) * vb_in->buf.size.u32Height;
	*blk_out = vb_get_block_with_id(VB_INVALID_POOLID, buf_size, CVI_ID_VO);
	if (*blk_out == VB_INVALID_HANDLE) {
		vb_release_block(blk_in);
		CVI_TRACE_VO(CVI_VO_INFO, "No more vb for i80 transform.\n");
		return CVI_FAILURE;
	}
	vb_i80 = (struct vb_s *)*blk_out;

	//vb_i80->vir_addr = CVI_SYS_MmapCache(vb_i80->phy_addr, buf_size);
	if (vb_i80->vir_addr == CVI_NULL) {
		vb_release_block(blk_in);
		vb_release_block(*blk_out);
		CVI_TRACE_VO(CVI_VO_INFO, "mmap for i80 transform failed.\n");
		return CVI_FAILURE;
	}
	_i80_package_frame(vb_in, vb_i80->vir_addr, byte_cnt);
	vb_release_block(blk_in);
	//CVI_SYS_IonFlushCache(vb_i80->phy_addr, vb_i80->vir_addr, buf_size);
	//CVI_SYS_Munmap(vb_i80->vir_addr, buf_size);
	vb_i80->buf.enPixelFormat = PIXEL_FORMAT_RGB_888;
	vb_i80->buf.phy_addr[0] = vb_i80->phy_addr;
	vb_i80->buf.length[0] = buf_size;
	vb_i80->buf.stride[0] = ALIGN((vb_in->buf.size.u32Width * byte_cnt + 1) * 3, 32);
	return CVI_SUCCESS;
}

void vo_post_job(CVI_U8 vo_dev)
{
	MMF_CHN_S chn = {.enModId = CVI_ID_VO, .s32DevId = 0, .s32ChnId = 0};
	struct vb_jobs_t *jobs;

	jobs = base_get_jobs_by_chn(chn, CHN_TYPE_IN);
	if (!jobs) {
		CVI_TRACE_VO(CVI_VO_ERR, "get in jobs failed\n");
	}

	// sem_post(&jobs->sem);
	aos_event_set(&jobs->event, 0x01, AOS_EVENT_OR);
	// CVI_TRACE_VO(CVI_VO_INFO, "vo post job sem.count[%d]\n", jobs->sem.count);
}

static CVI_VOID _vo_qbuf(VB_BLK blk)
{
	int i = 0;
	struct vb_s *vb = (struct vb_s *)blk;
	struct cvi_disp_buffer *qbuf;
	MMF_CHN_S chn = {.enModId = CVI_ID_VO, .s32DevId = 0, .s32ChnId = 0};
	struct vb_jobs_t *jobs = base_get_jobs_by_chn(chn, CHN_TYPE_IN);

	vb->mod_ids |= BIT(CVI_ID_VO);
	if (gVoCtx->stPubAttr.enIntfType == VO_INTF_I80) {
		VB_BLK blk_i80;

		if (_i80_transform_frame(blk, &blk_i80) != CVI_SUCCESS) {
			CVI_TRACE_VO(CVI_VO_ERR, "i80 transform NG.\n");
			return;
		}
		vb = (struct vb_s *)blk_i80;
	}
	if (!jobs->inited) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "vb_qbuf fail, jobs not initialized yet\n");
		return;
	}

	pthread_mutex_lock(&jobs->lock);
	if (FIFO_FULL(&jobs->workq)) {
		pthread_mutex_unlock(&jobs->lock);
		vb_release_block(blk);
		CVI_TRACE_VO(CVI_VO_WARN, "vo workq is full. drop new one.\n");
		return;
	}

	if (gVoCtx->clearchnbuf) {
		//pthread_pthread_mutex_unlock(&gVoCtx->vb_jobs.lock);
		pthread_mutex_unlock(&jobs->lock);
		vb_release_block(blk);
		CVI_TRACE_VO(CVI_VO_INFO, "clearchnbuf is set. drop new one.\n");
		return;
	}

	for (i = 0; i < 3; i++) {
		CVI_TRACE_VO(CVI_VO_DBG, "vb->buf.phy_add[%d].addr=%lx\n", i, vb->buf.phy_addr[i]);
	}

	FIFO_PUSH(&jobs->workq, vb);
	pthread_mutex_unlock(&jobs->lock);

	qbuf = malloc(sizeof(struct cvi_disp_buffer));
	if (qbuf == NULL) {
		CVI_TRACE_VO(CVI_VO_INFO, "QBUF malloc size(%d) fail\n", (int)sizeof(struct cvi_disp_buffer));
		return;
	}
	memset(qbuf, 0, sizeof(struct cvi_disp_buffer));
	qbuf->buf.length = 3;
	qbuf->buf.index  = chn.s32ChnId;

	for (i = 0; i < qbuf->buf.length; i++) {
		qbuf->buf.planes[i].addr = ((struct vb_s *)blk)->buf.phy_addr[i];
	}

	vo_disp_buf_queue(gVdev, qbuf);
}

void _vo_gdc_callback(CVI_VOID *pParam, VB_BLK blk)
{
	if (!pParam) {
		pthread_mutex_unlock(&vo_gdc_lock);
		return;
	}

	pthread_mutex_unlock(&vo_gdc_lock);

	_vo_qbuf(blk);
	free(pParam);
}

static CVI_S32 _mesh_gdc_do_op_cb(enum GDC_USAGE usage, const CVI_VOID *pUsageParam,
				struct vb_s *vb_in, PIXEL_FORMAT_E enPixFormat, CVI_U64 mesh_addr,
				CVI_BOOL sync_io, CVI_VOID *pcbParam, CVI_U32 cbParamSize,
				MOD_ID_E enModId, ROTATION_E enRotation)
{
	struct mesh_gdc_cfg cfg;
	struct base_exe_m_cb exe_cb;

	memset(&cfg, 0, sizeof(cfg));
	cfg.usage = usage;
	cfg.pUsageParam = pUsageParam;
	cfg.vb_in = vb_in;
	cfg.enPixFormat = enPixFormat;
	cfg.mesh_addr = mesh_addr;
	cfg.sync_io = sync_io;
	cfg.pcbParam = pcbParam;
	cfg.cbParamSize = cbParamSize;
	cfg.enRotation = enRotation;

	exe_cb.callee = E_MODULE_LDC;
	exe_cb.caller = E_MODULE_VO;
	exe_cb.cmd_id = LDC_CB_MESH_GDC_OP;
	exe_cb.data   = &cfg;
	return base_exe_module_cb(&exe_cb);
}

#ifdef CVI_TRACE_VOOFILE
void _vo_update_chnRealFrameRate(VO_CHN_STATUS_S *pstVoChnStatus)
{
	CVI_U64 duration, curTimeUs;
	struct timespec curTime;

	clock_gettime(CLOCK_REALTIME, &curTime);
	curTimeUs = curTime.tv_sec * 1000000L + curTime.tv_nsec / 1000L;
	duration = curTimeUs - pstVoChnStatus->u64PrevTime;

	if (duration >= 1000000) {
		pstVoChnStatus->u32RealFrameRate = pstVoChnStatus->u32frameCnt;
		pstVoChnStatus->u32frameCnt = 0;
		pstVoChnStatus->u64PrevTime = curTimeUs;
	}
}
#endif

static void *_vo_disp_thread(void *arg)
{
	struct cvi_vo_dev *vdev = (struct cvi_vo_dev *)arg;
	int ret = 0;
	enum E_VO_TH th_id = E_VO_TH_DISP;
	VB_BLK blk;
	struct vb_s *vb;
	MMF_CHN_S chn = {.enModId = CVI_ID_VO, .s32DevId = 0, .s32ChnId = 0};
	struct vb_jobs_t *jobs = base_get_jobs_by_chn(chn, CHN_TYPE_IN);
	u32 i = 0;
	SIZE_S size;
	VB_CAL_CONFIG_S stVbCalConfig;
	struct _vo_gdc_cb_param cb_param = { .chn = chn, .usage = GDC_USAGE_LDC};
#ifdef CVI_TRACE_VOOFILE
	struct timespec time[2];
	CVI_U32 sum = 0, duration, duration_max = 0, duration_min = 1000 * 1000;
	CVI_U8 count = 0;
#endif
	CVI_U32 actl_flags;
	struct sclr_disp_cfg *cfg;
	struct timespec ts;

	if (!jobs) {
		CVI_TRACE_VO(CVI_VO_ERR, "get in jobs failed\n");
		return NULL;
	}

	while (1) {
		if (vdev->vo_th[th_id].flag == 2) {
			CVI_TRACE_VO(CVI_VO_INFO, "%s exit\n", vdev->vo_th[th_id].th_name);
			atomic_set(&vdev->vo_th[th_id].thread_exit, 1);
			pthread_exit(NULL);
		}

		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_nsec += SEM_WAIT_TIMEOUT_MS * 1000 * 1000;
		ts.tv_sec += ts.tv_nsec / 1000000000;
		ts.tv_nsec %= 1000000000;

		ret = sem_timedwait(&jobs->sem, &ts);
		if (ret != 0) {
			CVI_TRACE_VO(CVI_VO_INFO, "Disp thread expired time, loop\n");
			continue;
		}

		if (gVoCtx->pause) {
			CVI_TRACE_VO(CVI_VO_INFO, "pause and skip update.\n");
			continue;
		}

		blk = base_mod_jobs_waitq_pop(chn, CHN_TYPE_IN);
		if (blk == (VB_BLK)VB_INVALID_HANDLE) {
			CVI_TRACE_VO(CVI_VO_INFO, "No more vb for dequeue.\n");
			continue;
		}
		vb = (struct vb_s *)blk;
		CVI_TRACE_VO(CVI_VO_DBG, "waitq pop blk[0x%lx]\n", blk);

		for (i = 0; i < 3; i++) {
			CVI_TRACE_VO(CVI_VO_DBG, "vb->buf.phy_add[%d].addr=%lx\n", i, vb->buf.phy_addr[i]);
		}

		gVoCtx->u64DisplayPts[chn.s32DevId][chn.s32ChnId] = vb->buf.u64PTS;

		cfg = sclr_disp_get_cfg();
		cfg->mem.width = gVoCtx->rect_crop.width = gVoCtx->stChnAttr.stRect.u32Width;
		cfg->mem.height = gVoCtx->rect_crop.height = gVoCtx->stChnAttr.stRect.u32Height;
		cfg->mem.start_x = gVoCtx->rect_crop.left = vb->buf.s16OffsetLeft;
		cfg->mem.start_y = gVoCtx->rect_crop.top = vb->buf.s16OffsetTop;
		if (gVoCtx->enRotation == ROTATION_0) {
			if ((vb->buf.s16OffsetLeft != gVoCtx->rect_crop.left)
				|| (vb->buf.s16OffsetTop != gVoCtx->rect_crop.top)) {
				CVI_TRACE_VO(CVI_VO_INFO, "Crop Area (%d,%d,%d,%d)\n", cfg->mem.start_x,
				cfg->mem.start_y, cfg->mem.width, cfg->mem.height);

				sclr_disp_set_mem(&cfg->mem);
			}
			_vo_qbuf(blk);
		} else {
			pthread_mutex_lock(&vo_gdc_lock);

			size.u32Width = ALIGN(vb->buf.size.u32Width, LDC_ALIGN);
			size.u32Height = ALIGN(vb->buf.size.u32Height, LDC_ALIGN);
			((struct vb_s *)blk)->buf.size = size;
			if (gVoCtx->enRotation == ROTATION_90) {
				cfg->mem.start_x = gVoCtx->rect_crop.left
								= size.u32Height - gVoCtx->stChnAttr.stRect.u32Width;
				cfg->mem.start_y = 0;
				sclr_disp_set_mem(&cfg->mem);
			} else if (gVoCtx->enRotation == ROTATION_270) {
				cfg->mem.start_x = gVoCtx->rect_crop.left = 0;
				cfg->mem.start_y = gVoCtx->rect_crop.top
								= size.u32Width - gVoCtx->stChnAttr.stRect.u32Height;
				sclr_disp_set_mem(&cfg->mem);
			}
			CVI_TRACE_VO(CVI_DBG_INFO, "rect_crop(%d %d %d %d)\n"
				, gVoCtx->rect_crop.width, gVoCtx->rect_crop.height
				, gVoCtx->rect_crop.left, gVoCtx->rect_crop.top);

			COMMON_GetPicBufferConfig(size.u32Width, size.u32Height, gVoCtx->stLayerAttr.enPixFormat
				, DATA_BITWIDTH_8, COMPRESS_MODE_NONE, LDC_ALIGN, &stVbCalConfig);
			for (i = 0; i < stVbCalConfig.plane_num; ++i) {
				((struct vb_s *)blk)->buf.length[i] = ALIGN((i == 0)
					? stVbCalConfig.u32MainYSize
					: stVbCalConfig.u32MainCSize,
					stVbCalConfig.u16AddrAlign);
			}
			if (_mesh_gdc_do_op_cb(GDC_USAGE_ROTATION
				, NULL
				, vb
				, gVoCtx->stLayerAttr.enPixFormat
				, gVoCtx->mesh.paddr
				, CVI_TRUE, &cb_param
				, sizeof(cb_param)
				, CVI_ID_VO
				, gVoCtx->enRotation) != CVI_SUCCESS) {
				pthread_mutex_unlock(&vo_gdc_lock);
				CVI_TRACE_VO(CVI_VO_ERR, "gdc rotation failed.\n");
				continue;
			}
		}
		// except for i80, vo needs to keep at least one buf for display.
		// Thus, no buf done if there is only one buffer.
		if (FIFO_SIZE(&jobs->workq) == 1 && (gVoCtx->stPubAttr.enIntfType != VO_INTF_I80) &&
			vdev->vo_th[th_id].flag == 2) {
			CVI_TRACE_VO(CVI_VO_INFO, "vo keep one buf for display.\n");
			continue;
		}

		//check vb buffer done, wait for vo_wake_up_th()
		ret = aos_event_get(&vdev->vo_th[th_id].event, 0x01, AOS_EVENT_OR_CLEAR, &actl_flags,
							SEM_WAIT_TIMEOUT_MS);
		if (ret != 0) {
			CVI_TRACE_VO(CVI_VO_ERR, "interrupt time, loop\n");
			continue;
		}

		if (vdev->vo_th[th_id].flag == 0) {
			CVI_TRACE_VO(CVI_VO_WARN, "flag=0\n");
			continue;
		}
		if (vdev->vo_th[th_id].flag == 2) {
			CVI_TRACE_VO(CVI_VO_INFO, "%s exit\n", vdev->vo_th[th_id].th_name);
			atomic_set(&vdev->vo_th[th_id].thread_exit, 1);
			pthread_exit(NULL);
		}
		vdev->vo_th[th_id].flag = 0;

		vb_dqbuf(chn, CHN_TYPE_IN, &blk);

		if (blk == VB_INVALID_HANDLE) {
			CVI_TRACE_VO(CVI_VO_WARN, "can't get vb-blk.\n");
		} else {
			vb = (struct vb_s *)blk;
			CVI_TRACE_VO(CVI_VO_DBG, "vb_done_handler,[0x%lx]\n", vb->phy_addr);
			gVoCtx->u64PreDonePts[chn.s32DevId][chn.s32ChnId] = ((struct vb_s *)blk)->buf.u64PTS;
			gVoCtx->chnStatus[chn.s32DevId][chn.s32ChnId].u32frameCnt++;

			vb_done_handler(chn, CHN_TYPE_IN, blk);
		}

		if(plogodata != NULL)
		{
			sys_ion_free((CVI_U64)plogodata);
			CVI_TRACE_VO(CVI_VO_INFO, "sys_ion_free,[0x%lx]\n", (CVI_U64)plogodata);
			plogodata = NULL;
		}

#ifdef CVI_TRACE_VOOFILE
		clock_gettime(CLOCK_REALTIME, &time[1]);
		duration = get_diff_in_us(time[0], time[1]);
		duration_max = MAX(duration, duration_max);
		duration_min = MIN(duration, duration_min);
		sum += duration;
		if (++count == 100) {
			CVI_TRACE_VO(CVI_VO_INFO, "VO duration(ms): average(%d), max(%d) min(%d)\n"
				, sum / count / 1000, duration_max / 1000, duration_min / 1000);
			count = 0;
			sum = duration_max = 0;
			duration_min = 1000 * 1000;
		}
#endif
	}

	return NULL;
}

int vo_destroy_thread(struct cvi_vo_dev *vdev, enum E_VO_TH th_id)
{
	int rc = 0;

	if (th_id < 0 || th_id >= E_VO_TH_MAX) {
		CVI_TRACE_VO(CVI_VO_ERR, "No such thread_id(%d)\n", th_id);
		return -1;
	}

	if (vdev->vo_th[th_id].w_thread != NULL) {
		// kthread_stop(vdev->vo_th[th_id].w_thread);
		vdev->vo_th[E_VO_TH_DISP].flag = 2;
		// sem_post(&vdev->vo_th[E_VO_TH_DISP].sem);
		aos_event_set(&vdev->vo_th[E_VO_TH_DISP].event, 0x01, AOS_EVENT_OR);
		while (atomic_read(&vdev->vo_th[th_id].thread_exit) == 0) {
			CVI_TRACE_VO(CVI_VO_INFO, "wait for %s exit\n", vdev->vo_th[th_id].th_name);
			usleep(5 * 1000);
		}
		pthread_join(vdev->vo_th[th_id].w_thread, NULL);
		vdev->vo_th[th_id].w_thread = NULL;
	}

	// sem_destroy(&vdev->vo_th[E_VO_TH_DISP].sem);
	aos_event_free(&vdev->vo_th[E_VO_TH_DISP].event);
	pthread_mutex_destroy(&vo_gdc_lock);
	return rc;
}

int vo_create_thread(struct cvi_vo_dev *vdev, enum E_VO_TH th_id)
{
	struct sched_param param;
	pthread_attr_t attr;
	pthread_condattr_t cattr;
	int rc = 0;

	if (th_id < 0 || th_id >= E_VO_TH_MAX) {
		CVI_TRACE_VO(CVI_VO_ERR, "_vo_create_thread fail\n");
		return -1;
	}
	param.sched_priority = VIP_RT_PRIO;

	if (vdev->vo_th[th_id].w_thread == NULL) {
		switch (th_id) {
		case E_VO_TH_DISP:
			memcpy(vdev->vo_th[th_id].th_name, "cvitask_disp", sizeof("cvitask_disp"));
			vdev->vo_th[th_id].th_handler = _vo_disp_thread;
			break;

		default:
			CVI_TRACE_VO(CVI_VO_ERR, "No such thread(%d)\n", th_id);
			return -1;
		}

		pthread_attr_init(&attr);
		pthread_condattr_init(&cattr);
		pthread_condattr_setclock(&cattr, CLOCK_REALTIME);
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		pthread_attr_setschedparam(&attr, &param);
		pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

		// sem_init(&vdev->vo_th[th_id].sem, 0, 0);
		aos_event_new(&vdev->vo_th[th_id].event, 0);
		vdev->vo_th[th_id].flag = 0;
		atomic_set(&vdev->vo_th[th_id].thread_exit, 0);

		pthread_create(&vdev->vo_th[th_id].w_thread, &attr,
					vdev->vo_th[th_id].th_handler,
					(void *)vdev);
		pthread_setname_np(vdev->vo_th[th_id].w_thread, vdev->vo_th[th_id].th_name);

		if (!vdev->vo_th[th_id].w_thread) {
			CVI_TRACE_VO(CVI_VO_ERR, "Unable to start %s.\n", vdev->vo_th[th_id].th_name);
			return -1;
		}
		pthread_mutex_init(&vo_gdc_lock, NULL);

	}

	return rc;
}

int vo_start_streaming(struct cvi_vo_dev *vdev)
{
	int rc = 0;
	struct sclr_top_cfg *cfg = sclr_top_get_cfg();

	//dprintk(VIP_VB2, "+\n");

	//_vo_jobs_qbuf();

	cfg->disp_enable = true;
	sclr_top_set_cfg(cfg);
	sclr_disp_enable_window_bgcolor(true);

	vdev->align = VIP_ALIGNMENT;
	vdev->seq_count = 0;
	vdev->frame_number = 0;

	if (vdev->disp_interface != CVI_VIP_DISP_INTF_I80)
		sclr_disp_tgen_enable(true);

	atomic_set(&vdev->disp_streamon, 1);
	atomic_set(&vdev->num_rdy, 0);

	//ToDo need to remove this code after enable/disable chn is ok
	//vo_create_thread(vdev, E_VO_TH_DISP);

	return rc;

}
int vo_stop_streaming(struct cvi_vo_dev *vdev)
{
	int rc = 0;

	//struct cvi_disp_vdev *ddev = vb2_get_drv_priv(vq);
	struct cvi_disp_buffer *b;
	unsigned long flags;
	struct sclr_top_cfg *cfg = sclr_top_get_cfg();

	cfg->disp_enable = false;
	sclr_top_set_cfg(cfg);

	sclr_disp_enable_window_bgcolor(true);
	//dprintk(VIP_VB2, "+\n");

	// if (!smooth && (vdev->disp_interface != CVI_VIP_DISP_INTF_LVDS))
	// 	sclr_disp_tgen_enable(false);
	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */
	spin_lock_irqsave(&vdev->rdy_lock, flags);
	while (!list_empty(&vdev->rdy_queue)) {
		b = list_first_entry(&vdev->rdy_queue,
			struct cvi_disp_buffer, list);
		list_del_init(&b->list);
		free(b);
	}

	atomic_set(&vdev->num_rdy, 0);
	INIT_LIST_HEAD(&vdev->rdy_queue);
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	atomic_set(&vdev->disp_streamon, 0);
	memset(&vdev->compose_out, 0, sizeof(vdev->compose_out));

	return rc;
}

static long _vo_s_ctrl(struct cvi_vo_dev *vdev, struct vo_ext_control *p)
{
	u32 id = p->id;
	long rc = -EINVAL;

	switch (id) {
	case VO_IOCTL_SDK_CTRL:
	{
		rc = vo_sdk_ctrl(vdev, p);
		break;
	}

	case VO_IOCTL_START_STREAMING:
	{
		if (vo_start_streaming(vdev)) {
			CVI_TRACE_VO(CVI_VO_ERR, "Failed to vo start streaming\n");
			break;
		}
		rc = 0;
		break;
	}

	case VO_IOCTL_STOP_STREAMING:
	{
		if (vo_stop_streaming(vdev)) {
			CVI_TRACE_VO(CVI_VO_ERR, "Failed to vo stop streaming\n");
			break;
		}

		rc = 0;
		break;
	}
	case VO_IOCTL_ENQ_WAITQ:
	{
		CVI_S32 ret;
		const uint32_t buf_len = 0x100000;
		MMF_CHN_S chn = {.enModId = CVI_ID_VO, .s32DevId = 0, .s32ChnId = 0};
		uint32_t i, j;
		VB_BLK blk[3] = {VB_INVALID_HANDLE, VB_INVALID_HANDLE, VB_INVALID_HANDLE};
		struct vb_s *vb;

		gVoCtx->u32DisBufLen = 3;

		if (0 == job_init) {
			base_mod_jobs_init(chn, CHN_TYPE_OUT, gVoCtx->u32DisBufLen - 1, 2, 0);
			job_init = 1;
		}
		// get blk and qbuf to workq
		// ------------------------------------------
		for (i = 0; i < 3; i++) {
			blk[i] = vb_get_block_with_id(VB_INVALID_POOLID, buf_len, CVI_ID_VO);
			vb = (struct vb_s *)blk[i];

			if (blk[i] == VB_INVALID_HANDLE) {
				CVI_TRACE_VO(CVI_VO_ERR, "vb_get_block_with_id fail, blk idx(%d)\n", i);

				break;
			}
			CVI_TRACE_VO(CVI_VO_DBG, "blk[%d]=(0x%lx)\n", i, blk[i]);

			for (j = 0; j < 3; j++) {
				CVI_TRACE_VO(CVI_VO_INFO, "vb->buf.phy_addr[%d].addr=%lx\n", j, vb->buf.phy_addr[j]);
			}

			ret = vb_qbuf(chn, CHN_TYPE_IN, blk[i]);
			if (ret != CVI_SUCCESS) {
				CVI_TRACE_VO(CVI_VO_ERR, "vb_qbuf fail\n");

				break;
			}
		}
		//vb_release_block(blk[i]);

		rc = 0;
		break;
	}

	case VO_IOCTL_SET_DV_TIMINGS:
	{
		struct vo_dv_timings *timings, _timings_;
		struct sclr_disp_timing timing;

		timings = &_timings_;
		memcpy(timings, (void *)p->ptr, sizeof(struct vo_dv_timings));
#if 0//TODO
		if (!list_empty(&vdev->rdy_queue))
			return -EBUSY;
#endif
		vdev->dv_timings = *timings;
		vdev->sink_rect.width = timings->bt.width;
		vdev->sink_rect.height = timings->bt.height;
		vdev->compose_out = vdev->sink_rect;
		CVI_TRACE_VO(CVI_VO_INFO, "timing %d-%d\n", timings->bt.width, timings->bt.height);

		vo_fill_disp_timing(&timing, &timings->bt);
		sclr_disp_set_timing(&timing);

		rc = 0;
		break;
	}
	case VO_IOCTL_SEL_TGT_COMPOSE:
	{
		struct vo_rect area;

		memcpy(&area, p->ptr, sizeof(area));

		if (memcmp(&vdev->compose_out, &area, sizeof(area))) {
			struct sclr_rect rect;

			rect.x = area.left;
			rect.y = area.top;
			rect.w = area.width;
			rect.h = area.height;

			CVI_TRACE_VO(CVI_VO_INFO, "Compose Area (%d,%d,%d,%d)\n", rect.x, rect.y, rect.w, rect.h);
			if (sclr_disp_set_rect(rect) == 0)
				vdev->compose_out = area;
		}
		rc = 0;
		break;
	}
	case VO_IOCTL_SEL_TGT_CROP:
	{
		struct sclr_disp_cfg *cfg;
		struct vo_rect area;

		memcpy(&area, (void *)p->ptr, sizeof(struct vo_rect));

		cfg = sclr_disp_get_cfg();
		cfg->mem.start_x = area.left;
		cfg->mem.start_y = area.top;
		cfg->mem.width	 = area.width;
		cfg->mem.height  = area.height;

		CVI_TRACE_VO(CVI_VO_INFO, "Crop Area (%d,%d,%d,%d)\n", cfg->mem.start_x,
												cfg->mem.start_y,
												cfg->mem.width,
												cfg->mem.height);
		sclr_disp_set_mem(&cfg->mem);
		vdev->crop_rect = area;

		rc = 0;
		break;
	}
	case VO_IOCTL_SET_ALIGN:
	{
		if (p->value >= VIP_ALIGNMENT) {
			vdev->align = p->value;
			CVI_TRACE_VO(CVI_VO_INFO, "Set Align(%d)\n", vdev->align);
		}
		rc = 0;
		break;
	}
	case VO_IOCTL_SET_RGN:
	{
		struct sclr_disp_timing *timing = sclr_disp_get_timing();
		struct sclr_size size;
		struct cvi_rgn_cfg cfg;

		memcpy(&cfg, p->ptr, sizeof(struct cvi_rgn_cfg));
		size.w = timing->hfde_end - timing->hfde_start + 1;
		size.h = timing->vfde_end - timing->vfde_start + 1;
		vo_set_rgn_cfg(SCL_GOP_DISP, &cfg, &size);

		rc = 0;
		break;
	}
	case VO_IOCTL_I80_SW_MODE:
	{
		sclr_i80_sw_mode(p->value);

		rc = 0;
		break;
	}
	case VO_IOCTL_I80_CMD:
	{
		sclr_i80_packet(p->value);

		rc = 0;
		break;
	}
	case VO_IOCTL_SET_CUSTOM_CSC:
	{
		struct sclr_csc_matrix cfg;

		memcpy(&cfg, p->ptr, sizeof(struct sclr_csc_matrix));
		sclr_disp_set_csc(&cfg);

		rc = 0;
		break;
	}
	case VO_IOCTL_SET_CLK:
	{
		if (p->value < 8000) {
			CVI_TRACE_VO(CVI_VO_ERR, "V4L2_CID_DV_VIP_DISP_SET_CLK clk(%d) less than 8000 kHz.\n",
				p->value);
			break;
		}
		dphy_dsi_set_pll(p->value, 4, 24);

		rc = 0;
		break;
	}
	case VO_IOCTL_INTR:
	{
		static bool service_isr = true;
		union sclr_intr intr_mask;

		service_isr = !service_isr;
		intr_mask = sclr_get_intr_mask();
		intr_mask.b.disp_frame_end = (service_isr) ? 1 : 0;
		sclr_set_intr_mask(intr_mask);

		rc = 0;
		break;
	}
	case VO_IOCTL_OUT_CSC:
	{
		if (p->value >= SCL_CSC_601_LIMIT_YUV2RGB &&
			p->value <= SCL_CSC_709_FULL_YUV2RGB) {
			CVI_TRACE_VO(CVI_VO_ERR, "invalid disp-out-csc(%d)\n", p->value);
			break;
		}
		sclr_disp_set_out_csc(p->value);

		rc = 0;
		break;
	}
	case VO_IOCTL_PATTERN:
	{
		if (p->value >= CVI_VIP_PAT_MAX) {
			CVI_TRACE_VO(CVI_VO_ERR, "invalid disp-pattern(%d)\n",
					p->value);
			break;
		}
		sclr_disp_set_pattern(patterns[p->value].type, patterns[p->value].color, patterns[p->value].rgb);

		rc = 0;
		break;
	}
	case VO_IOCTL_FRAME_BGCOLOR:
	{
		u16 u16_rgb[3], r, g, b;

		memcpy(&u16_rgb[0], p->ptr, sizeof(u16_rgb));

		r = u16_rgb[0];
		g = u16_rgb[1];
		b = u16_rgb[2];
		CVI_TRACE_VO(CVI_VO_INFO, "Set Frame BG color (R,G,B) = (%x,%x,%x)\n", r, g, b);

		sclr_disp_set_frame_bgcolor(r, g, b);

		rc = 0;
		break;

	}
	case VO_IOCTL_WINDOW_BGCOLOR:
	{
		u16 u16_rgb[3], r, g, b;

		memcpy(&u16_rgb[0], p->ptr, sizeof(u16_rgb));

		r = u16_rgb[0];
		g = u16_rgb[1];
		b = u16_rgb[2];
		CVI_TRACE_VO(CVI_VO_INFO, "Set window BG color 2(R,G,B) = (%d,%d,%d)\n", r, g, b);

		sclr_disp_set_window_bgcolor(r, g, b);

		rc = 0;
		break;
	}
	case VO_IOCTL_ONLINE:
	{
		if (atomic_read(&vdev->disp_streamon) == 1) {
			CVI_TRACE_VO(CVI_VO_ERR, "V4L2_CID_DV_VIP_DISP_ONLINE can't be control if streaming.\n");

			rc = 0;
			break;
		}
		//memcpy(&vdev->disp_online, (p->value), sizeof(bool));
		vdev->disp_online = p->value;

		sclr_ctrl_set_disp_src(vdev->disp_online);

		rc = 0;
		break;
	}
	case VO_IOCTL_INTF:
	{
		struct cvi_disp_intf_cfg *cfg, _cfg_;

		cfg = &_cfg_;
		memcpy(cfg, p->ptr, sizeof(struct cvi_disp_intf_cfg));

		if (smooth) {
			CVI_TRACE_VO(CVI_VO_DBG, "V4L2_CID_DV_VIP_DISP_INTF won't apply if smooth.\n");
			sclr_disp_reg_force_up();
			vdev->disp_interface = cfg->intf_type;
			rc = 0;
			break;
		}

#if 0//TODO
		if (vb2_is_streaming(&ddev->vb_q)) {
			dprintk(VIP_ERR, "V4L2_CID_DV_VIP_DISP_INTF can't be control if streaming.\n");
			break;
		}
#endif
		if (atomic_read(&vdev->disp_streamon) == 1) {
			CVI_TRACE_VO(CVI_VO_ERR, "V4L2_CID_DV_VIP_DISP_ONLINE can't be control if streaming.\n");
			break;
		}

		if (cfg->intf_type == CVI_VIP_DISP_INTF_DSI) {
			CVI_TRACE_VO(CVI_VO_INFO, "MIPI use mipi_tx to control.\n");
			//return rc;
		} else if (cfg->intf_type == CVI_VIP_DISP_INTF_LVDS) {
			int i = 0;
			union sclr_lvdstx lvds_reg;
			bool data_en[LANE_MAX_NUM] = {false, false, false, false, false};

			for (i = 0; i < LANE_MAX_NUM; i++) {
				if ((cfg->lvds_cfg.lane_id[i] < 0) ||
					(cfg->lvds_cfg.lane_id[i] >= LANE_MAX_NUM)) {
					dphy_dsi_set_lane(i, DSI_LANE_MAX, false, false);
					continue;
				}
				dphy_dsi_set_lane(i, cfg->lvds_cfg.lane_id[i],
						  cfg->lvds_cfg.lane_pn_swap[i], false);
				if (cfg->lvds_cfg.lane_id[i] != MIPI_TX_LANE_CLK) {
					data_en[cfg->lvds_cfg.lane_id[i] - 1] = true;
				}
			}
			dphy_dsi_lane_en(true, data_en, false);
			sclr_disp_set_intf(SCLR_VO_INTF_LVDS);

			if (cfg->lvds_cfg.pixelclock == 0) {
				CVI_TRACE_VO(CVI_VO_ERR, "lvds pixelclock 0 invalid\n");
				//return rc;
			}
			lvds_reg.b.out_bit = cfg->lvds_cfg.out_bits;
			lvds_reg.b.vesa_mode = cfg->lvds_cfg.mode;
			if (cfg->lvds_cfg.chn_num == 1)
				lvds_reg.b.dual_ch = 0;
			else if (cfg->lvds_cfg.chn_num == 2)
				lvds_reg.b.dual_ch = 1;
			else {
				lvds_reg.b.dual_ch = 0;
				CVI_TRACE_VO(CVI_VO_ERR, "invalid lvds chn_num(%d). Use 1 instead."
					, cfg->lvds_cfg.chn_num);
			}
			lvds_reg.b.vs_out_en = cfg->lvds_cfg.vs_out_en;
			lvds_reg.b.hs_out_en = cfg->lvds_cfg.hs_out_en;
			lvds_reg.b.hs_blk_en = cfg->lvds_cfg.hs_blk_en;
			lvds_reg.b.ml_swap = cfg->lvds_cfg.msb_lsb_data_swap;
			lvds_reg.b.ctrl_rev = cfg->lvds_cfg.serial_msb_first;
			lvds_reg.b.oe_swap = cfg->lvds_cfg.even_odd_link_swap;
			lvds_reg.b.en = cfg->lvds_cfg.enable;
			dphy_dsi_analog_setting(true);
			dphy_lvds_set_pll(cfg->lvds_cfg.pixelclock, cfg->lvds_cfg.chn_num);
			sclr_lvdstx_set(lvds_reg);
		} else if (cfg->intf_type == CVI_VIP_DISP_INTF_I80) {
			union sclr_bt_enc enc;
			union sclr_bt_sync_code sync;

			sclr_disp_set_intf(SCLR_VO_INTF_I80);
			enc.raw = 0;
			enc.b.fmt_sel = 2;
			enc.b.clk_inv = 1;
			sync.raw = 0;
			sync.b.sav_vld = 0x80;
			sync.b.sav_blk = 0xab;
			sync.b.eav_vld = 0x9d;
			sync.b.eav_blk = 0xb6;
			sclr_bt_set(enc, sync);
		} else if (cfg->intf_type == CVI_VIP_DISP_INTF_BT) {
			union sclr_bt_enc enc;
			union sclr_bt_sync_code sync;

			if (cfg->bt_cfg.mode == BT_MODE_1120)
				sclr_disp_set_intf(SCLR_VO_INTF_BT1120);
			else if (cfg->bt_cfg.mode == BT_MODE_656)
				sclr_disp_set_intf(SCLR_VO_INTF_BT656);
			else if (cfg->bt_cfg.mode == BT_MODE_601)
				sclr_disp_set_intf(SCLR_VO_INTF_BT601);
			else {
				CVI_TRACE_VO(CVI_VO_ERR, "invalid bt-mode(%d)\n", cfg->bt_cfg.mode);
				//return rc;
			}

			if (cfg->bt_cfg.mode == BT_MODE_1120) {
				dphy_dsi_set_pll(cfg->bt_cfg.pixelclock, 4, 24);
				vip_sys_clk_setting(0x10010);
			} else if (cfg->bt_cfg.mode == BT_MODE_656) {
				dphy_dsi_set_pll(cfg->bt_cfg.pixelclock * 2, 4, 24);
				vip_sys_clk_setting(0x10000);
			} else if (cfg->bt_cfg.mode == BT_MODE_601) {
				dphy_dsi_set_pll(cfg->bt_cfg.pixelclock * 2, 4, 24);
				vip_sys_clk_setting(0x10000);
			}

			//set csc value
			sclr_disp_set_out_csc(SCL_CSC_601_FULL_RGB2YUV);

			enc.raw = 0;
			enc.b.fmt_sel = cfg->bt_cfg.mode;
			sync.b.sav_vld = 0x80;
			sync.b.sav_blk = 0xab;
			sync.b.eav_vld = 0x9d;
			sync.b.eav_blk = 0xb6;
			sclr_bt_set(enc, sync);
		} else {
			CVI_TRACE_VO(CVI_VO_ERR, "invalid disp-intf(%d)\n", cfg->intf_type);
			//return rc;
		}
		sclr_disp_reg_force_up();

		vdev->disp_interface = cfg->intf_type;

		rc = 0;
		break;
	}
	case VO_IOCTL_ENABLE_WIN_BGCOLOR:
	{
		vdev->bgcolor_enable = p->value;
		sclr_disp_enable_window_bgcolor(p->value);

		rc = 0;
		break;
	}
	case VO_IOCTL_GAMMA_LUT_UPDATE:
	{
		int i = 0;
		struct sclr_disp_gamma_attr gamma_attr_sclr;
		VO_GAMMA_INFO_S gamma_attr;

		memcpy(&gamma_attr, (void *)p->ptr, sizeof(gamma_attr));

		gamma_attr_sclr.enable = gamma_attr.enable;
		gamma_attr_sclr.pre_osd = gamma_attr.osd_apply;

		for (i = 0; i < SCL_DISP_GAMMA_NODE; ++i) {
			gamma_attr_sclr.table[i] = gamma_attr.value[i];
		}

		sclr_disp_gamma_ctrl(gamma_attr_sclr.enable, gamma_attr_sclr.pre_osd);
		sclr_disp_gamma_lut_update(gamma_attr_sclr.table, gamma_attr_sclr.table, gamma_attr_sclr.table);

		rc = 0;
		break;
	}

	case VO_IOCTL_GET_SHARE_MEM:
		p->ptr = vdev->shared_mem;
		rc = 0;
		break;

	default:
		break;
	}

	return rc;
}

static long _vo_g_ctrl(struct cvi_vo_dev *vdev, struct vo_ext_control *p)
{
	u32 id = p->id;
	long rc = -EINVAL;

	switch (id) {
	case VO_IOCTL_GET_DV_TIMINGS:{
		memcpy(p->ptr, &vdev->dv_timings, sizeof(struct vo_dv_timings));

		return 0;
	}
	break;

	case VO_IOCTL_GET_VLAYER_SIZE:{

		struct sclr_disp_timing *timing = sclr_disp_get_timing();
		struct dsize {
			u32 width;
			u32 height;
		} vsize;

		vsize.width = timing->hfde_end - timing->hfde_start + 1;
		vsize.height = timing->vfde_end - timing->vfde_start + 1;

		memcpy(p->ptr, &vsize, sizeof(struct dsize));
		rc = 0;
	}
	break;

	case VO_IOCTL_GET_INTF_TYPE:{
		enum sclr_vo_sel vo_sel;

		vo_sel = sclr_disp_mux_get();
		memcpy(p->ptr, &vo_sel, sizeof(vo_sel));
		rc = 0;
	}
	break;

	case VO_IOCTL_GET_PANEL_STATUS:{
		int is_init = 0;

		if (sclr_disp_mux_get() == SCLR_VO_SEL_I80) {
			is_init = sclr_disp_check_i80_enable();
		} else {
			is_init = sclr_disp_check_tgen_enable();
		}
		memcpy(p->ptr, &is_init, sizeof(is_init));
		rc = 0;
	}
	break;

	case VO_IOCTL_GAMMA_LUT_READ:{
		int i = 0;
		VO_GAMMA_INFO_S gamma_attr;
		struct sclr_disp_gamma_attr gamma_attr_sclr;

		sclr_disp_gamma_lut_read(&gamma_attr_sclr);

		gamma_attr.enable = gamma_attr_sclr.enable;
		gamma_attr.osd_apply = gamma_attr_sclr.pre_osd;

		for (i = 0; i < SCL_DISP_GAMMA_NODE; ++i) {
			gamma_attr.value[i] = gamma_attr_sclr.table[i];
		}

		memcpy((void *)p->ptr, &gamma_attr, sizeof(VO_GAMMA_INFO_S));

		rc = 0;
	}
	break;

	default:
		break;
	}

	return rc;
}

long vo_ioctl(u_int cmd, u_long arg)
{
	struct cvi_vo_dev *vdev = gVdev;
	int ret = 0;
	struct vo_ext_control p;

	memcpy(&p, (void *)arg, sizeof(struct vo_ext_control));

	switch (cmd) {
	case VO_IOC_G_CTRL:
	{
		ret = _vo_g_ctrl(vdev, &p);
		break;
	}
	case VO_IOC_S_CTRL:
	{
		ret = _vo_s_ctrl(vdev, &p);
		break;
	}
	default:
		ret = -ENOTTY;
		break;
	}

	memcpy((void *)arg, &p, sizeof(struct vo_ext_control));

	return ret;

}

static void _vo_sw_init(struct cvi_vo_dev *vdev)
{
	spin_lock_init(&vdev->rdy_lock);
	INIT_LIST_HEAD(&vdev->rdy_queue);

	atomic_set(&vdev->num_rdy, 0);
	atomic_set(&vdev->disp_streamon, 0);
}

int vo_open(void)
{
	int ret = 0;

	if (!atomic_read(&dev_open_cnt)) {
		struct cvi_vo_dev *vdev = gVdev;

		_vo_sw_init(vdev);

		sclr_disp_reg_shadow_sel(false);
		if (!smooth)
			sclr_disp_set_cfg(sclr_disp_get_cfg());
	}

	atomic_inc(&dev_open_cnt);
	return ret;

}

void _vo_sdk_release(struct cvi_vo_dev *vdev)
{
	vo_disable_chn(0, 0);
	vo_disablevideolayer(0);
	vo_disable(0);

	memset(gVoCtx, 0, sizeof(*gVoCtx));
}

int vo_close(void)
{
	int ret = 0;

	atomic_dec(&dev_open_cnt);

	if (!atomic_read(&dev_open_cnt)) {
		_vo_sdk_release(gVdev);
	}

	return ret;
}

int vo_mmap(struct cvi_vo_dev *pvdev)
{
	struct cvi_vo_dev *vdev = pvdev;

	UNUSED(vdev);
	#if 0
	unsigned long vm_start = vma->vm_start;
	unsigned int vm_size = vma->vm_end - vma->vm_start;
	unsigned int offset = vma->vm_pgoff << PAGE_SHIFT;
	void *pos = vdev->shared_mem;

	if (offset < 0 || (vm_size + offset) > VO_SHARE_MEM_SIZE)
		return -EINVAL;

	while (vm_size > 0) {
		if (remap_pfn_range(vma, vm_start, virt_to_pfn(pos), PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
		//CVI_TRACE_VO(CVI_VO_DBG, "vo proc mmap vir(%p) phys(%#lx)\n", pos, virt_to_phys((void *) pos));
		vm_start += PAGE_SIZE;
		pos += PAGE_SIZE;
		vm_size -= PAGE_SIZE;
	}
	#endif
	return 0;
}

int _vo_call_cb(u32 m_id, u32 cmd_id, void *data)
{
	struct base_exe_m_cb exe_cb;

	exe_cb.callee = m_id;
	exe_cb.caller = E_MODULE_VO;
	exe_cb.cmd_id = cmd_id;
	exe_cb.data   = (void *)data;

	return base_exe_module_cb(&exe_cb);
}

int vo_cb(void *dev, enum ENUM_MODULES_ID caller, u32 cmd, void *arg)
{
	struct cvi_vo_dev *vdev = (struct cvi_vo_dev *)dev;
	int rc = -1;

	switch (cmd) {
	case VO_CB_IRQ_HANDLER:
	{
		union sclr_intr intr_status = *(union sclr_intr *)arg;

		vo_irq_handler(vdev, intr_status);

		rc = 0;
		break;
	}

	case VO_CB_GET_RGN_HDLS:
	{
		struct _rgn_hdls_cb_param *attr = (struct _rgn_hdls_cb_param *)arg;
		VO_LAYER VoLayer = attr->stChn.s32DevId;
		VO_CHN VoChn = attr->stChn.s32ChnId;
		RGN_HANDLE *pstHandle = attr->hdls;

		CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_GET_RGN_HDLS\n");

		if (vo_cb_get_rgn_hdls(VoLayer, VoChn, pstHandle)) {
			CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_GET_RGN_HDLS failed.\n");
		}

		rc = 0;
		break;
	}

	case VO_CB_SET_RGN_HDLS:
	{
		struct _rgn_hdls_cb_param *attr = (struct _rgn_hdls_cb_param *)arg;
		VO_LAYER VoLayer = attr->stChn.s32DevId;
		VO_CHN VoChn = attr->stChn.s32ChnId;
		RGN_HANDLE *pstHandle = attr->hdls;

		CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_SET_RGN_HDLS\n");

		if (vo_cb_set_rgn_hdls(VoLayer, VoChn, pstHandle)) {
			CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_SET_RGN_HDLS failed.\n");
		}

		rc = 0;
		break;
	}

	case VO_CB_SET_RGN_CFG:
	{
		struct sclr_disp_timing *timing = sclr_disp_get_timing();
		struct sclr_size size;
		struct _rgn_cfg_cb_param *attr = (struct _rgn_cfg_cb_param *)arg;
		struct cvi_rgn_cfg *pstRgnCfg = &attr->rgn_cfg;
		VO_LAYER VoLayer = attr->stChn.s32DevId;
		VO_CHN VoChn = attr->stChn.s32ChnId;

		CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_SET_RGN_CFG\n");

		if (vo_cb_set_rgn_cfg(VoLayer, VoChn, pstRgnCfg)) {
			CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_SET_RGN_CFG is failed.\n");
		}

		size.w = timing->hfde_end - timing->hfde_start + 1;
		size.h = timing->vfde_end - timing->vfde_start + 1;
		vo_set_rgn_cfg(SCL_GOP_DISP, pstRgnCfg, &size);
		rc = CVI_SUCCESS;
		break;
	}

	case VO_CB_GET_CHN_SIZE:
	{
		struct _rgn_chn_size_cb_param *param = (struct _rgn_chn_size_cb_param *)arg;
		VO_LAYER VoLayer = param->stChn.s32DevId;
		VO_CHN VoChn = param->stChn.s32ChnId;

		CVI_TRACE_VO(CVI_VO_INFO, "VO_CB_GET_CHN_SIZE\n");

		rc = vo_cb_get_chn_size(VoLayer, VoChn, &param->rect);
		if (rc != CVI_SUCCESS) {
			CVI_TRACE_VO(CVI_VO_ERR, "VO_CB_GET_CHN_SIZE failed\n");
		}
		break;
	}

	case VO_CB_GDC_OP_DONE:
	{
		struct ldc_op_done_cfg *cfg =
			(struct ldc_op_done_cfg *)arg;
		_vo_gdc_callback(cfg->pParam, cfg->blk);

		rc = 0;
		break;
	}

	case VO_CB_QBUF_VO_GET_CHN_ROTATION:
	{
		struct vo_get_chnrotation_cfg *cfg =
			(struct vo_get_chnrotation_cfg *)arg;

		vo_get_chnrotation(cfg->VoLayer, cfg->VoChn, (ROTATION_E *)&cfg->enRotation);

		rc = 0;
		break;
	}

	case VO_CB_QBUF_TRIGGER:
	{
		u8 vpss_dev;

		vpss_dev = *((u8 *)arg);
		CVI_TRACE_VO(CVI_VO_ERR, "VO_CB_QBUF_TRIGGER\n");

		vo_post_job(vpss_dev);
		rc = 0;
		break;
	}

	default:
		break;
	}

	return rc;
}
/*******************************************************
 *  Irq handlers
 ******************************************************/

void vo_irq_handler(struct cvi_vo_dev *vdev, union sclr_intr intr_status)
{
	if (atomic_read(&vdev->disp_streamon) == 0)
		return;

	if (intr_status.b.disp_frame_end) {
		union sclr_disp_dbg_status status = sclr_disp_get_dbg_status(true);

		++vdev->frame_number;

		if (status.b.bw_fail)
			CVI_TRACE_VO(CVI_VO_ERR, " disp bw failed at frame#%d\n", vdev->frame_number);
		if (status.b.osd_bw_fail)
			CVI_TRACE_VO(CVI_VO_ERR, " osd bw failed at frame#%d\n", vdev->frame_number);

		// CVI_TRACE_VO(CVI_VO_INFO, " vo_irq_handler entry 1 ,vdev->num_rdy[%d]\n",vdev->num_rdy);

		// i80 won't need to keep one frame for read, but others need.
		if ((atomic_read(&vdev->num_rdy) > 1) || (vdev->disp_interface == CVI_VIP_DISP_INTF_I80)) {
			vo_buf_remove((struct cvi_vo_dev *)vdev);
			// muted until frame available.
			if (gVoCtx->is_layer_enable[0]) {
				sclr_disp_enable_window_bgcolor(false);
			}

			vo_wake_up_th((struct cvi_vo_dev *)vdev);

			_vo_hw_enque(vdev);
		}
	}
}

static int _vo_init_param(struct cvi_vo_dev *vdev)
{
	int ret = 0;
	struct mod_ctx_s  ctx_s;

	gVoCtx = (struct cvi_vo_ctx *)vdev->shared_mem;

	ctx_s.modID = CVI_ID_VO;
	ctx_s.ctx_num = 0;
	ctx_s.ctx_info = (void *)gVoCtx;

	ret = base_set_mod_ctx(&ctx_s);
	if (ret) {
		CVI_TRACE_VO(CVI_VO_ERR, "Failed to set mod ctx\n");
		goto err;
	}

	memset(gVoCtx, 0, sizeof(*gVoCtx));
err:
	return ret;

}
/*******************************************************
 *  Common interface for core
 ******************************************************/
int vo_create_instance(struct cvi_vo_dev *pdev)
{
	int ret = 0;
	struct cvi_vo_dev *vdev = pdev;
	u16 rgb[3] = {0, 0, 0};

	job_init = 0;//tmp test

#if 0//set base addr by vpss
	sclr_set_base_addr(vdev->reg_base[0]);
	vip_set_base_addr(vdev->reg_base[1]);
	dphy_set_base_addr(vdev->reg_base[2]);
#endif

	if (sclr_disp_mux_get() == SCLR_VO_SEL_I80) {
		smooth = sclr_disp_check_i80_enable();
	} else {
		smooth = sclr_disp_check_tgen_enable();
	}

	ret = _vo_create_proc(vdev);
	if (ret) {
		CVI_TRACE_VO(CVI_VO_ERR, "Failed to create proc\n");
		goto err;
	}
	ret = _vo_init_param(vdev);
	gVoCtx = (struct cvi_vo_ctx *)vdev->shared_mem;

	gVdev = vdev;

	if (hide_vo) {
		sclr_disp_set_pattern(SCL_PAT_TYPE_FULL, SCL_PAT_COLOR_USR, rgb);
		sclr_disp_set_frame_bgcolor(0, 0, 0);
	}

err:
	return ret;
}

int vo_destroy_instance(struct cvi_vo_dev *pdev)
{
	int ret = 0;
	struct cvi_vo_dev *vdev = pdev;

	vo_destroy_thread(vdev, E_VO_TH_DISP);

	ret = _vo_destroy_proc(vdev);
	return ret;
}
