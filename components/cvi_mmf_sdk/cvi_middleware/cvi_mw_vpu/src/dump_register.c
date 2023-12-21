#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include "cvi_isp.h"
#include "cvi_mw_base.h"
#include "cvi_vi.h"
#include "cvi_sys.h"
#include "vi_ioctl.h"

#define VDEV_CLOSED_CHK(_dev_type, _dev_id)					\
	struct vdev *d;								\
	d = get_dev_info(_dev_type, _dev_id);					\
	if (IS_VDEV_CLOSED(d->state)) {						\
		CVI_TRACE_VI(CVI_DBG_ERR, "vi dev(%d) state(%d) incorrect.",	\
					_dev_id, d->state);			\
		return CVI_ERR_VI_SYS_NOTREADY;					\
	}

#define CHECK_VI_NULL_PTR(ptr)							\
	do {									\
		if (ptr == NULL) {						\
			CVI_TRACE_VI(CVI_DBG_ERR, " Invalid null pointer\n");	\
			return CVI_ERR_VI_INVALID_NULL_PTR;			\
		}								\
	} while (0)

#define GET_BASE_VADDR(ip_info_id)				\
	do {							\
		paddr = ip_info_list[ip_info_id].str_addr;	\
		size = ip_info_list[ip_info_id].size;		\
		vaddr = (void *)(CVI_U64)paddr;			\
	} while (0)

#define CLEAR_ACCESS_CNT(addr_ofs)				\
	do {							\
		val = vaddr + addr_ofs;				\
		*val = 0x1;					\
	} while (0)

#define SET_BITS(addr_ofs, val_ofs)				\
	do {							\
		val = vaddr + addr_ofs;				\
		data = *val | (0x1 << val_ofs);			\
		*val = data;					\
	} while (0)

#define CLEAR_BITS(addr_ofs, val_ofs)				\
	do {							\
		val = vaddr + addr_ofs;				\
		data = *val & (~(0x1 << val_ofs));		\
		*val = data;					\
	} while (0)

#define SET_REGISTER_COMMON(addr_ofs, val_ofs, on_off)		\
	do {							\
		if (!on_off) {/* off */				\
			CLEAR_BITS(addr_ofs, val_ofs);		\
		} else {/* on */				\
			SET_BITS(addr_ofs, val_ofs);		\
		}						\
	} while (0)

#define GET_REGISTER_COMMON(addr_ofs, val_ofs, ret)		\
	do {							\
		val = vaddr + addr_ofs;				\
		data = *val;					\
		ret = (data >> val_ofs) & 0x1;			\
	} while (0)

#define WAIT_IP_DISABLE(addr_ofs, val_ofs)			\
	do {							\
		do {						\
			usleep(1);				\
			val = vaddr + addr_ofs;			\
			data = *val;				\
		} while (((data >> val_ofs) & 0x1) != 0);	\
	} while (0)

#define SET_SW_MODE_ENABLE(addr_ofs, val_ofs, on_off)		\
	SET_REGISTER_COMMON(addr_ofs, val_ofs, on_off)

#define SET_SW_MODE_MEM_SEL(addr_ofs, val_ofs, mem_id)		\
	SET_REGISTER_COMMON(addr_ofs, val_ofs, mem_id)

#define FPRINTF_VAL()									\
	do {										\
		val = vaddr + offset;							\
		pos += sprintf(buf+pos, "\t\"%s\": {\n", name);				\
		pos += sprintf(buf+pos, "\t\t\"length\": %u,\n", length);		\
		pos += sprintf(buf+pos, "\t\t\"lut\": [\n");				\
		pos += sprintf(buf+pos, "\t\t\t");					\
		for (CVI_U32 i = 0 ; i < length; i++) {					\
			if (i == length - 1) {						\
				pos += sprintf(buf+pos, "%u\n", *val);			\
			} else if (i % 16 == 15) {					\
				pos += sprintf(buf+pos, "%u,\n\t\t\t", *val);		\
			} else {							\
				pos += sprintf(buf+pos, "%u,\t", *val);			\
			}								\
		}									\
		pos += sprintf(buf+pos, "\t\t]\n\t},\n");				\
	} while (0)

#define FPRINTF_VAL2()									\
	do {										\
		val = vaddr + offset;							\
		pos += sprintf(buf+pos, "\t\"%s\": {\n", name);				\
		pos += sprintf(buf+pos, "\t\t\"length\": %u,\n", length);		\
		pos += sprintf(buf+pos, "\t\t\"lut\": [\n");				\
		pos += sprintf(buf+pos, "\t\t\t");					\
		for (CVI_U32 i = 0 ; i < length; i++) {					\
			if (i == length - 1) {						\
				pos += sprintf(buf+pos, "%u\n", *(val++));		\
			} else if (i % 16 == 15) {					\
				pos += sprintf(buf+pos, "%u,\n\t\t\t", *(val++));	\
			} else {							\
				pos += sprintf(buf+pos, "%u,\t", *(val++));		\
			}								\
		}									\
		pos += sprintf(buf+pos, "\t\t]\n\t},\n");				\
	} while (0)

