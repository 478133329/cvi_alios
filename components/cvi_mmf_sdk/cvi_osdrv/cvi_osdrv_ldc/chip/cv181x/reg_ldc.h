// $Module: reg_ldc $
// $RegisterBank Version: V 1.0.00 $
// $Author: andy.tsao $
// $Date: Fri, 14 Jan 2022 11:24:55 AM $
//

//GEN REG ADDR/OFFSET/MASK
#define  LDC_FORMAT  0x0
#define  LDC_RAS_MODE  0x4
#define  LDC_RAS_XSIZE  0x8
#define  LDC_RAS_YSIZE  0xc
#define  LDC_MAP_BASE  0x10
#define  LDC_MAP_BYPASS  0x14
#define  LDC_SRC_BASE_Y  0x20
#define  LDC_SRC_BASE_C  0x24
#define  LDC_SRC_XSIZE  0x28
#define  LDC_SRC_YSIZE  0x2c
#define  LDC_SRC_XSTR  0x30
#define  LDC_SRC_XEND  0x34
#define  LDC_SRC_BG  0x38
#define  LDC_DST_BASE_Y  0x40
#define  LDC_DST_BASE_C  0x44
#define  LDC_DST_MODE  0x48
#define  LDC_IRQEN  0x50
#define  LDC_START  0x54
#define  LDC_IRQSTAT  0x58
#define  LDC_IRQCLR  0x5c
#define  LDC_SB_MODE  0x60
#define  LDC_SB_NB  0x64
#define  LDC_SB_SIZE  0x68
#define  LDC_SB_SET_STR  0x6c
#define  LDC_SB_SW_WPTR  0x70
#define  LDC_SB_SW_CLR  0x74
#define  LDC_SB_FULL  0x78
#define  LDC_SB_EMPTY  0x7c
#define  LDC_SB_WPTR_RO  0x80
#define  LDC_SB_DPTR_RO  0x84
#define  LDC_SB_STRIDE  0x88
#define  LDC_LDC_DIR  0x8c
#define  LDC_LDC_CMDQ_IRQ_EN  0x90
#define  LDC_LDC_FORCE_IN_RANGE  0x90
#define  LDC_LDC_OUT_RANGE  0x94
#define  LDC_LDC_OUT_RANGE_DST_X  0x98
#define  LDC_LDC_OUT_RANGE_DST_Y  0x98
#define  LDC_LDC_OUT_RANGE_SRC_X  0x9c
#define  LDC_LDC_OUT_RANGE_SRC_Y  0x9c
#define  LDC_LDC_DST_TI_CNT_X  0xa0
#define  LDC_LDC_DST_TI_CNT_Y  0xa4
#define  LDC_REG_FORMAT   0x0
#define  LDC_REG_FORMAT_OFFSET 0
#define  LDC_REG_FORMAT_MASK   0x1
#define  LDC_REG_FORMAT_BITS   0x1
#define  LDC_REG_RAS_MODE   0x4
#define  LDC_REG_RAS_MODE_OFFSET 0
#define  LDC_REG_RAS_MODE_MASK   0x1
#define  LDC_REG_RAS_MODE_BITS   0x1
#define  LDC_REG_RAS_XSIZE   0x8
#define  LDC_REG_RAS_XSIZE_OFFSET 0
#define  LDC_REG_RAS_XSIZE_MASK   0xfff
#define  LDC_REG_RAS_XSIZE_BITS   0xc
#define  LDC_REG_RAS_YSIZE   0xc
#define  LDC_REG_RAS_YSIZE_OFFSET 0
#define  LDC_REG_RAS_YSIZE_MASK   0xfff
#define  LDC_REG_RAS_YSIZE_BITS   0xc
#define  LDC_REG_MAP_BASE   0x10
#define  LDC_REG_MAP_BASE_OFFSET 0
#define  LDC_REG_MAP_BASE_MASK   0x7ffffff
#define  LDC_REG_MAP_BASE_BITS   0x1b
#define  LDC_REG_MAP_BYPASS   0x14
#define  LDC_REG_MAP_BYPASS_OFFSET 0
#define  LDC_REG_MAP_BYPASS_MASK   0x1
#define  LDC_REG_MAP_BYPASS_BITS   0x1
#define  LDC_REG_SRC_BASE_Y   0x20
#define  LDC_REG_SRC_BASE_Y_OFFSET 0
#define  LDC_REG_SRC_BASE_Y_MASK   0x7ffffff
#define  LDC_REG_SRC_BASE_Y_BITS   0x1b
#define  LDC_REG_SRC_BASE_C   0x24
#define  LDC_REG_SRC_BASE_C_OFFSET 0
#define  LDC_REG_SRC_BASE_C_MASK   0x7ffffff
#define  LDC_REG_SRC_BASE_C_BITS   0x1b
#define  LDC_REG_SRC_XSIZE   0x28
#define  LDC_REG_SRC_XSIZE_OFFSET 0
#define  LDC_REG_SRC_XSIZE_MASK   0xfff
#define  LDC_REG_SRC_XSIZE_BITS   0xc
#define  LDC_REG_SRC_YSIZE   0x2c
#define  LDC_REG_SRC_YSIZE_OFFSET 0
#define  LDC_REG_SRC_YSIZE_MASK   0xfff
#define  LDC_REG_SRC_YSIZE_BITS   0xc
#define  LDC_REG_SRC_XSTR   0x30
#define  LDC_REG_SRC_XSTR_OFFSET 0
#define  LDC_REG_SRC_XSTR_MASK   0xfff
#define  LDC_REG_SRC_XSTR_BITS   0xc
#define  LDC_REG_SRC_XEND   0x34
#define  LDC_REG_SRC_XEND_OFFSET 0
#define  LDC_REG_SRC_XEND_MASK   0xfff
#define  LDC_REG_SRC_XEND_BITS   0xc
#define  LDC_REG_SRC_BG   0x38
#define  LDC_REG_SRC_BG_OFFSET 0
#define  LDC_REG_SRC_BG_MASK   0xffffff
#define  LDC_REG_SRC_BG_BITS   0x18
#define  LDC_REG_DST_BASE_Y   0x40
#define  LDC_REG_DST_BASE_Y_OFFSET 0
#define  LDC_REG_DST_BASE_Y_MASK   0x7ffffff
#define  LDC_REG_DST_BASE_Y_BITS   0x1b
#define  LDC_REG_DST_BASE_C   0x44
#define  LDC_REG_DST_BASE_C_OFFSET 0
#define  LDC_REG_DST_BASE_C_MASK   0x7ffffff
#define  LDC_REG_DST_BASE_C_BITS   0x1b
#define  LDC_REG_DST_MODE   0x48
#define  LDC_REG_DST_MODE_OFFSET 0
#define  LDC_REG_DST_MODE_MASK   0x3
#define  LDC_REG_DST_MODE_BITS   0x2
#define  LDC_REG_IRQEN   0x50
#define  LDC_REG_IRQEN_OFFSET 0
#define  LDC_REG_IRQEN_MASK   0x1
#define  LDC_REG_IRQEN_BITS   0x1
#define  LDC_PULSE_START   0x54
#define  LDC_PULSE_START_OFFSET 0
#define  LDC_PULSE_START_MASK   0x1
#define  LDC_PULSE_START_BITS   0x1
#define  LDC_REG_IRQSTAT   0x58
#define  LDC_REG_IRQSTAT_OFFSET 0
#define  LDC_REG_IRQSTAT_MASK   0x3
#define  LDC_REG_IRQSTAT_BITS   0x2
#define  LDC_PULSE_IRQCLR   0x5c
#define  LDC_PULSE_IRQCLR_OFFSET 0
#define  LDC_PULSE_IRQCLR_MASK   0x1
#define  LDC_PULSE_IRQCLR_BITS   0x1
#define  LDC_REG_SB_MODE   0x60
#define  LDC_REG_SB_MODE_OFFSET 0
#define  LDC_REG_SB_MODE_MASK   0x3
#define  LDC_REG_SB_MODE_BITS   0x2
#define  LDC_REG_SB_NB   0x64
#define  LDC_REG_SB_NB_OFFSET 0
#define  LDC_REG_SB_NB_MASK   0xf
#define  LDC_REG_SB_NB_BITS   0x4
#define  LDC_REG_SB_SIZE   0x68
#define  LDC_REG_SB_SIZE_OFFSET 0
#define  LDC_REG_SB_SIZE_MASK   0x3
#define  LDC_REG_SB_SIZE_BITS   0x2
#define  LDC_REG_SB_SET_STR   0x6c
#define  LDC_REG_SB_SET_STR_OFFSET 0
#define  LDC_REG_SB_SET_STR_MASK   0x1
#define  LDC_REG_SB_SET_STR_BITS   0x1
#define  LDC_REG_SB_SW_WPTR   0x70
#define  LDC_REG_SB_SW_WPTR_OFFSET 0
#define  LDC_REG_SB_SW_WPTR_MASK   0xf
#define  LDC_REG_SB_SW_WPTR_BITS   0x4
#define  LDC_REG_SB_SW_CLR   0x74
#define  LDC_REG_SB_SW_CLR_OFFSET 0
#define  LDC_REG_SB_SW_CLR_MASK   0x1
#define  LDC_REG_SB_SW_CLR_BITS   0x1
#define  LDC_REG_SB_FULL   0x78
#define  LDC_REG_SB_FULL_OFFSET 0
#define  LDC_REG_SB_FULL_MASK   0x1
#define  LDC_REG_SB_FULL_BITS   0x1
#define  LDC_REG_SB_EMPTY   0x7c
#define  LDC_REG_SB_EMPTY_OFFSET 0
#define  LDC_REG_SB_EMPTY_MASK   0x1
#define  LDC_REG_SB_EMPTY_BITS   0x1
#define  LDC_REG_WPTR_RO   0x80
#define  LDC_REG_WPTR_RO_OFFSET 0
#define  LDC_REG_WPTR_RO_MASK   0xf
#define  LDC_REG_WPTR_RO_BITS   0x4
#define  LDC_REG_DPTR_RO   0x84
#define  LDC_REG_DPTR_RO_OFFSET 0
#define  LDC_REG_DPTR_RO_MASK   0x1f
#define  LDC_REG_DPTR_RO_BITS   0x5
#define  LDC_REG_LINE_STRIDE   0x88
#define  LDC_REG_LINE_STRIDE_OFFSET 0
#define  LDC_REG_LINE_STRIDE_MASK   0xfff
#define  LDC_REG_LINE_STRIDE_BITS   0xc
#define  LDC_REG_LDC_DIR   0x8c
#define  LDC_REG_LDC_DIR_OFFSET 0
#define  LDC_REG_LDC_DIR_MASK   0x1
#define  LDC_REG_LDC_DIR_BITS   0x1
#define  LDC_REG_CMDQ_IRQ_EN   0x90
#define  LDC_REG_CMDQ_IRQ_EN_OFFSET 0
#define  LDC_REG_CMDQ_IRQ_EN_MASK   0x1
#define  LDC_REG_CMDQ_IRQ_EN_BITS   0x1
#define  LDC_REG_LDC_FORCE_IN_RANGE   0x90
#define  LDC_REG_LDC_FORCE_IN_RANGE_OFFSET 4
#define  LDC_REG_LDC_FORCE_IN_RANGE_MASK   0x10
#define  LDC_REG_LDC_FORCE_IN_RANGE_BITS   0x1
#define  LDC_REG_LDC_OUT_RANGE   0x94
#define  LDC_REG_LDC_OUT_RANGE_OFFSET 0
#define  LDC_REG_LDC_OUT_RANGE_MASK   0x1
#define  LDC_REG_LDC_OUT_RANGE_BITS   0x1
#define  LDC_REG_LDC_OUT_RANGE_DST_X   0x98
#define  LDC_REG_LDC_OUT_RANGE_DST_X_OFFSET 0
#define  LDC_REG_LDC_OUT_RANGE_DST_X_MASK   0xfff
#define  LDC_REG_LDC_OUT_RANGE_DST_X_BITS   0xc
#define  LDC_REG_LDC_OUT_RANGE_DST_Y   0x98
#define  LDC_REG_LDC_OUT_RANGE_DST_Y_OFFSET 16
#define  LDC_REG_LDC_OUT_RANGE_DST_Y_MASK   0xfff0000
#define  LDC_REG_LDC_OUT_RANGE_DST_Y_BITS   0xc
#define  LDC_REG_LDC_OUT_RANGE_SRC_X   0x9c
#define  LDC_REG_LDC_OUT_RANGE_SRC_X_OFFSET 0
#define  LDC_REG_LDC_OUT_RANGE_SRC_X_MASK   0xfff
#define  LDC_REG_LDC_OUT_RANGE_SRC_X_BITS   0xc
#define  LDC_REG_LDC_OUT_RANGE_SRC_Y   0x9c
#define  LDC_REG_LDC_OUT_RANGE_SRC_Y_OFFSET 16
#define  LDC_REG_LDC_OUT_RANGE_SRC_Y_MASK   0xfff0000
#define  LDC_REG_LDC_OUT_RANGE_SRC_Y_BITS   0xc
#define  LDC_REG_LDC_DST_TI_X   0xa0
#define  LDC_REG_LDC_DST_TI_X_OFFSET 0
#define  LDC_REG_LDC_DST_TI_X_MASK   0x3f
#define  LDC_REG_LDC_DST_TI_X_BITS   0x6
#define  LDC_REG_LDC_DST_TI_Y   0xa4
#define  LDC_REG_LDC_DST_TI_Y_OFFSET 8
#define  LDC_REG_LDC_DST_TI_Y_MASK   0x3f00
#define  LDC_REG_LDC_DST_TI_Y_BITS   0x6
