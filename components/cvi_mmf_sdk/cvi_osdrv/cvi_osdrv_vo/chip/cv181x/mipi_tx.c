/* SPDX-License-Identifier: GPL-2.0 */
#include <errno.h>
#include <unistd.h>

#include "cvi_reg.h"
#include "mipi_tx.h"
#include <vip_common.h>
#include "scaler.h"
#include "dsi_phy.h"

struct cvi_vip_mipi_tx_dev {
	struct combo_dev_cfg_s dev_cfg;
	int irq_vbat;
	void *vbat_addr[2];
};

/*
 * macro definition
 */

/*
 * global variables definition
 */
static u32 mipi_tx_log_lv = CVI_MIPI_TX_ERR;
struct cvi_vip_mipi_tx_dev *mtdev;
static int pxl_clk, lane_num, bits;

static int debug = 1;
/* debug: debug
 * bit[0]: dcs cmd mode. 0(hw)/1(sw)
 */

/*
 * function definition
 */
static int _mipi_tx_check_comb_dev_cfg(struct combo_dev_cfg_s *dev_cfg)
{
	if (dev_cfg->output_mode != OUTPUT_MODE_DSI_VIDEO) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "output_mode(%d) not supported!\n", dev_cfg->output_mode);
		return -EINVAL;
	}

	if (dev_cfg->video_mode != BURST_MODE) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "video_mode(%d) not supported!\n", dev_cfg->video_mode);
		return -EINVAL;
	}

	return 0;
}

static void _fill_disp_timing(struct sclr_disp_timing *timing, struct sync_info_s *sync_info)
{
	timing->vtotal = sync_info->vid_vsa_lines + sync_info->vid_vbp_lines
			+ sync_info->vid_active_lines + sync_info->vid_vfp_lines - 1;
	timing->htotal = sync_info->vid_hsa_pixels + sync_info->vid_hbp_pixels
			+ sync_info->vid_hline_pixels + sync_info->vid_hfp_pixels - 1;
	timing->vsync_start = 1;
	timing->vsync_end = timing->vsync_start + sync_info->vid_vsa_lines - 1;
	timing->vfde_start = timing->vmde_start =
		timing->vsync_start + sync_info->vid_vsa_lines + sync_info->vid_vbp_lines;
	timing->vfde_end = timing->vmde_end =
		timing->vfde_start + sync_info->vid_active_lines - 1;
	timing->hsync_start = 1;
	timing->hsync_end = timing->hsync_start + sync_info->vid_hsa_pixels - 1;
	timing->hfde_start = timing->hmde_start =
		timing->hsync_start + sync_info->vid_hsa_pixels + sync_info->vid_hbp_pixels;
	timing->hfde_end = timing->hmde_end =
		timing->hfde_start + sync_info->vid_hline_pixels - 1;
	timing->vsync_pol = sync_info->vid_vsa_pos_polarity;
	timing->hsync_pol = sync_info->vid_hsa_pos_polarity;

}

static void _get_sync_info(struct sclr_disp_timing timing, struct sync_info_s *sync_info)
{
	sync_info->vid_hsa_pixels = timing.hsync_end - timing.hsync_start + 1;
	sync_info->vid_hbp_pixels = timing.hfde_start - timing.hsync_start - sync_info->vid_hsa_pixels;
	sync_info->vid_hline_pixels = timing.hfde_end - timing.hfde_start + 1;
	sync_info->vid_hfp_pixels = timing.htotal - sync_info->vid_hsa_pixels
				    - sync_info->vid_hbp_pixels - sync_info->vid_hline_pixels + 1;
	sync_info->vid_vsa_lines = timing.vsync_end - timing.vsync_start + 1;
	sync_info->vid_vbp_lines = timing.vfde_start - timing.vsync_start - sync_info->vid_vsa_lines;
	sync_info->vid_active_lines = timing.vfde_end - timing.vfde_start + 1;
	sync_info->vid_vfp_lines = timing.vtotal - sync_info->vid_vsa_lines
				   - sync_info->vid_vbp_lines - sync_info->vid_active_lines + 1;
	sync_info->vid_vsa_pos_polarity = timing.vsync_pol;
	sync_info->vid_hsa_pos_polarity = timing.hsync_pol;

}

