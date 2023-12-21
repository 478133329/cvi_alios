#ifndef _CVI_REG_H_
#define _CVI_REG_H_

#include "mmio.h"

#define REG_VIP_BASE_ADDR		(void *)0x0a0c8000
#define REG_ISP_BASE_ADDR		(void *)0x0a000000
#define REG_SCLR_BASE_ADDR		(void *)0x0a080000
#define REG_LDC_BASE_ADDR		(void *)0x0a0c0000
#define REG_DSI_PHY_BASE_ADDR		(void *)0x0a0d1000

#define VIP_INT_LDC_WRAP		24

#define _reg_read(addr) mmio_read_32(addr)
#define _reg_write(addr, data) mmio_write_32(addr, data)
#define _reg_write_mask(addr, mask, data) mmio_clrsetbits_32(addr, mask, data)


#endif //_CVI_REG_H_
