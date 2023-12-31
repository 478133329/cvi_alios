/*
* Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
*/
#ifndef __DW_SPI_HEADER_H__
#define __DW_SPI_HEADER_H__

#include <drv/common.h>
#include "io.h"
#include "drv/spi.h"

#define SPI_REGBASE                     0x04180000
#define SPI_REF_CLK                     187500000
#define MAX_SPI_NUM                     4

#define DW_SPI_CTRLR0                   0x00
#define DW_SPI_CTRLR1                   0x04
#define DW_SPI_SSIENR                   0x08
#define DW_SPI_MWCR                     0x0c
#define DW_SPI_SER                      0x10
#define DW_SPI_BAUDR                    0x14
#define DW_SPI_TXFTLR                   0x18
#define DW_SPI_RXFTLR                   0x1c
#define DW_SPI_TXFLR                    0x20
#define DW_SPI_RXFLR                    0x24
#define DW_SPI_SR                       0x28
#define DW_SPI_IMR                      0x2c
#define DW_SPI_ISR                      0x30
#define DW_SPI_RISR                     0x34
#define DW_SPI_TXOICR                   0x38
#define DW_SPI_RXOICR                   0x3c
#define DW_SPI_RXUICR                   0x40
#define DW_SPI_MSTICR                   0x44
#define DW_SPI_ICR                      0x48
#define DW_SPI_DMACR                    0x4c
#define DW_SPI_DMATDLR                  0x50
#define DW_SPI_DMARDLR                  0x54
#define DW_SPI_IDR                      0x58
#define DW_SPI_VERSION                  0x5c
#define DW_SPI_DR                       0x60

/* Bit fields in CTRLR0 */
#define SPI_DFS_OFFSET                  0

#define SPI_FRF_OFFSET                  4
#define SPI_FRF_SPI                     0x0
#define SPI_FRF_SSP                     0x1
#define SPI_FRF_MICROWIRE               0x2
#define SPI_FRF_RESV                    0x3

#define SPI_MODE_OFFSET                 6
#define SPI_SCPH_OFFSET                 6
#define SPI_SCOL_OFFSET                 7

#define SPI_TMOD_OFFSET                 8
#define SPI_TMOD_MASK                   (0x3 << SPI_TMOD_OFFSET)
#define SPI_TMOD_TR                     0x0             /* xmit & recv */
#define SPI_TMOD_TO                     0x1             /* xmit only */
#define SPI_TMOD_RO                     0x2             /* recv only */
#define SPI_TMOD_EPROMREAD              0x3             /* eeprom read mode */

#define SPI_SLVOE_OFFSET                10
#define SPI_SRL_OFFSET                  11
#define SPI_CFS_OFFSET                  12

/* Bit fields in SR, 7 bits */
#define SR_MASK                         0x7f
#define SR_BUSY                         (1 << 0)
#define SR_TF_NOT_FULL                  (1 << 1)
#define SR_TF_EMPT                      (1 << 2)
#define SR_RF_NOT_EMPT                  (1 << 3)
#define SR_RF_FULL                      (1 << 4)
#define SR_TX_ERR                       (1 << 5)
#define SR_DCOL                         (1 << 6)

/* Bit fields in ISR, IMR, RISR, 7 bits */
#define SPI_INT_TXEI                    (1 << 0)
#define SPI_INT_TXOI                    (1 << 1)
#define SPI_INT_RXUI                    (1 << 2)
#define SPI_INT_RXOI                    (1 << 3)
#define SPI_INT_RXFI                    (1 << 4)
#define SPI_INT_MSTI                    (1 << 5)

/* Bit fields in DMACR */
#define SPI_DMA_RDMAE                   (1 << 0)
#define SPI_DMA_TDMAE                   (1 << 1)

/* TX RX interrupt level threshold, max can be 256 */
#define SPI_INT_THRESHOLD               32

struct dw_spi {
	void                    *regs;
	int                     irq;
	int                     index;
	uint32_t                fifo_len;       /* depth of the FIFO buffer */
	uint16_t                num_cs;         /* supported slave numbers */

	/* Current message transfer state info */
	size_t                  len;
	const void              *tx;
	const void                    *tx_end;
	void                    *rx;
	void                    *rx_end;
	uint8_t                 n_bytes;       /* current is a 1/2 bytes op */
	uint32_t                dma_width;
	//int             (*transfer_handler)(struct dw_spi *dws);
	int             (*transfer_handler)(csi_spi_t *spi);

