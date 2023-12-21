#ifndef __SYS_CONTEXT_H__
#define __SYS_CONTEXT_H__

#include "sys/types.h"
#include "base.h"
#include "k_atomic.h"
#include <cvi_comm_sys.h>

struct sys_info {
	char version[VERSION_NAME_MAXLEN];
	uint32_t chip_id;
};

struct sys_mode_cfg {
	VI_VPSS_MODE_S vivpss_mode;
	VPSS_MODE_S vpss_mode;
};

struct sys_ctx_info {
	struct sys_info sys_info;
	struct sys_mode_cfg mode_cfg;
	int sys_inited;
};

int32_t sys_ctx_init(void);
struct sys_ctx_info *sys_get_ctx(void);

uint32_t sys_ctx_get_chipid(void);
uint8_t *sys_ctx_get_version(void);
void *sys_ctx_get_sysinfo(void);

VPSS_MODE_E sys_ctx_get_vpssmode(void);

void sys_ctx_release_bind(void);
int32_t sys_ctx_bind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn);
int32_t sys_ctx_unbind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn);

int32_t sys_ctx_get_bindbysrc(MMF_CHN_S *pstSrcChn, MMF_BIND_DEST_S *pstBindDest);
int32_t sys_ctx_get_bindbydst(MMF_CHN_S *pstDestChn, MMF_CHN_S *pstSrcChn);



#endif  /* __SYS_CONTEXT_H__ */