#define FPRINTF_TBL(data_tbl)								\
	do {										\
		pos += sprintf(buf+pos, "\t\"%s\": {\n", name);				\
		pos += sprintf(buf+pos, "\t\t\"length\": %u,\n", length);		\
		pos += sprintf(buf+pos, "\t\t\"lut\": [\n");				\
		pos += sprintf(buf+pos, "\t\t\t");					\
		for (CVI_U32 i = 0 ; i < length; i++) {					\
			if (i == length - 1) {						\
				pos += sprintf(buf+pos, "%u\n", data_tbl[i]);		\
			} else if (i % 16 == 15) {					\
				pos += sprintf(buf+pos, "%u,\n\t\t\t", data_tbl[i]);	\
			} else {							\
				pos += sprintf(buf+pos, "%u,\t", data_tbl[i]);		\
			}								\
		}									\
		pos += sprintf(buf+pos, "\t\t]\n\t},\n");				\
	} while (0)

#define DUMP_LUT_BASE(data_tbl, data_mask, r_addr, r_trig, r_data)	\
	do {								\
		for (CVI_U32 i = 0 ; i < length; i++) {			\
			val = vaddr + r_addr;				\
			*val = i;					\
									\
			val = vaddr + r_trig;				\
			data = (*val | (0x1 << 31));			\
			*val = data;					\
									\
			val = vaddr + r_data;				\
			data = *val;					\
			data_tbl[i] = (data & data_mask);		\
		}							\
	} while (0)

#define DUMP_LUT_COMMON(data_tbl, data_mask, sw_mode, r_addr, r_trig, r_data)	\
	do {									\
		val = vaddr + sw_mode;						\
		data = 0x1;							\
		*val = data;							\
										\
		DUMP_LUT_BASE(data_tbl, data_mask, r_addr, r_trig, r_data);	\
										\
		val = vaddr + sw_mode;						\
		data = 0x0;							\
		*val = data;							\
	} while (0)

struct reg_tbl {
	int addr_ofs;
	int val_ofs;
	int data;
	int mask;
};

struct gamma_tbl {
	enum IP_INFO_GRP ip_info_id;
	char name[16];
	int length;
	struct reg_tbl enable;
	struct reg_tbl shdw_sel;
	struct reg_tbl force_clk_enable;
	struct reg_tbl prog_en;
	struct reg_tbl raddr;
	struct reg_tbl rdata_r;
	struct reg_tbl rdata_gb;
};

