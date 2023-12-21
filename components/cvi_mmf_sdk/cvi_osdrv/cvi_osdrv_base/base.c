#include <stdio.h>
#include "base.h"
#include "vb.h"
#include <cvi_base_ctx.h>
#include <base_cb.h>
#include <sys_k.h>
#include <vip_common.h>
#include "cvi_efuse.h"
#include "cvi_reg.h"
#include "cvi_debug.h"


/* register bank */
#define TOP_BASE 0x03000000
#define TOP_REG_BANK_SIZE 0x10000
#define GP_REG3_OFFSET 0x8C
#define GP_REG_CHIP_ID_MASK 0xFFFF

#define CV182X_RTC_BASE 0x05026000
#define RTC_REG_BANK_SIZE 0x140
#define RTC_ST_ON_REASON 0xF8

/* sensor cmm extern function. */
enum vip_sys_cmm {
	VIP_CMM_I2C = 0,
	VIP_CMM_SSP,
	VIP_CMM_BUTT,
};

struct vip_sys_cmm_ops {
	long (*cb)(void *hdlr, unsigned int cmd, void *arg);
};

struct vip_sys_cmm_dev {
	enum vip_sys_cmm		cmm_type;
	void				*hdlr;
	struct vip_sys_cmm_ops		ops;
};

extern void vip_set_base_addr(void *base);

static struct vip_sys_cmm_dev cmm_ssp;
static struct vip_sys_cmm_dev cmm_i2c;

const char * const CB_MOD_STR[] = CB_FOREACH_MOD(CB_GENERATE_STRING);
static struct base_m_cb_info base_m_cb[E_MODULE_BUTT];

static unsigned int _cvi_base_read_by_kernel(unsigned int chip_id)
{
#ifdef __riscv
	switch (chip_id) {
	case 0x1810C:
		return E_CHIPID_CV1810C;
	case 0x1811C:
		return E_CHIPID_CV1811C;
	case 0x1812C:
		return E_CHIPID_CV1812C;
	case 0x1811F:
		return E_CHIPID_CV1811H;
	case 0x1812F:
		return E_CHIPID_CV1812H;
	case 0x1813F:
		return E_CHIPID_CV1813H;
	}
#else
	switch (chip_id) {
	case 0x1810C:
		return E_CHIPID_CV1820A;
	case 0x1811C:
		return E_CHIPID_CV1821A;
	case 0x1812C:
		return E_CHIPID_CV1822A;
	case 0x1811F:
		return E_CHIPID_CV1823A;
	case 0x1812F:
		return E_CHIPID_CV1825A;
	case 0x1813F:
		return E_CHIPID_CV1826A;
	}
#endif

	//mars default CV1810C
	return E_CHIPID_CV1810C;
}

unsigned int cvi_base_read_chip_id(void)
{

	unsigned int chip_id = _reg_read(TOP_BASE + GP_REG3_OFFSET);

	switch (chip_id) {
	case 0x1821:
		return E_CHIPID_CV1821;
	case 0x1822:
		return E_CHIPID_CV1822;
	case 0x1826:
		return E_CHIPID_CV1826;
	case 0x1832:
		return E_CHIPID_CV1832;
	case 0x1838:
		return E_CHIPID_CV1838;
	case 0x1829:
		return E_CHIPID_CV1829;
	case 0x1820:
		return E_CHIPID_CV1820;
	case 0x1823:
		return E_CHIPID_CV1823;
	case 0x1825:
		return E_CHIPID_CV1825;
	case 0x1835:
		return E_CHIPID_CV1835;

	case 0x1810C:
	case 0x1811C:
	case 0x1812C:
	case 0x1811F:
	case 0x1812F:
	case 0x1813F:
		return _cvi_base_read_by_kernel(chip_id);

	// cv180x
	case 0x1800B:
		return E_CHIPID_CV1800B;
	case 0x1801B:
		return E_CHIPID_CV1801B;
	case 0x1800C:
		return E_CHIPID_CV1800C;
	case 0x1801C:
		return E_CHIPID_CV1801C;

	//default cv1835
	default:
		return E_CHIPID_CV1835;
	}
}

unsigned int cvi_base_read_chip_version(void)
{
	unsigned int chip_version = 0;

	chip_version =  _reg_read(TOP_BASE);

	switch (chip_version) {
	case 0x18802000:
	case 0x18220000:
		return E_CHIPVERSION_U01;
	case 0x18802001:
	case 0x18220001:
		return E_CHIPVERSION_U02;
	default:
		return E_CHIPVERSION_U01;
	}
}

