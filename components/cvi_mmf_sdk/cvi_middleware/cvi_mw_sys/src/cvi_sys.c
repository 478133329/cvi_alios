#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <queue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <aos/aos.h>
#include <unistd.h>

#include "cvi_mw_base.h"
#include "cvi_sys.h"
#include "cvi_errno.h"
#include "sys_ioctl.h"
#if CONFIG_DUALOS_NO_CROP
#include "cvi_efuse.h"
#endif //#if CONFIG_DUALOS_NO_CROP
#include "cvi_base.h"
#include "arch_helpers.h"


CVI_CHAR const *log_name[8] = {
	(CVI_CHAR *)"EMG", (CVI_CHAR *)"ALT", (CVI_CHAR *)"CRI", (CVI_CHAR *)"ERR",
	(CVI_CHAR *)"WRN", (CVI_CHAR *)"NOT", (CVI_CHAR *)"INF", (CVI_CHAR *)"DBG"
};


static MMF_VERSION_S mmf_version;
VI_VPSS_MODE_S stVIVPSSMode;
VPSS_MODE_S stVPSSMode;
CVI_BOOL bFbOnSc;
CVI_S32 log_levels[CVI_ID_BUTT] = { [0 ... CVI_ID_BUTT - 1] = CVI_DBG_WARN};

#define _GENERATE_STRING(STRING) (#STRING),
static const char *const MOD_STRING[] = FOREACH_MOD(_GENERATE_STRING);
#define CVI_GET_MOD_NAME(id) (id < CVI_ID_BUTT)? MOD_STRING[id] : "UNDEF"

CVI_S32 CVI_SYS_Init(void)
{
	CVI_S32 s32ret = CVI_SUCCESS;
	CVI_U32 sys_init = 0;

	sys_get_sys_init(&sys_init);
	if (sys_init == 0) {
		cvi_sys_open();

		//it always success;
		vi_dev_open();

		vpss_dev_open();

		for (CVI_U8 i = 0; i < VI_MAX_PIPE_NUM; ++i)
			stVIVPSSMode.aenMode[i] = VI_OFFLINE_VPSS_OFFLINE;
		CVI_SYS_SetVIVPSSMode(&stVIVPSSMode);
		stVPSSMode.enMode = VPSS_MODE_SINGLE;
		for (CVI_U8 i = 0; i < VPSS_IP_NUM; ++i)
			stVPSSMode.aenInput[i] = VPSS_INPUT_MEM;
		memset(&mmf_version, 0, sizeof(mmf_version));
		CVI_SYS_GetVersion(&mmf_version);
		CVI_SYS_SetVPSSModeEx(&stVPSSMode);
		vo_dev_open();

		rgn_dev_open();
	}

	sys_set_sys_init();
	sys_set_fbonsc(bFbOnSc);
	CVI_TRACE_SYS(CVI_DBG_INFO, "-\n");

	return s32ret;
}

CVI_S32 CVI_SYS_Exit(void)
{
	CVI_S32 s32ret = CVI_SUCCESS;

	CVI_TRACE_SYS(CVI_DBG_INFO, "+\n");
	cvi_sys_close();

	rgn_dev_close();
	vo_dev_close();
	vpss_dev_close();
	vi_dev_close();
	CVI_TRACE_SYS(CVI_DBG_INFO, "-\n");

	return s32ret;
}

CVI_S32 _CVI_SYS_BindIOCtl(const MMF_CHN_S *pstSrcChn, const MMF_CHN_S *pstDestChn, CVI_U8 is_bind)
{
	CVI_S32 ret = 0;
	struct sys_bind_cfg bind_cfg;

	memset(&bind_cfg, 0, sizeof(struct sys_bind_cfg));
	bind_cfg.is_bind = is_bind;
	bind_cfg.mmf_chn_src = *pstSrcChn;
	bind_cfg.mmf_chn_dst = *pstDestChn;

	ret = cvi_sys_ioctl(SYS_SET_BINDCFG, &bind_cfg);
	if (ret)
		CVI_TRACE_SYS(CVI_DBG_ERR, "_CVI_SYS_BindIOCtl()failed\n");

	return ret;
}

