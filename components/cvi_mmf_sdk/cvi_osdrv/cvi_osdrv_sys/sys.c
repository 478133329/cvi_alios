#include <stdio.h>
#include <aos/aos.h>
#include <aos/kernel.h>

#include "sys_context.h"
#include "sys.h"
#include "sys_uapi.h"
#include "cvi_comm_sys.h"
#include "cvi_defines.h"
#include "base.h"
#include "base_cb.h"
#include "vi_cb.h"
#include "vpss_cb.h"
#include "cvi_debug.h"
#include "mmio.h"

static struct base_m_cb_info *sys_m_cb;

#define GENERATE_STRING(STRING) (#STRING),
static const char *const MOD_STRING[] = FOREACH_MOD(GENERATE_STRING);

const char *sys_get_modname(MOD_ID_E id)
{
	return (id < CVI_ID_BUTT) ? MOD_STRING[id] : "UNDEF";
}

uint32_t sys_get_chipid(void)
{
	struct sys_info *p_info = (struct sys_info *)sys_ctx_get_sysinfo();
	return p_info->chip_id;
}

uint8_t *sys_get_version(void)
{
	struct sys_info *p_info = (struct sys_info *)sys_ctx_get_sysinfo();
	return (uint8_t *)p_info->version;
}

static int32_t sys_set_bind_cfg(struct sys_bind_cfg *ioctl_arg)
{
	int32_t ret = 0;

	if (ioctl_arg->is_bind)
		ret = sys_ctx_bind(&ioctl_arg->mmf_chn_src, &ioctl_arg->mmf_chn_dst);
	else
		ret = sys_ctx_unbind(&ioctl_arg->mmf_chn_src, &ioctl_arg->mmf_chn_dst);

	return ret;
}

static int32_t sys_get_bind_cfg(struct sys_bind_cfg *ioctl_arg)
{
	int32_t ret = 0;

	if (ioctl_arg->get_by_src)
		ret = sys_ctx_get_bindbysrc(&ioctl_arg->mmf_chn_src, &ioctl_arg->bind_dst);
	else
		ret = sys_ctx_get_bindbydst(&ioctl_arg->mmf_chn_dst, &ioctl_arg->mmf_chn_src);

	if (ret)
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys_ctx_getbind failed\n");

	return ret;
}

int32_t sys_bind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn)
{
	return sys_ctx_bind(pstSrcChn, pstDestChn);
}

int32_t sys_unbind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn)
{
	return sys_ctx_unbind(pstSrcChn, pstDestChn);
}

int32_t sys_ion_alloc(uint64_t *p_paddr, void **pp_vaddr, char *buf_name, uint32_t buf_len, bool is_cached)
{
	UNUSED(buf_name);
	UNUSED(is_cached);

	if (buf_len == 0) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "can't alloc size 0.\n");
		return -1;
	}

	//CVI_VOID *p = aos_malloc(buf_len);
	CVI_VOID *p = aos_ion_malloc(buf_len);
	if (!p) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "alloc failed.\n");
		return -1;
	}

	if (pp_vaddr)
		*pp_vaddr = p;

	*p_paddr = (uint64_t)p;
	return CVI_SUCCESS;
}

int32_t sys_ion_free(uint64_t u64PhyAddr)
{
	aos_ion_free((void *)u64PhyAddr);
	return CVI_SUCCESS;
}

int32_t sys_get_bindbysrc(MMF_CHN_S *pstSrcChn, MMF_BIND_DEST_S *pstBindDest)
{
	return sys_ctx_get_bindbysrc(pstSrcChn, pstBindDest);
}

int32_t sys_get_bindbydst(MMF_CHN_S *pstDestChn, MMF_CHN_S *pstSrcChn)
{
	return sys_ctx_get_bindbydst(pstSrcChn, pstSrcChn);
}

void sys_save_modules_cb(void *base_m_cb)
{
	sys_m_cb = (struct base_m_cb_info *)base_m_cb;
}