	/* Bus interface info */
	void                    *priv;
};

#define SPI_CPHA                0x01
#define SPI_CPOL                0x02

#define SPI_MODE_0              (0|0)
#define SPI_MODE_1              (0|SPI_CPHA)
#define SPI_MODE_2              (SPI_CPOL|0)
#define SPI_MODE_3              (SPI_CPOL|SPI_CPHA)

enum transfer_type {
	POLL_TRAN = 0,
	IRQ_TRAN,
	DMA_TRAN,
};

enum dw_ssi_type {
	SSI_MOTO_SPI = 0,
	SSI_TI_SSP,
	SSI_NS_MICROWIRE,
};

#ifndef BIT
#define BIT(_n)  ( 1 << (_n))
#endif

#define SPI_DEBUG     1
#define         spi_debug(info, ...) \
                do { \
                        if (SPI_DEBUG > 3) { \
                                printf("[%s %d] ==> "info"",__func__, __LINE__,##__VA_ARGS__);} \
                } while (0);

static void dw_writel(struct dw_spi *dws, uint32_t off, uint32_t val)
{
	writel(val, (dws->regs + off));
}

//static void dw_writew(struct dw_spi *dws, uint32_t off, uint16_t val)
//{
//	writew(val, (dws->regs + off));
//}

static uint32_t dw_readl(struct dw_spi *dws, uint32_t off)
{
	return readl(dws->regs + off);
}

//static uint16_t dw_readw(struct dw_spi *dws, uint32_t off)
//{
//	return readw(dws->regs + off);
//}

static inline void spi_enable_chip(struct dw_spi *dws, int enable)
{
	dw_writel(dws, DW_SPI_SSIENR, (enable ? 1 : 0));
}

static inline void spi_set_clk(struct dw_spi *dws, uint16_t div)
{
	dw_writel(dws, DW_SPI_BAUDR, div);
}

/* Disable IRQ bits */
static inline void spi_mask_intr(struct dw_spi *dws, uint32_t mask)
{
	uint32_t new_mask;

	new_mask = dw_readl(dws, DW_SPI_IMR) & ~mask;
	dw_writel(dws, DW_SPI_IMR, new_mask);
}

static inline uint32_t spi_get_status(struct dw_spi *dws)
{
	return dw_readl(dws, DW_SPI_SR);
}

/* Enable IRQ bits */
static inline void spi_umask_intr(struct dw_spi *dws, uint32_t mask)
{
	uint32_t new_mask;

	new_mask = dw_readl(dws, DW_SPI_IMR) | mask;
	dw_writel(dws, DW_SPI_IMR, new_mask);
}

static inline void spi_reset_chip(struct dw_spi *dws)
{
	spi_enable_chip(dws, 0);
	spi_mask_intr(dws, 0xff);
	spi_enable_chip(dws, 1);
}

static inline void spi_enable_dma(struct dw_spi *dws, uint8_t is_tx, uint8_t op)
{
	/* 1: TDMAE, 0: RDMAE */
	uint32_t val = dw_readl(dws, DW_SPI_DMACR);

	if (op)
		val = 1 << (!!is_tx);
	else
		val &= ~(1 << (!!is_tx));

	dw_writel(dws, DW_SPI_DMACR, val);
}

static inline void spi_shutdown_chip(struct dw_spi *dws)
{
	spi_enable_chip(dws, 0);
	spi_set_clk(dws, 0);
}

void spi_hw_init(struct dw_spi *dws);
void dw_spi_set_controller_mode(struct dw_spi *dws, uint8_t enable_master);
void dw_spi_set_polarity_and_phase(struct dw_spi *dws, csi_spi_cp_format_t format);
void dw_spi_set_clock(struct dw_spi *dws, uint32_t clock_in, uint32_t clock_out);
int dw_spi_set_data_frame_len(struct dw_spi *dws, uint32_t size);
void dw_spi_set_cs(struct dw_spi *dws, bool enable, uint32_t index);
void dw_reader(struct dw_spi *dws);
void dw_writer(struct dw_spi *dws);
void set_tran_mode(struct dw_spi *dws);
void dw_spi_show_regs(struct dw_spi *dws);
int poll_transfer(struct dw_spi *dws);
#endif