CVI_S32 CVI_SYS_Bind(const MMF_CHN_S *pstSrcChn, const MMF_CHN_S *pstDestChn)
{
	return _CVI_SYS_BindIOCtl(pstSrcChn, pstDestChn, 1);
}

CVI_S32 CVI_SYS_UnBind(const MMF_CHN_S *pstSrcChn, const MMF_CHN_S *pstDestChn)
{
	return _CVI_SYS_BindIOCtl(pstSrcChn, pstDestChn, 0);
}

CVI_S32 CVI_SYS_GetBindbyDest(const MMF_CHN_S *pstDestChn, MMF_CHN_S *pstSrcChn)
{
	CVI_S32 ret = 0;
	struct sys_bind_cfg bind_cfg;

	memset(&bind_cfg, 0, sizeof(struct sys_bind_cfg));
	bind_cfg.get_by_src = 0;
	bind_cfg.mmf_chn_dst = *pstDestChn;

	ret = cvi_sys_ioctl(SYS_GET_BINDCFG, &bind_cfg);
	if (ret) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "CVI_SYS_GetBindbyDest() failed\n");
		return ret;
	}

	memcpy(pstSrcChn, &bind_cfg.mmf_chn_src, sizeof(MMF_CHN_S));
	return CVI_SUCCESS;

}

CVI_S32 CVI_SYS_GetBindbySrc(const MMF_CHN_S *pstSrcChn, MMF_BIND_DEST_S *pstBindDest)
{
	CVI_S32 ret = 0;
	struct sys_bind_cfg bind_cfg;

	memset(&bind_cfg, 0, sizeof(struct sys_bind_cfg));
	bind_cfg.get_by_src = 1;
	bind_cfg.mmf_chn_src = *pstSrcChn;

	ret = cvi_sys_ioctl(SYS_GET_BINDCFG, &bind_cfg);

	if (ret) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "CVI_SYS_GetBindbySrc() failed\n");
		return ret;
	}

	memcpy(pstBindDest, &bind_cfg.bind_dst, sizeof(MMF_BIND_DEST_S));
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_GetVersion(MMF_VERSION_S *pstVersion)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstVersion);

#ifndef MMF_VERSION
#define MMF_VERSION  (CVI_CHIP_NAME MMF_VER_PRIX MK_VERSION(VER_X, VER_Y, VER_Z) VER_D)
#endif
	snprintf(pstVersion->version, VERSION_NAME_MAXLEN, "%s-%s", MMF_VERSION, "64bit");
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_GetChipId(CVI_U32 *pu32ChipId)
{
	static CVI_U32 id = 0xffffffff;

	if (id == 0xffffffff) {
		CVI_U32 tmp = 0;

		if (cvi_sys_ioctl(SYS_READ_CHIP_ID, &tmp) < 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "ioctl SYS_READ_CHIP_ID failed\n");
			return CVI_FAILURE;
		}
		id = tmp;
	}

	*pu32ChipId = id;
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_GetPowerOnReason(CVI_U32 *pu32PowerOnReason)
{
	CVI_U32 ret_val = 0x0;
	CVI_U32 reason = 0x0;

	if (cvi_sys_ioctl(SYS_READ_CHIP_PWR_ON_REASON, &reason) < 0) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "SYS_READ_CHIP_PWR_ON_REASON failed\n");
		return CVI_FAILURE;
	}

	switch (reason) {
	case E_CHIP_PWR_ON_COLDBOOT:
		ret_val = CVI_COLDBOOT;
	break;
	case E_CHIP_PWR_ON_WDT:
		ret_val = CVI_WDTBOOT;
	break;
	case E_CHIP_PWR_ON_SUSPEND:
		ret_val = CVI_SUSPENDBOOT;
	break;
	case E_CHIP_PWR_ON_WARM_RST:
		ret_val = CVI_WARMBOOT;
	break;
	default:
		CVI_TRACE_SYS(CVI_DBG_ERR, "unknown reason (%#x)\n", reason);
		return CVI_ERR_SYS_NOT_PERM;
	break;
	}

	*pu32PowerOnReason = ret_val;
	return CVI_SUCCESS;
}