unsigned int cvi_base_read_chip_pwr_on_reason(void)
{
	unsigned int reason = 0;

	reason = _reg_read(CV182X_RTC_BASE + RTC_ST_ON_REASON);

	switch (reason) {
	case 0x800d0000:
	case 0x800f0000:
		return E_CHIP_PWR_ON_COLDBOOT;
	case 0x880d0003:
	case 0x880f0003:
		return E_CHIP_PWR_ON_WDT;
	case 0x80050009:
	case 0x800f0009:
		return E_CHIP_PWR_ON_SUSPEND;
	case 0x840d0003:
	case 0x840f0003:
		return E_CHIP_PWR_ON_WARM_RST;
	default:
		return E_CHIP_PWR_ON_COLDBOOT;
	}
}

int vip_sys_register_cmm_cb(unsigned long cmm, void *hdlr, void *cb)
{
	struct vip_sys_cmm_dev *cmm_dev;

	if ((cmm >= VIP_CMM_BUTT) || !hdlr || !cb)
		return -1;

	cmm_dev = (cmm == VIP_CMM_I2C) ? &cmm_i2c : &cmm_ssp;

	cmm_dev->cmm_type = cmm;
	cmm_dev->hdlr = hdlr;
	cmm_dev->ops.cb = cb;

	return 0;
}

int vip_sys_cmm_cb_i2c(unsigned int cmd, void *arg)
{
	struct vip_sys_cmm_dev *cmm_dev = &cmm_i2c;

	if (cmm_dev->cmm_type != VIP_CMM_I2C)
		return -1;

	return (cmm_dev->ops.cb) ? cmm_dev->ops.cb(cmm_dev->hdlr, cmd, arg) : (-1);
}

int vip_sys_cmm_cb_ssp(unsigned int cmd, void *arg)
{
	struct vip_sys_cmm_dev *cmm_dev = &cmm_ssp;

	if (cmm_dev->cmm_type != VIP_CMM_SSP)
		return -1;

	return (cmm_dev->ops.cb) ? cmm_dev->ops.cb(cmm_dev->hdlr, cmd, arg) : (-1);
}

int base_rm_module_cb(enum ENUM_MODULES_ID module_id)
{
	if (module_id < 0 || module_id >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base rm cb error: wrong module_id\n");
		return -1;
	}

	base_m_cb[module_id].dev = NULL;
	base_m_cb[module_id].cb  = NULL;

	return 0;
}

int base_reg_module_cb(struct base_m_cb_info *cb_info)
{
	if (!cb_info || !cb_info->dev || !cb_info->cb) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base reg cb error: no data\n");
		return -1;
	}

	if (cb_info->module_id < 0 || cb_info->module_id >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base reg cb error: wrong module_id\n");
		return -1;
	}

	base_m_cb[cb_info->module_id] = *cb_info;

	return 0;
}

int base_exe_module_cb(struct base_exe_m_cb *exe_cb)
{
	struct base_m_cb_info *cb_info;

	if (exe_cb->caller < 0 || exe_cb->caller >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base exe cb error: wrong caller\n");
		return -1;
	}

	if (exe_cb->callee < 0 || exe_cb->callee >= E_MODULE_BUTT) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "base exe cb error: wrong callee\n");
		return -1;
	}

	cb_info = &base_m_cb[exe_cb->callee];

	if (!cb_info->cb) {
		CVI_TRACE_SYS(CVI_DBG_INFO, "base exe cb error: cb of callee(%s) is null, caller(%s)\n",
			IDTOSTR(exe_cb->callee), IDTOSTR(exe_cb->caller));
		return -1;
	}

	return cb_info->cb(cb_info->dev, exe_cb->caller, exe_cb->cmd_id, exe_cb->data);
}

int base_core_init(void)
{
	sys_save_modules_cb(&base_m_cb[0]);

	vip_set_base_addr(REG_VIP_BASE_ADDR);
	vip_sys_set_offline(VIP_SYS_AXI_BUS_SC_TOP, true);
	vip_sys_set_offline(VIP_SYS_AXI_BUS_ISP_RAW, true);
	vip_sys_set_offline(VIP_SYS_AXI_BUS_ISP_YUV, true);

	memset(base_m_cb, 0, sizeof(struct base_m_cb_info) * E_MODULE_BUTT);

	return 0;
}

void base_core_exit(void)
{
}