static void _dump_gamma_table(void *fp, struct ip_info *ip_info_list, struct gamma_tbl *tbl, CVI_U32 *offset)
{
	CVI_U32 paddr;
	CVI_U32 size;
	CVI_VOID *vaddr;
	CVI_U32 *val;

	CVI_U32 length = tbl->length;
	CVI_U32 data;
	CVI_CHAR name[32];
	CVI_CHAR *buf = (CVI_CHAR *)fp;
	CVI_U32 pos = *offset;

	CVI_U32 *data_gamma_r = calloc(1, sizeof(CVI_U32) * length);
	CVI_U32 *data_gamma_g = calloc(1, sizeof(CVI_U32) * length);
	CVI_U32 *data_gamma_b = calloc(1, sizeof(CVI_U32) * length);

	GET_BASE_VADDR(tbl->ip_info_id);
	GET_REGISTER_COMMON(tbl->enable.addr_ofs, tbl->enable.val_ofs, tbl->enable.data);
	GET_REGISTER_COMMON(tbl->shdw_sel.addr_ofs, tbl->shdw_sel.val_ofs, tbl->shdw_sel.data);
	GET_REGISTER_COMMON(tbl->force_clk_enable.addr_ofs, tbl->force_clk_enable.val_ofs, tbl->force_clk_enable.data);

	SET_REGISTER_COMMON(tbl->enable.addr_ofs, tbl->enable.val_ofs, 0);
	// SET_REGISTER_COMMON(tbl->shdw_sel.addr_ofs, tbl->shdw_sel.val_ofs, 0);
	SET_REGISTER_COMMON(tbl->force_clk_enable.addr_ofs, tbl->force_clk_enable.val_ofs, 0);
	WAIT_IP_DISABLE(tbl->enable.addr_ofs, tbl->enable.val_ofs);
	SET_REGISTER_COMMON(tbl->prog_en.addr_ofs, tbl->prog_en.val_ofs, 1);

	CVI_TRACE_VI(CVI_DBG_DEBUG, "addr = %d, size = %u\n", paddr, size);
#ifdef __CV180X__
	if (tbl->ip_info_id == IP_INFO_ID_RGBGAMMA ||
		tbl->ip_info_id == IP_INFO_ID_YGAMMA ||
		tbl->ip_info_id == IP_INFO_ID_DCI) {
		CVI_U8 r_sel = 0;

		GET_REGISTER_COMMON(tbl->prog_en.addr_ofs, 4, r_sel);
		CVI_TRACE_VI(CVI_DBG_INFO, "mem[%d] work, mem[%d] IDLE\n", r_sel, r_sel ^ 0x1);
		SET_REGISTER_COMMON(tbl->raddr.addr_ofs, tbl->raddr.val_ofs, r_sel ^ 0x1);
	}
#endif

	for (CVI_U32 i = 0 ; i < length; i++) {
		val = vaddr + tbl->raddr.addr_ofs;
		data = (*val & (~tbl->raddr.mask)) | i;
		*val = data;

		val = vaddr + tbl->rdata_r.addr_ofs;
		data = (*val | (0x1 << tbl->rdata_r.val_ofs));
		*val = data;

		val = vaddr + tbl->rdata_r.addr_ofs;
		data = *val;
		data_gamma_r[i] = (data & tbl->rdata_r.mask);

		val = vaddr + tbl->rdata_gb.addr_ofs;
		data = *val;
		data_gamma_g[i] = (data & tbl->rdata_gb.mask);
		data_gamma_b[i] = ((data >> tbl->rdata_gb.val_ofs) & tbl->rdata_gb.mask);
	}

	SET_REGISTER_COMMON(tbl->prog_en.addr_ofs, tbl->prog_en.val_ofs, 0);
	SET_REGISTER_COMMON(tbl->force_clk_enable.addr_ofs, tbl->force_clk_enable.val_ofs, tbl->force_clk_enable.data);
	SET_REGISTER_COMMON(tbl->shdw_sel.addr_ofs, tbl->shdw_sel.val_ofs, tbl->shdw_sel.data);
	SET_REGISTER_COMMON(tbl->enable.addr_ofs, tbl->enable.val_ofs, tbl->enable.data);

	memset(name, 0, sizeof(name));
	strcat(strcat(name, tbl->name), "_r");
	FPRINTF_TBL(data_gamma_r);

	memset(name, 0, sizeof(name));
	strcat(strcat(name, tbl->name), "_g");
	FPRINTF_TBL(data_gamma_g);

	memset(name, 0, sizeof(name));
	strcat(strcat(name, tbl->name), "_b");
	FPRINTF_TBL(data_gamma_b);

	*offset = pos;

	free(data_gamma_r);
	free(data_gamma_g);
	free(data_gamma_b);
}

CVI_S32 dump_hw_register(VI_PIPE ViPipe, void *fp, VI_DUMP_REGISTER_TABLE_S *pstRegTbl)
{
	VDEV_CLOSED_CHK(VDEV_TYPE_ISP, ViPipe);
	CHECK_VI_NULL_PTR(fp);
	CHECK_VI_NULL_PTR(pstRegTbl);

	CVI_S32 s32Ret = CVI_SUCCESS;
#if defined(__CV181X__) || defined(__CV180X__)
	VI_DEV_ATTR_S stDevAttr;
	struct ip_info *ip_info_list;
	CVI_CHAR *buf = (CVI_CHAR *)fp;
	CVI_U32 pos = 0;
	int tuning_dis[4] = {0, 1, 1, 1};
	CVI_U32 devNum = 0;

	ip_info_list = calloc(1, sizeof(struct ip_info) * IP_INFO_ID_MAX);

	CVI_U32 paddr;
	CVI_U32 size;
	CVI_VOID *vaddr;
	CVI_U32 *val;

	CVI_U32 ip_info_id;
	CVI_U32 offset;
	CVI_U32 length;
	CVI_U32 data;
	CVI_CHAR name[32];

	CVI_TRACE_VI(CVI_DBG_INFO, "dump_addr %p\n", fp);

	s32Ret = vi_sdk_get_dev_attr(d->fd, (int)ViPipe, &stDevAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_sdk_get_dev_attr ioctl\n");
		free(ip_info_list);
		return s32Ret;
	}

	s32Ret = vi_get_ip_dump_list(d->fd, ip_info_list);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_get_ip_dump_list ioctl\n");
		free(ip_info_list);
		return s32Ret;
	}

	s32Ret = CVI_VI_GetDevNum(&devNum);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_VI(CVI_DBG_ERR, "vi_get_dev num fail\n");
		free(ip_info_list);
		return s32Ret;
	}

#ifdef __CV181X__
	/* stop tuning update */
	if (ViPipe == 0) {
		tuning_dis[0] = 1;
	} else if (ViPipe == 1) {
		tuning_dis[0] = 2;
	}
	vi_set_tuning_dis(d->fd, tuning_dis);