#if CONFIG_DUALOS_NO_CROP
CVI_S32 CVI_SYS_GetChipSN(CVI_U8 *pu8SN, CVI_U32 u32SNSize)
{
	int ret = 0;
	uint32_t size = 0;

	if (u32SNSize < 8) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "size is invailed\n");
		return CVI_FAILURE;
	}

	ret = cvi_efuse_get_chip_sn(pu8SN, &size);
	if (ret < 0) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "cvi_base_get_chip_sn failed\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}
#endif //#if CONFIG_DUALOS_NO_CROP

CVI_S32 CVI_SYS_GetChipVersion(CVI_U32 *pu32ChipVersion)
{
	static CVI_U32 version = 0xffffffff;

	if (version == 0xffffffff) {
		CVI_U32 tmp = 0;

		if (cvi_sys_ioctl(SYS_READ_CHIP_VERSION, &tmp) < 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "ioctl SYS_READ_CHIP_VERSION failed\n");
			return CVI_FAILURE;
		}

		switch (tmp) {
		case E_CHIPVERSION_U01:
			version = CVIU01;
		break;
		case E_CHIPVERSION_U02:
			version = CVIU02;
		break;
		default:
			CVI_TRACE_SYS(CVI_DBG_ERR, "unknown version(%#x)\n", tmp);
			return CVI_ERR_SYS_NOT_PERM;
		break;
		}
	}

	*pu32ChipVersion = version;
	return CVI_SUCCESS;
}

static CVI_S32 _SYS_IonAlloc(CVI_U64 *pu64PhyAddr, CVI_VOID **ppVirAddr,
			     CVI_U32 u32Len, CVI_BOOL cached, const CVI_CHAR *name)
{
	UNUSED(name);
	UNUSED(cached);

	if (u32Len == 0) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "can't alloc size 0.\n");
		return -1;
	}

	CVI_VOID *p = aos_ion_malloc(u32Len);
	if (!p) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "alloc failed.\n");
		return -1;
	}

	if (ppVirAddr)
		*ppVirAddr = p;

	*pu64PhyAddr = (CVI_U64)p;
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_IonAlloc(CVI_U64 *pu64PhyAddr, CVI_VOID **ppVirAddr, const CVI_CHAR *strName, CVI_U32 u32Len)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pu64PhyAddr);
	return _SYS_IonAlloc(pu64PhyAddr, ppVirAddr, u32Len, CVI_FALSE, strName);
}

/* CVI_SYS_IonAlloc_Cached - acquire buffer of u32Len from ion
 *
 * @param pu64PhyAddr: the phy-address of the buffer
 * @param ppVirAddr: the cached vir-address of the buffer
 * @param strName: the name of the buffer
 * @param u32Len: the length of the buffer acquire
 * @return CVI_SUCCES if ok
 */
CVI_S32 CVI_SYS_IonAlloc_Cached(CVI_U64 *pu64PhyAddr, CVI_VOID **ppVirAddr,
				 const CVI_CHAR *strName, CVI_U32 u32Len)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pu64PhyAddr);
	return _SYS_IonAlloc(pu64PhyAddr, ppVirAddr, u32Len, CVI_TRUE, strName);
}