static int _mipi_tx_set_combo_dev_cfg(struct cvi_vip_mipi_tx_dev *tdev, struct combo_dev_cfg_s *dev_cfg)
{
	int ret, i;
	bool data_en[LANE_MAX_NUM] = {false, false, false, false, false};
	struct sclr_disp_timing timing;
	enum sclr_dsi_fmt dsi_fmt;
	bool preamble_on = false;

	ret = _mipi_tx_check_comb_dev_cfg(dev_cfg);
	if (ret < 0) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx check combo_dev config failed!\n");
		return -EINVAL;
	}

	lane_num = 0;
	bits = 0;
	dphy_dsi_disable_lanes();

	memcpy(&tdev->dev_cfg, dev_cfg, sizeof(tdev->dev_cfg));
	for (i = 0; i < LANE_MAX_NUM; i++) {
		if ((dev_cfg->lane_id[i] < 0) || (dev_cfg->lane_id[i] >= MIPI_TX_LANE_MAX)) {
			dphy_dsi_set_lane(i, DSI_LANE_MAX, false, true);
			continue;
		}
		dphy_dsi_set_lane(i, dev_cfg->lane_id[i], dev_cfg->lane_pn_swap[i], true);
		if (dev_cfg->lane_id[i] != MIPI_TX_LANE_CLK) {
			++lane_num;
			data_en[dev_cfg->lane_id[i] - 1] = true;
		}
	}
	if (lane_num == 0) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "no active mipi-dsi lane\n");
		return -EINVAL;
	}

	_fill_disp_timing(&timing, &dev_cfg->sync_info);
	switch (dev_cfg->output_format) {
	case OUT_FORMAT_RGB_16_BIT:
		bits = 16;
		dsi_fmt = SCLR_DSI_FMT_RGB565;
	break;

	case OUT_FORMAT_RGB_18_BIT:
		bits = 18;
		dsi_fmt = SCLR_DSI_FMT_RGB666;
	break;

	case OUT_FORMAT_RGB_24_BIT:
		bits = 24;
		dsi_fmt = SCLR_DSI_FMT_RGB888;
	break;

	case OUT_FORMAT_RGB_30_BIT:
		bits = 30;
		dsi_fmt = SCLR_DSI_FMT_RGB101010;
	break;

	default:
	return -EINVAL;
	}

	preamble_on = (dev_cfg->pixel_clk * bits / lane_num) > 1500000;

	dphy_dsi_lane_en(true, data_en, preamble_on);
	dphy_dsi_set_pll(dev_cfg->pixel_clk, lane_num, bits);
	pxl_clk = dev_cfg->pixel_clk;
	sclr_disp_set_intf(SCLR_VO_INTF_MIPI);
	sclr_dsi_config(lane_num, dsi_fmt, dev_cfg->sync_info.vid_hline_pixels);
	sclr_disp_set_timing(&timing);
	sclr_disp_tgen_enable(true);

	CVI_TRACE_MIPI_TX(CVI_MIPI_TX_INFO, "lane_num(%d) preamble_on(%d) dsi_fmt(%d) bits(%d)\n",
		lane_num, preamble_on, dsi_fmt, bits);

	return ret;
}

int mipi_tx_get_combo_dev_cfg(struct combo_dev_cfg_s *dev_cfg)
{
	int ret = 0;
	char data_lan_num = 0;
	struct sclr_disp_timing timing;

	if (dev_cfg == NULL) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "ptr dev_cfg is NULL!\n");
		return -EINVAL;
	}

	data_lan_num = dphy_dsi_get_lane((enum lane_id *)dev_cfg->lane_id);
	dev_cfg->video_mode = BURST_MODE;
	dev_cfg->output_mode = OUTPUT_MODE_DSI_VIDEO;
	dev_cfg->output_format = OUT_FORMAT_RGB_24_BIT;

	sclr_disp_get_hw_timing(&timing);
	_get_sync_info(timing, &dev_cfg->sync_info);
	dphy_dsi_get_pixclk(&dev_cfg->pixel_clk, data_lan_num, 24);

	return ret;
}

