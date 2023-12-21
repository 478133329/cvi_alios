#ifndef __CVI_VC_CTRL_H__
#define __CVI_VC_CTRL_H__
#include "stdio.h"
#include "cvi_pthread.h"

#ifndef NULL
#define NULL 0
#endif

#define CtrlWriteReg(CORE, ADDR, DATA) cvi_vc_drv_write_vc_reg(REG_CTRL, ADDR, DATA)
#define CtrlReadReg(CORE, ADDR) cvi_vc_drv_read_vc_reg(REG_CTRL, ADDR)
#define RemapWriteReg(CORE, ADDR, DATA) cvi_vc_drv_write_vc_reg(REG_REMAP, ADDR, DATA)
#define RemapReadReg(CORE, ADDR) cvi_vc_drv_read_vc_reg(REG_REMAP, ADDR)
#define ReadSbmRegister(ADDR) cvi_vc_drv_read_vc_reg(REG_SBM, ADDR)
#define WritSbmRegister(ADDR, DATA) cvi_vc_drv_write_vc_reg(REG_SBM, ADDR, DATA)

struct cvi_vc_drv_context {
	SEM_T s_sbm_interrupt_sem;
	unsigned int irq_num;
	int binit;
};

typedef enum {
	REG_CTRL = 0,
	REG_SBM,
	REG_REMAP,
} REG_TYPE;

void cvi_vc_drv_init(void);
unsigned int cvi_vc_drv_read_vc_reg(REG_TYPE eRegType, unsigned long addr);
void cvi_vc_drv_write_vc_reg(REG_TYPE eRegType, unsigned long addr, unsigned int data);
void cvi_VENC_SBM_IrqEnable(void);
void cvi_VENC_SBM_IrqDisable(void);
void wake_sbm_waitinng(void);
int sbm_wait_interrupt(int timeout);
void cvi_vc_drv_init(void);



#endif /* __CVI_VC_CTRL_H__ */

