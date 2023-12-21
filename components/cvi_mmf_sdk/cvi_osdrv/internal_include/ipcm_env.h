
#ifndef __IPCM_ENV_H__
#define __IPCM_ENV_H__

#include <aos/kernel.h>
#include <debug/dbg.h>
#include "arch_helpers.h"

#define PR aos_debug_printf

#define ipcm_alloc malloc
#define ipcm_free free
#define ipcm_msleep aos_msleep
#define ipcm_mutex_t aos_mutex_t
#define ipcm_mutex_init aos_mutex_new
#define ipcm_mutex_uninit(x) if (aos_mutex_is_valid(x)) aos_mutex_free(x)
#define ipcm_mutex_lock(x) aos_mutex_lock((x),AOS_WAIT_FOREVER)
#define ipcm_mutex_unlock aos_mutex_unlock
#define ipcm_pool_cache_flush(paddr, vaddr, size) flush_dcache_range(paddr, size)
#define ipcm_pool_cache_invalidate(paddr, vaddr, size) inv_dcache_range(paddr, size)
#define ipcm_mailbox_irq_enable csi_irq_enable(MBOX_INT_C906_2ND);
#define ipcm_mailbox_irq_disable csi_irq_disable(MBOX_INT_C906_2ND);
#define ipcm_sem aos_sem_t *

#endif