#else
	/* stop tuning update */
	if (ViPipe == 0) {
		vi_set_tuning_dis(d->fd, tuning_dis);
	}
#endif

	/* In the worst case, have to wait two frames to stop tuning update. */
	for (CVI_U32 pipe = 0; pipe < devNum; pipe++) {
		if (ViPipe == pipe) continue;

		for (CVI_U32 i = 0; i < 2; i++) {
			s32Ret = CVI_ISP_GetVDTimeOut(pipe, ISP_VD_FE_START, 500);
			if (s32Ret != CVI_SUCCESS) {
				CVI_TRACE_VI(CVI_DBG_ERR, "CVI_ISP_GetVDTimeOut fail\n");
				goto err_handle;
			}
		}
	}

	for (CVI_U32 i = 0; i < 3; i++) {
		s32Ret = CVI_ISP_GetVDTimeOut(ViPipe, ISP_VD_FE_START, 500);
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_VI(CVI_DBG_ERR, "CVI_ISP_GetVDTimeOut fail\n");
			goto err_handle;
		}
	}
	/* start of file */
	pos += sprintf(buf+pos, "{\n");
	/* dump isp moudle register */
	for (CVI_U32 i = 0; i < IP_INFO_ID_MAX; i++) {
		CVI_TRACE_VI(CVI_DBG_INFO, "%d\n", i);

		GET_BASE_VADDR(i);
		val = vaddr;

		pos += sprintf(buf+pos, "\t\"0x%08X\": {\n", paddr);
		for (CVI_U32 j = 0 ; j < (size / 0x4); j++) {
			pos += sprintf(buf+pos, "\t\t\"h%02x\": %u,\n", (j * 4), *(val++));
		}
		pos += sprintf(buf+pos, "\t\t\"size\": %u\n\t},\n", size);
	}

	/* dump look up table */
	// DPC
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "DPC\n");

		CVI_U8 enable = 0;
		// CVI_U8 shdw_sel = 0;
		CVI_U8 force_clk_enable = 0;
#ifdef __CV181X__
		CVI_U8 idx = (stDevAttr.stWDRAttr.enWDRMode == WDR_MODE_2To1_LINE) ? 2 : 1;
		CVI_U32 *data_dpc = NULL;
		CVI_CHAR name_list[2][16] = {"dpc_le_bp_tbl", "dpc_se_bp_tbl"};
		enum IP_INFO_GRP ip_info_id_list[2] = {IP_INFO_ID_DPC0, IP_INFO_ID_DPC1};
#else
		CVI_U8 idx = 1;
		CVI_U32 *data_dpc = NULL;
		CVI_CHAR name_list[1][16] = {"dpc_le_bp_tbl"};
		enum IP_INFO_GRP ip_info_id_list[1] = {IP_INFO_ID_DPC0};