CVI_S32 CVI_SYS_IonFree(CVI_U64 u64PhyAddr, CVI_VOID *pVirAddr)
{
	if (pVirAddr)
		aos_ion_free(pVirAddr);
	else
		aos_ion_free((void *)u64PhyAddr);
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_IonFlushCache(CVI_U64 u64PhyAddr, CVI_VOID *pVirAddr, CVI_U32 u32Len)
{
	UNUSED(pVirAddr);
	flush_dcache_range((uintptr_t)u64PhyAddr, u32Len);
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_IonInvalidateCache(CVI_U64 u64PhyAddr, CVI_VOID *pVirAddr, CVI_U32 u32Len)
{
	UNUSED(pVirAddr);
	inv_dcache_range((uintptr_t)u64PhyAddr, u32Len);
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_SetVIVPSSMode(const VI_VPSS_MODE_S *pstVIVPSSMode)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstVIVPSSMode);

	return sys_set_vivpssmode(pstVIVPSSMode);
}

CVI_S32 CVI_SYS_GetVIVPSSMode(VI_VPSS_MODE_S *pstVIVPSSMode)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstVIVPSSMode);

	return sys_get_vivpssmode(pstVIVPSSMode);
}

CVI_S32 CVI_SYS_SetVPSSMode(VPSS_MODE_E enVPSSMode)
{
	if (sys_set_vpssmode(enVPSSMode))
		return -1;

	stVPSSMode.enMode = enVPSSMode;
	return CVI_SYS_SetVPSSModeEx(&stVPSSMode);
}

VPSS_MODE_E CVI_SYS_GetVPSSMode(void)
{
	VPSS_MODE_E enMode;

	if (sys_get_vpssmode(&enMode))
		return -1;

	return enMode;
}

CVI_S32 CVI_SYS_SetVPSSModeEx(const VPSS_MODE_S *pstVPSSMode)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstVPSSMode);

	return sys_set_vpssmodeex(pstVPSSMode);
}

CVI_S32 CVI_SYS_GetVPSSModeEx(VPSS_MODE_S *pstVPSSMode)
{
	VPSS_MODE_S vpss_mode;

	if (sys_get_vpssmodeex(&vpss_mode))
		return -1;

	*pstVPSSMode = vpss_mode;

	return CVI_SUCCESS;
}

const CVI_CHAR *CVI_SYS_GetModName(MOD_ID_E id)
{
	return CVI_GET_MOD_NAME(id);
}

CVI_S32 CVI_LOG_SetLevelConf(LOG_LEVEL_CONF_S *pstConf)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstConf);

	if (pstConf->enModId >= CVI_ID_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "Invalid ModId(%d)\n", pstConf->enModId);
		return CVI_ERR_SYS_ILLEGAL_PARAM;
	}

	log_levels[pstConf->enModId] = pstConf->s32Level;
	return CVI_SUCCESS;
}

CVI_S32 CVI_LOG_GetLevelConf(LOG_LEVEL_CONF_S *pstConf)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pstConf);

	if (pstConf->enModId >= CVI_ID_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "Invalid ModId(%d)\n", pstConf->enModId);
		return CVI_ERR_SYS_ILLEGAL_PARAM;
	}

	pstConf->s32Level = log_levels[pstConf->enModId];
	return CVI_SUCCESS;
}

CVI_S32 CVI_SYS_GetCurPTS(CVI_U64 *pu64CurPTS)
{
	MOD_CHECK_NULL_PTR(CVI_ID_SYS, pu64CurPTS);

	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	*pu64CurPTS = (CVI_U64)ts.tv_sec*1000000 + ts.tv_nsec/1000;

	return CVI_SUCCESS;

}

CVI_S32 CVI_SYS_VI_Open(void)
{
	CVI_S32 s32ret = CVI_SUCCESS;

	s32ret = vi_dev_open();
	if (s32ret != CVI_SUCCESS) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base_vi_open failed\n");
		return CVI_ERR_SYS_NOTREADY;
	}

	return s32ret;
}

CVI_S32 CVI_SYS_VI_Close(void)
{
	CVI_S32 s32ret = CVI_SUCCESS;

	s32ret = vi_dev_close();
	if (s32ret != CVI_SUCCESS) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base_vi_close failed\n");
		return CVI_ERR_SYS_NOTREADY;
	}

	return s32ret;
}
