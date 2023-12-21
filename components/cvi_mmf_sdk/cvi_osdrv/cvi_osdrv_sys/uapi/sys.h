#ifndef __SYS_H__
#define __SYS_H__

#include <stdbool.h>
#include "rtos_types.h"
#include "sys/types.h"
#include "base.h"
#include <cvi_comm_sys.h>

int32_t sys_ion_dump(void);
int32_t sys_ion_alloc(uint64_t *p_paddr, void **pp_vaddr, char *buf_name, uint32_t buf_len, bool is_cached);
int32_t sys_ion_free(uint64_t u64PhyAddr);

int32_t sys_cache_invalidate(uint64_t addr_p, void *addr_v, uint32_t u32Len);
int32_t sys_cache_flush(uint64_t addr_p, void *addr_v, uint32_t u32Len);

uint32_t sys_get_chipid(void);
uint8_t *sys_get_version(void);
int32_t sys_get_bindbysrc(MMF_CHN_S *pstSrcChn, MMF_BIND_DEST_S *pstBindDest);
int32_t sys_get_bindbydst(MMF_CHN_S *pstDestChn, MMF_CHN_S *pstSrcChn);

int32_t sys_bind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn);
int32_t sys_unbind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn);

const char *sys_get_modname(MOD_ID_E id);
VPSS_MODE_E sys_get_vpssmode(void);

#if 0
#define SYS_IOCTL_BASE	'y'
#define SYS_ION_ALLOC		_IOWR(SYS_IOCTL_BASE, 0x01, unsigned long long)
#define SYS_ION_FREE		_IOW(SYS_IOCTL_BASE, 0x02, unsigned long long)
#define SYS_CACHE_INVLD	_IOW(SYS_IOCTL_BASE, 0x03, unsigned long long)
#define SYS_CACHE_FLUSH	_IOW(SYS_IOCTL_BASE, 0x04, unsigned long long)

#define SYS_INIT_USER		_IOW(SYS_IOCTL_BASE, 0x05, unsigned long long)
#define SYS_EXIT_USER		_IOW(SYS_IOCTL_BASE, 0x06, unsigned long long)
#define SYS_GET_SYSINFO	_IOR(SYS_IOCTL_BASE, 0x07, unsigned long long)

#define SYS_SET_MODECFG	_IOW(SYS_IOCTL_BASE, 0x08, unsigned long long)
#define SYS_GET_MODECFG	_IOR(SYS_IOCTL_BASE, 0x08, unsigned long long)
#define SYS_SET_BINDCFG	_IOW(SYS_IOCTL_BASE, 0x09, unsigned long long)
#endif

#endif  /* __SYS_H__ */