#endif

		length = 2047;

		for (CVI_U8 i = 0; i < idx; i++) {
			GET_BASE_VADDR(ip_info_id_list[i]);
			data_dpc = calloc(1, sizeof(CVI_U32) * length);

			GET_REGISTER_COMMON(0x8, 0, enable);
			// GET_REGISTER_COMMON(0x4, 0, shdw_sel);
			GET_REGISTER_COMMON(0x8, 8, force_clk_enable);

			SET_REGISTER_COMMON(0x8, 0, 0); // reg_dpc_enable
			// SET_REGISTER_COMMON(0x4, 0, 0); // reg_shdw_read_sel
			SET_REGISTER_COMMON(0x8, 8, 0); // reg_force_clk_enable
			WAIT_IP_DISABLE(0x8, 0);
			SET_REGISTER_COMMON(0x44, 31, 1); // reg_dpc_mem_prog_mode

			DUMP_LUT_BASE(data_dpc, 0xFFFFFF, 0x48, 0x4C, 0x4C);

			SET_REGISTER_COMMON(0x44, 31, 0); // reg_dpc_mem_prog_mode
			SET_REGISTER_COMMON(0x8, 8, force_clk_enable); // reg_force_clk_enable
			// SET_REGISTER_COMMON(0x4, 0, shdw_sel); // reg_shdw_read_sel
			SET_REGISTER_COMMON(0x8, 0, enable); // reg_dpc_enable

			snprintf(name, sizeof(name), name_list[i]);
			FPRINTF_TBL(data_dpc);

			free(data_dpc);
		}
	}

	// MLSC
	// NO INDIRECT LUT
	// use the isp tun buffer
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "MLSC\n");
		MLSC_GAIN_LUT_S *MlscGainLut = &pstRegTbl->MlscGainLut;

		length = 1369;
		if (MlscGainLut->RGain && MlscGainLut->GGain && MlscGainLut->BGain) {
			snprintf(name, sizeof(name), "mlsc_gain_r");

			FPRINTF_TBL(MlscGainLut->RGain);
			snprintf(name, sizeof(name), "mlsc_gain_g");
			FPRINTF_TBL(MlscGainLut->GGain);

			snprintf(name, sizeof(name), "mlsc_gain_b");
			FPRINTF_TBL(MlscGainLut->BGain);
		} else {
			CVI_TRACE_VI(CVI_DBG_INFO, "MLSC is no data\n");
		}
	}

	// RGB_GAMMA
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "RGB_GAMMA\n");

		struct gamma_tbl rgb_gamma = {
			.ip_info_id = IP_INFO_ID_RGBGAMMA,
			.name = "rgb_gamma",
			.length = 256,
			.enable = {
				.addr_ofs = 0x0,
				.val_ofs = 0,
			},
			.shdw_sel = {
				.addr_ofs = 0x0,
				.val_ofs = 1,
			},
			.force_clk_enable = {
				.addr_ofs = 0x0,
				.val_ofs = 2,
			},
			.prog_en = {
				.addr_ofs = 0x4,
				.val_ofs = 8,
			},
			.raddr = {
				.addr_ofs = 0x14,
				.mask = 0xFF,
				.val_ofs = 12,
			},
			.rdata_r = {
				.addr_ofs = 0x18,
				.val_ofs = 31,
				.mask = 0xFFF,
			},
			.rdata_gb = {
				.addr_ofs = 0x1C,
				.val_ofs = 16,
				.mask = 0xFFF,
			},
		};

		_dump_gamma_table(fp, ip_info_list, &rgb_gamma, &pos);
	}

	//Y_GAMMA
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "Y_GAMMA\n");

		struct gamma_tbl y_gamma = {
			.ip_info_id = IP_INFO_ID_YGAMMA,
			.name = "y_gamma",
			.length = 256,
			.enable = {
				.addr_ofs = 0x0,
				.val_ofs = 0,
			},
			.shdw_sel = {
				.addr_ofs = 0x0,
				.val_ofs = 1,
			},
			.force_clk_enable = {
				.addr_ofs = 0x0,
				.val_ofs = 2,
			},
			.prog_en = {
				.addr_ofs = 0x4,
				.val_ofs = 8,
			},
			.raddr = {
				.addr_ofs = 0x14,
				.mask = 0xFF,
				.val_ofs = 12,
			},
			.rdata_r = {
				.addr_ofs = 0x18,
				.val_ofs = 31,
				.mask = 0xFFFF,
			},
			.rdata_gb = {
				.addr_ofs = 0x1C,
				.val_ofs = 16,
				.mask = 0xFFF,
			},
		};

		_dump_gamma_table(fp, ip_info_list, &y_gamma, &pos);
	}

	// CLUT
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "CLUT\n");
		ip_info_id = IP_INFO_ID_CLUT;

		CVI_U32 r_idx = 17;
		CVI_U32 g_idx = 17;
		CVI_U32 b_idx = 17;
		CVI_U32 rgb_idx = 0;
		CVI_U32 *data_clut_r = NULL;
		CVI_U32 *data_clut_g = NULL;
		CVI_U32 *data_clut_b = NULL;
		CVI_U8 enable = 0;
		CVI_U8 shdw_sel = 0;
		// CVI_U8 force_clk_enable = 0;

		length = r_idx * g_idx * b_idx;
		data_clut_r = calloc(1, sizeof(CVI_U32) * length);
		data_clut_g = calloc(1, sizeof(CVI_U32) * length);
		data_clut_b = calloc(1, sizeof(CVI_U32) * length);

		GET_BASE_VADDR(ip_info_id);
		GET_REGISTER_COMMON(0x0, 0, enable);
		GET_REGISTER_COMMON(0x0, 1, shdw_sel);
		// GET_REGISTER_COMMON(0x0, 2, force_clk_enable);

		SET_REGISTER_COMMON(0x0, 0, 0); // reg_clut_enable
		SET_REGISTER_COMMON(0x0, 1, 0); // reg_clut_shdw_sel
		// SET_REGISTER_COMMON(0x0, 2, 0); // reg_force_clk_enable
		WAIT_IP_DISABLE(0x0, 0);
		SET_REGISTER_COMMON(0x0, 3, 1); // reg_prog_en

		for (CVI_U32 i = 0 ; i < b_idx; i++) {
			for (CVI_U32 j = 0 ; j < g_idx; j++) {
				for (CVI_U32 k = 0 ; k < r_idx; k++) {
					rgb_idx = i * g_idx * r_idx + j * r_idx + k;

					val = vaddr + 0x04; // reg_sram_r_idx/reg_sram_g_idx/reg_sram_b_idx
					data = (i << 16) | (j << 8) | k;
					*val = data;

					val = vaddr + 0x0C; // reg_sram_rd
					data = (0x1 << 31);
					*val = data;

					val = vaddr + 0x0C; // reg_sram_rdata
					data = *val;

					data_clut_r[rgb_idx] = (data >> 20) & 0x3FF;
					data_clut_g[rgb_idx] = (data >> 10) & 0x3FF;
					data_clut_b[rgb_idx] = data & 0x3FF;
				}
			}
		}

		SET_REGISTER_COMMON(0x0, 3, 0); // reg_prog_en
		// SET_REGISTER_COMMON(0x0, 2, force_clk_enable); // reg_force_clk_enable
		SET_REGISTER_COMMON(0x0, 1, shdw_sel); // reg_clut_shdw_sel
		SET_REGISTER_COMMON(0x0, 0, enable); // reg_clut_enable

		snprintf(name, sizeof(name), "clut_r");
		FPRINTF_TBL(data_clut_r);

		snprintf(name, sizeof(name), "clut_g");
		FPRINTF_TBL(data_clut_g);

		snprintf(name, sizeof(name), "clut_b");
		FPRINTF_TBL(data_clut_b);

		free(data_clut_r);
		free(data_clut_g);
		free(data_clut_b);
	}

	// LTM
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "LTM\n");
		ip_info_id = IP_INFO_ID_HDRLTM;

		CVI_U8 enable = 0;
		CVI_U8 shdw_sel = 0;
		CVI_U8 force_clk_enable = 0;

		GET_BASE_VADDR(ip_info_id);
		GET_REGISTER_COMMON(0x0, 0, enable);
		GET_REGISTER_COMMON(0x0, 5, shdw_sel);
		GET_REGISTER_COMMON(0x0, 31, force_clk_enable);

		SET_REGISTER_COMMON(0x0, 0, 0); // reg_ltm_enable
		SET_REGISTER_COMMON(0x0, 5, 0); // reg_shdw_read_sel
		SET_REGISTER_COMMON(0x0, 31, 0); // reg_force_clk_enable
		WAIT_IP_DISABLE(0x0, 0);

		// dark tone
		length = 257;
		CVI_U32 *data_dtone_curve = calloc(1, sizeof(CVI_U32) * length);

		SET_REGISTER_COMMON(0x34, 17, 1); // reg_lut_prog_en_dark
		for (CVI_U32 i = 0 ; i < length; i++) {
			val = vaddr + 0x34;
			data = (*val & ~(0x3FF)) | i; // reg_lut_dbg_raddr[0,9]
			data = (data | (0x1 << 15)); // reg_lut_dbg_read_en_1t
			*val = data;

			val = vaddr + 0x4C; // reg_lut_dbg_rdata
			data = *val;
			data_dtone_curve[i] = data;
		}
		SET_REGISTER_COMMON(0x34, 17, 0); // reg_lut_prog_en_dark
		val = vaddr + 0x44; //reg_dark_lut_max
		data_dtone_curve[length - 1] = *val;
		snprintf(name, sizeof(name), "ltm_dtone_curve");
		FPRINTF_TBL(data_dtone_curve);
		free(data_dtone_curve);

		// bright tone
