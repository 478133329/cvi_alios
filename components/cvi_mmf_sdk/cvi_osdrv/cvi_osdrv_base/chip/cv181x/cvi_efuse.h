#ifndef __CVI_EFUSE_H__
#define __CVI_EFUSE_H__

#include <stdint.h>
#include "rtos_types.h"


int64_t cvi_efuse_read_from_shadow(uint32_t addr);
int cvi_efuse_write(uint32_t addr, uint32_t value);

int cvi_efuse_read_buf(u32 addr, void *buf, size_t buf_size);

#endif /* __CVI_EFUSE_H__ */