int _sys_exe_module_cb(struct base_exe_m_cb *exe_cb)
{
	struct base_m_cb_info *cb_info;

	if (exe_cb->caller < 0 || exe_cb->caller >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys exe cb error: wrong caller\n");
		return -1;
	}

	if (exe_cb->callee < 0 || exe_cb->callee >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys exe cb error: wrong callee\n");
		return -1;
	}

	if (!sys_m_cb) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys_m_cb/base_m_cb not ready yet\n");
		return -1;
	}

	cb_info = &sys_m_cb[exe_cb->callee];
	if (!cb_info->cb) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "sys exe cb error\n");
		return -1;
	}

	return cb_info->cb(cb_info->dev, exe_cb->caller, exe_cb->cmd_id, exe_cb->data);
}

static int _sys_call_cb(u32 m_id, u32 cmd_id, void *data)
{
	struct base_exe_m_cb exe_cb;

	exe_cb.callee = m_id;
	exe_cb.caller = E_MODULE_SYS;
	exe_cb.cmd_id = cmd_id;
	exe_cb.data   = (void *)data;

	return _sys_exe_module_cb(&exe_cb);
}

int _sys_s_ctrl(struct sys_ext_control *p)
{
	u32 id = p->id;
	int rc = -1;
	struct sys_ctx_info *sys_ctx = NULL;

	sys_ctx = sys_get_ctx();

	switch (id) {
	case SYS_IOCTL_SET_VIVPSSMODE:
	{
		VI_VPSS_MODE_S *stVIVPSSMode;

		stVIVPSSMode = &sys_ctx->mode_cfg.vivpss_mode;
		memcpy(stVIVPSSMode, p->ptr, sizeof(VI_VPSS_MODE_S));

		if (_sys_call_cb(E_MODULE_VI, VI_CB_SET_VIVPSSMODE, stVIVPSSMode) != 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VI_CB_SET_VIVPSSMODE failed\n");
			break;
		}

		if (_sys_call_cb(E_MODULE_VPSS, VPSS_CB_SET_VIVPSSMODE, stVIVPSSMode) != 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VPSS_CB_SET_VIVPSSMODE failed\n");
			break;
		}

		rc = 0;
		break;
	}
	case SYS_IOCTL_SET_VPSSMODE:
	{
		VPSS_MODE_E enVPSSMode;

		sys_ctx->mode_cfg.vpss_mode.enMode = enVPSSMode = (VPSS_MODE_E)p->value;

		if (_sys_call_cb(E_MODULE_VPSS, VPSS_CB_SET_VPSSMODE, (void *)&enVPSSMode) != 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VPSS_CB_SET_VPSSMODE failed\n");
			break;
		}

		rc = 0;
		break;
	}
	case SYS_IOCTL_SET_VPSSMODE_EX:
	{
		VPSS_MODE_S *stVPSSMode;

		stVPSSMode = &sys_ctx->mode_cfg.vpss_mode;
		memcpy(stVPSSMode, p->ptr, sizeof(VPSS_MODE_S));

		if (_sys_call_cb(E_MODULE_VPSS, VPSS_CB_SET_VPSSMODE_EX, (void *)stVPSSMode) != 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VPSS_CB_SET_VPSSMODE_EX failed\n");
			break;
		}

		rc = 0;
		break;
	}
	case SYS_IOCTL_SET_FBONSC:
	{
		CVI_BOOL bFbOnSc;

		bFbOnSc = (VPSS_MODE_E)p->value;
		CVI_TRACE_SYS(CVI_DBG_INFO, "SYS_IOCTL_SET_FBONSC bFbOnSc(%d).\n", bFbOnSc);

		if (_sys_call_cb(E_MODULE_VPSS, VPSS_CB_SET_FB_ON_VPSS, (void *)&bFbOnSc) != 0) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VPSS_CB_SET_FB_ON_VPSS failed\n");
			break;
		}

		rc = 0;
		break;
	}
	case SYS_IOCTL_SET_SYS_INIT:
	{
		sys_ctx->sys_inited = 1;

		rc = 0;
		break;
	}
	default:
		break;
	}

	return rc;
}