static int _mipi_tx_set_cmd(struct cmd_info_s *cmd_info)
{
	int i;
	char str[160] = {0};

	if (cmd_info->cmd_size > CMD_MAX_NUM) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "cmd_size(%d) can't exceed %d!\n", cmd_info->cmd_size, CMD_MAX_NUM);
		return -EINVAL;
	} else if ((cmd_info->cmd_size != 0) && (cmd_info->cmd == NULL)) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "cmd is NULL, but cmd_size(%d) isn't zero!\n", cmd_info->cmd_size);
		return -EINVAL;
	}

	snprintf(str, 160, "%s: ", __func__);
	for (i = 0; i < cmd_info->cmd_size && i < 16; ++i)
		snprintf(str + strlen(str), 160 - strlen(str), "%#x ", cmd_info->cmd[i]);
	CVI_TRACE_MIPI_TX(CVI_MIPI_TX_INFO, "%s\n", str);


	return sclr_dsi_dcs_write_buffer(cmd_info->data_type, cmd_info->cmd, cmd_info->cmd_size, debug & 0x01);
}

static int _mipi_tx_get_cmd(struct get_cmd_info_s *get_cmd_info)
{
	int ret = 0;

	if (get_cmd_info->get_data_size > RX_MAX_NUM) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "get_data_size(%d) can't exceed %d!\n",
				get_cmd_info->get_data_size, RX_MAX_NUM);
		return -EINVAL;
	} else if ((get_cmd_info->get_data_size != 0) && (get_cmd_info->get_data == NULL)) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "cmd is NULL, but cmd_size(%d) isn't zero!\n",
			get_cmd_info->get_data_size);
		return -EINVAL;
	}

	if (pxl_clk) {
		dphy_dsi_set_pll(pxl_clk * 2, lane_num, bits);
		usleep(1000);
	}

	ret = sclr_dsi_dcs_read_buffer(get_cmd_info->data_type, get_cmd_info->data_param
		, get_cmd_info->get_data, get_cmd_info->get_data_size);

	if (pxl_clk) {
		dphy_dsi_set_pll(pxl_clk, lane_num, bits);
		usleep(1000);
	}
	return ret;
}

static void _mipi_tx_enable(void)
{
	sclr_dsi_set_mode(SCLR_DSI_MODE_HS);

	usleep(1000);
	//do not enable disp tgen before parameters of disp are set
	// sclr_disp_tgen_enable(false);
}

static void _mipi_tx_disable(void)
{
	int ret = 0;
	int count = 0;

	sclr_dsi_set_mode(SCLR_DSI_MODE_IDLE);
	do {
		usleep(1000);
		ret = sclr_dsi_chk_mode_done(SCLR_DSI_MODE_IDLE);
	} while ((ret != 0) && (count++ < 20));

}