#ifdef __CV181X__
		length = 513;
#else
		length = 257;
#endif
		CVI_U32 *data_btone_curve = calloc(1, sizeof(CVI_U32) * length);

		SET_REGISTER_COMMON(0x34, 16, 1); // reg_lut_prog_en_bright
		for (CVI_U32 i = 0 ; i < length; i++) {
			val = vaddr + 0x34;
			data = (*val & ~(0x3FF)) | i; // reg_lut_dbg_raddr[0,9]
			data = (data | (0x1 << 15)); // reg_lut_dbg_read_en_1t
			*val = data;

			val = vaddr + 0x4C;
			data = *val;
			data_btone_curve[i] = data;
		}

		SET_REGISTER_COMMON(0x34, 16, 0); // reg_lut_prog_en_bright
		val = vaddr + 0x40; //reg_bright_lut_max
		data_btone_curve[length - 1] = *val;
		snprintf(name, sizeof(name), "ltm_btone_curve");
		FPRINTF_TBL(data_btone_curve);
		free(data_btone_curve);

		// global tone
#ifdef __CV181X__
		length = 769;
#else
		length = 257;
#endif
		CVI_U32 *data_global_curve = calloc(1, sizeof(CVI_U32) * length);

		SET_REGISTER_COMMON(0x34, 18, 1); // reg_lut_prog_en_global
		for (CVI_U32 i = 0 ; i < length; i++) {
			val = vaddr + 0x34;
			data = (*val & ~(0x3FF)) | i; // reg_lut_dbg_raddr[0,9]
			data = (data | (0x1 << 15)); // reg_lut_dbg_read_en_1t
			*val = data;

			val = vaddr + 0x4C;
			data = *val;
			data_global_curve[i] = data;
		}
		SET_REGISTER_COMMON(0x34, 18, 0); // reg_lut_prog_en_global
		val = vaddr + 0x48; //reg_global_lut_max
		data_global_curve[length - 1] = *val;
		snprintf(name, sizeof(name), "ltm_global_curve");
		FPRINTF_TBL(data_global_curve);
		free(data_global_curve);

		SET_REGISTER_COMMON(0x0, 31, force_clk_enable); // reg_force_clk_enable
		SET_REGISTER_COMMON(0x0, 5, shdw_sel); // reg_shdw_read_sel
		SET_REGISTER_COMMON(0x0, 0, enable); // reg_ltm_enable

	}

	// CA_CP
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "CA_CP\n");
		length = 256;
		ip_info_id = IP_INFO_ID_CA;

		CVI_U8 enable = 0;
		CVI_U8 shdw_sel = 0;
		CVI_U8 ca_cp_mode = 0;
		CVI_U32 *data_cacp_y = calloc(1, sizeof(CVI_U32) * length);
		CVI_U32 *data_cacp_u = calloc(1, sizeof(CVI_U32) * length);
		CVI_U32 *data_cacp_v = calloc(1, sizeof(CVI_U32) * length);

		GET_BASE_VADDR(ip_info_id);
		GET_REGISTER_COMMON(0x0, 0, enable);
		GET_REGISTER_COMMON(0x0, 4, shdw_sel);
		GET_REGISTER_COMMON(0x0, 1, ca_cp_mode);

		SET_REGISTER_COMMON(0x0, 0, 0); // reg_cacp_enable
		SET_REGISTER_COMMON(0x0, 4, 0); // reg_cacp_shdw_read_sel
		WAIT_IP_DISABLE(0x0, 0);
		SET_REGISTER_COMMON(0x0, 3, 1); // reg_cacp_mem_sw_mode

		for (CVI_U32 i = 0 ; i < length; i++) {
			val = vaddr + 0x0C;
			data = (*val & ~0xFF) | i;
			*val = data;

			val = vaddr + 0x0C;
			data = (*val | (0x1 << 31));
			*val = data;

			val = vaddr + 0x10;
			data = *val;

			if (ca_cp_mode) {
				data_cacp_y[i] = ((data >> 16) & 0xFF);
				data_cacp_u[i] = ((data >> 8) & 0xFF);
				data_cacp_v[i] = (data & 0xFF);
			} else {
				data_cacp_y[i] = (data & 0x7FF);
			}
		}

		SET_REGISTER_COMMON(0x0, 3, 0);  // reg_cacp_mem_sw_mode
		SET_REGISTER_COMMON(0x0, 4, shdw_sel); // reg_cacp_shdw_read_sel
		SET_REGISTER_COMMON(0x0, 0, enable); // reg_cacp_enable

		if (ca_cp_mode) {
			snprintf(name, sizeof(name), "ca_cp_y");
			FPRINTF_TBL(data_cacp_y);
			snprintf(name, sizeof(name), "ca_cp_u");
			FPRINTF_TBL(data_cacp_u);
			snprintf(name, sizeof(name), "ca_cp_v");
			FPRINTF_TBL(data_cacp_v);
		} else {
			snprintf(name, sizeof(name), "ca_y_ratio");
			FPRINTF_TBL(data_cacp_y);
		}

		free(data_cacp_y);
		free(data_cacp_u);
		free(data_cacp_v);
	}

	// YNR
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "YNR\n");
		ip_info_id = IP_INFO_ID_YNR;

		CVI_U8 shdw_sel = 0;

		GET_BASE_VADDR(ip_info_id);
		GET_REGISTER_COMMON(0x0, 0, shdw_sel);

		SET_REGISTER_COMMON(0x0, 0, 0); // reg_shadow_rd_sel
		CLEAR_ACCESS_CNT(0x8);

		length = 6;
		offset = 0x00C;
		snprintf(name, sizeof(name), "ynr_ns0_luma_th");
		FPRINTF_VAL2();

		length = 5;
		offset = 0x024;
		snprintf(name, sizeof(name), "ynr_ns0_slope");
		FPRINTF_VAL2();

		length = 6;
		offset = 0x038;
		snprintf(name, sizeof(name), "ynr_ns0_offset");
		FPRINTF_VAL2();

		length = 6;
		offset = 0x050;
		snprintf(name, sizeof(name), "ynr_ns1_luma_th");
		FPRINTF_VAL2();

		length = 5;
		offset = 0x068;
		snprintf(name, sizeof(name), "ynr_ns1_slope");
		FPRINTF_VAL2();

		length = 6;
		offset = 0x07C;
		snprintf(name, sizeof(name), "ynr_ns1_offset");
		FPRINTF_VAL2();

		length = 16;
		offset = 0x098;
		snprintf(name, sizeof(name), "ynr_motion_lut");
		FPRINTF_VAL2();

		length = 16;
		offset = 0x260;
		snprintf(name, sizeof(name), "ynr_res_mot_lut");
		FPRINTF_VAL2();

		length = 64;
		offset = 0x200;
		snprintf(name, sizeof(name), "ynr_weight_lut");
		FPRINTF_VAL();

		SET_REGISTER_COMMON(0x0, 0, shdw_sel); // reg_shadow_rd_sel

	}

	// YCURVE
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "YCURVE\n");
		length = 64;
		ip_info_id = IP_INFO_ID_YCURVE;

		CVI_U8 enable = 0;
		CVI_U8 shdw_sel = 0;
		CVI_U8 force_clk_enable = 0;
		CVI_U32 *data_ycurve = calloc(1, sizeof(CVI_U32) * length);

		GET_BASE_VADDR(ip_info_id);
		CVI_TRACE_VI(CVI_DBG_INFO, "YCURVE[%x]\n", paddr);
		GET_REGISTER_COMMON(0x0, 0, enable);
		GET_REGISTER_COMMON(0x0, 1, shdw_sel);
		GET_REGISTER_COMMON(0x0, 2, force_clk_enable);

		SET_REGISTER_COMMON(0x0, 1, 0); // reg_ycur_shdw_sel
		SET_REGISTER_COMMON(0x0, 2, 0); // reg_force_clk_enable
		SET_REGISTER_COMMON(0x0, 0, 0); // reg_ycur_enable

		WAIT_IP_DISABLE(0x0, 0);
		SET_REGISTER_COMMON(0x4, 8, 1); // reg_ycur_prog_en