int _sys_g_ctrl(struct sys_ext_control *p)
{
	u32 id = p->id;
	int rc = -1;
	struct sys_ctx_info *sys_ctx = NULL;

	sys_ctx = sys_get_ctx();

	switch (id) {
	case SYS_IOCTL_GET_VIVPSSMODE:
	{
		memcpy(p->ptr, &sys_ctx->mode_cfg.vivpss_mode, sizeof(VI_VPSS_MODE_S));
		rc = 0;
		break;
	}
	case SYS_IOCTL_GET_VPSSMODE:
	{
		p->value = sys_ctx->mode_cfg.vpss_mode.enMode;

		rc = 0;
		break;
	}
	case SYS_IOCTL_GET_VPSSMODE_EX:
	{
		memcpy(p->ptr, &sys_ctx->mode_cfg.vpss_mode, sizeof(VPSS_MODE_S));
		rc = 0;
		break;
	}
	case SYS_IOCTL_GET_SYS_INIT:
	{
		p->value = sys_ctx->sys_inited;

		rc = 0;
		break;
	}
	default:
		break;
	}

	return rc;
}

static int _cvi_sys_sg_ctrl(unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sys_ext_control p;

	memcpy(&p, (void *)arg, sizeof(struct sys_ext_control));

	switch (cmd) {
	case SYS_IOC_S_CTRL:
		ret = _sys_s_ctrl(&p);
		break;
	case SYS_IOC_G_CTRL:
		ret = _sys_g_ctrl(&p);
		break;
	default:
		ret = -1;
		break;
	}

	memcpy((void *)arg, &p, sizeof(struct sys_ext_control));

	return ret;
}

int cvi_sys_ioctl(unsigned int cmd, void *arg)
{
	int ret = 0;

	switch (cmd) {
	case SYS_IOC_S_CTRL:
	case SYS_IOC_G_CTRL:
		ret = _cvi_sys_sg_ctrl(cmd, arg);
		break;
	case SYS_SET_BINDCFG:
		ret = sys_set_bind_cfg((struct sys_bind_cfg *)arg);
		break;
	case SYS_GET_BINDCFG:
		ret = sys_get_bind_cfg((struct sys_bind_cfg *)arg);
		break;
	case SYS_READ_CHIP_ID:
		*((uint32_t *)arg) = sys_get_chipid();
		break;
	case SYS_READ_CHIP_VERSION:
		*((uint32_t *)arg) = cvi_base_read_chip_version();
		break;
	case SYS_READ_CHIP_PWR_ON_REASON:
		*((uint32_t *)arg) = cvi_base_read_chip_pwr_on_reason();
		break;

	default:
		return -1;
	}
	return ret;
}

int cvi_sys_open()
{
	return 0;
}

int cvi_sys_close()
{
	struct sys_ctx_info *sys_ctx = NULL;

	sys_ctx = sys_get_ctx();
	sys_ctx_release_bind();
	sys_ctx->sys_inited = 0;
	return 0;
}

#define WDT2_TOC_ADDR           (0x0301201C)

static void heartbeat_task_entry(void *arg)
{
	uint32_t count = 0;

	while (1) {
		count++;

		mmio_write_32(WDT2_TOC_ADDR, count);
		aos_msleep(500);
	}
}

int cvi_sys_heartbeatinit(void)
{
	static uint8_t heartbeat_inited = 0;
	aos_task_t task;

	if (heartbeat_inited != 0)
		return 0;

	aos_task_new_ext(&task, "heartbeat", heartbeat_task_entry, NULL,
					1024, AOS_DEFAULT_APP_PRI - 5);

	heartbeat_inited = 1;

	return 0;
}

int sys_core_init()
{
	struct sys_ctx_info *sys_ctx = sys_get_ctx();

	cvi_sys_heartbeatinit();

	sys_ctx_init();
	base_core_init();

	sys_ctx->sys_info.chip_id  = cvi_base_read_chip_id();
	CVI_TRACE_SYS(CVI_DBG_DEBUG, "CVITEK CHIP ID = %d\n", sys_ctx->sys_info.chip_id);

	return 0;
}

int sys_core_exit()
{
	base_core_exit();
	return 0;
}