int mipi_tx_ctrls(CVI_U8 chl, struct mipi_tx_ctrls *ext_ctrls)
{
	int rc = 0;
	struct cvi_vip_mipi_tx_dev *tdev = mtdev;
	unsigned int cmd = ext_ctrls->cmd;
	unsigned long arg = (unsigned long)ext_ctrls->ptr;

	switch (cmd) {
	case CVI_VIP_MIPI_TX_SET_DEV_CFG: {
		struct combo_dev_cfg_s stcombo_dev_cfg;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}
		memcpy(&stcombo_dev_cfg, (void *)arg, sizeof(stcombo_dev_cfg));


		rc = _mipi_tx_set_combo_dev_cfg(tdev, &stcombo_dev_cfg);
		if (rc < 0)
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx set combo_dev config failed!\n");
		else
			tdev->dev_cfg = stcombo_dev_cfg;
	}
	break;

	case CVI_VIP_MIPI_TX_GET_DEV_CFG: {
		struct combo_dev_cfg_s stcombo_dev_cfg;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}

		memset(&stcombo_dev_cfg, 0, sizeof(stcombo_dev_cfg));
		rc = mipi_tx_get_combo_dev_cfg(&stcombo_dev_cfg);
		if (rc < 0)
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx get combo_dev config failed!\n");

		memcpy((void *)arg, &stcombo_dev_cfg, sizeof(stcombo_dev_cfg));

	}
	break;

	case CVI_VIP_MIPI_TX_SET_CMD: {
		struct cmd_info_s cmd_info;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}
		memcpy(&cmd_info, (void *)arg, sizeof(cmd_info));
		if (cmd_info.cmd_size == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "cmd_size zero.\n");
			rc = -EINVAL;
			break;
		}

		// if cmd is NULL, use cmd_size as cmd.
		if (cmd_info.cmd == NULL) {
			cmd_info.cmd = malloc(2);
			if (cmd_info.cmd == NULL) {
				CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "malloc failed.\n");
				rc = -ENOMEM;
				break;
			}
			cmd_info.cmd[0] = cmd_info.cmd_size & 0xff;
			cmd_info.cmd[1] = (cmd_info.cmd_size >> 8) & 0xff;
			cmd_info.cmd_size = (cmd_info.data_type == 0x05) ? 1 : 2;
		} else {
			unsigned char *tmp_cmd = malloc(cmd_info.cmd_size);

			if (tmp_cmd == NULL) {
				CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "malloc failed.\n");
				rc = -ENOMEM;
				break;
			}

			memcpy(tmp_cmd, (void *)cmd_info.cmd, cmd_info.cmd_size);
			cmd_info.cmd = tmp_cmd;
		}

		rc = _mipi_tx_set_cmd(&cmd_info);
		if (rc < 0)
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx set cmd failed!\n");
		free(cmd_info.cmd);
	}
	break;

	case CVI_VIP_MIPI_TX_GET_CMD: {
		struct get_cmd_info_s get_cmd_info;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}
		memcpy(&get_cmd_info, (void *)arg, sizeof(get_cmd_info));
		rc = _mipi_tx_get_cmd(&get_cmd_info);
		if (rc < 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx get cmd failed!\n");
			break;
		}
		memcpy((void *)arg, &get_cmd_info, sizeof(get_cmd_info));
	}
	break;

	case CVI_VIP_MIPI_TX_ENABLE:
		_mipi_tx_enable();
	break;

	case CVI_VIP_MIPI_TX_DISABLE:
		_mipi_tx_disable();
	break;

	case CVI_VIP_MIPI_TX_SET_HS_SETTLE: {
		struct hs_settle_s settle_cfg;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}
		memcpy(&settle_cfg, (void *)arg, sizeof(settle_cfg));
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_INFO, "Set hs settle: prepare(%d) zero(%d) trail(%d)\n",
				     settle_cfg.prepare, settle_cfg.zero, settle_cfg.trail);
		dphy_set_hs_settle(settle_cfg.prepare, settle_cfg.zero, settle_cfg.trail);
	}
	break;

	case CVI_VIP_MIPI_TX_GET_HS_SETTLE: {
		struct hs_settle_s settle_cfg;

		if (arg == 0) {
			CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "NULL pointer.\n");
			rc = -EINVAL;
			break;
		}
		dphy_get_hs_settle(&settle_cfg.prepare, &settle_cfg.zero, &settle_cfg.trail);
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_INFO, "Get hs settle: prepare(%d) zero(%d) trail(%d)\n",
				     settle_cfg.prepare, settle_cfg.zero, settle_cfg.trail);
		memcpy((void *)arg, &settle_cfg, sizeof(settle_cfg));
	}
	break;

	default: {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "invalid mipi_tx ioctl cmd\n");
		rc = -EINVAL;
	}
	break;
	}

	return rc;
}

static int _init_resources(struct cvi_vip_mipi_tx_dev *mtdev)
{
	int rc = 0;

	(void)mtdev;
	return rc;
}

int mipi_tx_probe(void)
{
	int rc = 0;

	if (mtdev)
		return rc;

	/* allocate main structure */
	mtdev = malloc(sizeof(struct cvi_vip_mipi_tx_dev));
	if (!mtdev) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "%s malloc size(%zd) fail\n",
				__func__, sizeof(struct cvi_vip_mipi_tx_dev));
		return -ENOMEM;
	}
	memset(mtdev, 0, sizeof(struct cvi_vip_mipi_tx_dev));

	_init_resources(mtdev);

	if (mtdev->irq_vbat > 0) {
		mtdev->vbat_addr[0] = (void *)0x03000220;
		mtdev->vbat_addr[1] = (void *)0x03005144;
	}

#if 0 //todo
	rc = mipi_tx_proc_init();
	if (rc) {
		CVI_TRACE_MIPI_TX(CVI_MIPI_TX_ERR, "mipi_tx proc init fail\n");
	}
#endif

	return rc;
}

int mipi_tx_remove(void)
{
	if (mtdev) {
		free(mtdev);
		mtdev = NULL;
	}

	// mipi_tx_proc_remove();

	return 0;
}