#ifdef __CV180X__
		CVI_U8 r_sel = 0;

		GET_REGISTER_COMMON(0x4, 4, r_sel);
		CVI_TRACE_VI(CVI_DBG_INFO, "mem[%d] work, mem[%d] IDLE\n", r_sel, r_sel ^ 0x1);
		SET_REGISTER_COMMON(0x14, 12, r_sel ^ 0x1);
#endif
		for (CVI_U32 i = 0 ; i < length; i++) {
			val = vaddr + 0x14;
			data = (*val & ~(0x3F)) | i;
			*val = data;

			val = vaddr + 0x18;
			data = (*val | (0x1 << 31));
			*val = data;

			val = vaddr + 0x18;
			data = *val;
			data_ycurve[i] = (data & 0xFF);
		}

		SET_REGISTER_COMMON(0x4, 8, 0); // reg_ycur_prog_en
		SET_REGISTER_COMMON(0x0, 2, force_clk_enable); // reg_force_clk_enable
		SET_REGISTER_COMMON(0x0, 1, shdw_sel); // reg_ycur_shdw_sel
		SET_REGISTER_COMMON(0x0, 0, enable); // reg_ycur_enable

		snprintf(name, sizeof(name), "ycurve");
		FPRINTF_TBL(data_ycurve);

		free(data_ycurve);
	}

	// DCI_GAMMA
	{
		CVI_TRACE_VI(CVI_DBG_INFO, "DCI_GAMMA\n");

		struct gamma_tbl dci_gamma = {
			.ip_info_id = IP_INFO_ID_DCI,
			.name = "dci_gamma",
			.length = 256,
			.enable = {
				.addr_ofs = 0x0C,
				.val_ofs = 0,
			},
			.shdw_sel = {
				.addr_ofs = 0x14,
				.val_ofs = 4,
			},
			.force_clk_enable = {
				.addr_ofs = 0x0C,
				.val_ofs = 8,
			},
			.prog_en = {
				.addr_ofs = 0x204,
				.val_ofs = 8,
			},
			.raddr = {
				.addr_ofs = 0x214,
				.mask = 0xFF,
				.val_ofs = 12,
			},
			.rdata_r = {
				.addr_ofs = 0x218,
				.val_ofs = 31,
				.mask = 0xFFF,
			},
			.rdata_gb = {
				.addr_ofs = 0x21C,
				.val_ofs = 16,
				.mask = 0xFFF,
			},
		};

		_dump_gamma_table(fp, ip_info_list, &dci_gamma, &pos);
	}

	/* end of file */
	pos += sprintf(buf+pos, "\t\"end\": {}\n");
	pos += sprintf(buf+pos, "}");

	CVI_TRACE_VI(CVI_DBG_INFO, "pos=%d\n", pos);

err_handle:
	/* start tuning update */
	memset(&tuning_dis, 0, sizeof(tuning_dis));
	vi_set_tuning_dis(d->fd, tuning_dis);

	free(ip_info_list);
#endif
	return s32Ret;
}
