#include <vi.h>
#include <inttypes.h>
#include <cvi_base_ctx.h>
#include <proc/vi_dbg_proc.h>
#include <proc/vi_proc.h>
#include <proc/vi_isp_proc.h>
#include <vi_ext.h>
#include <base_cb.h>
#include <base_ctx.h>
#include <cif_cb.h>
#include <vpss_cb.h>
#include <vi_cb.h>
#include <ldc_cb.h>
#include <vb.h>
#include <vip/vi_perf_chk.h>

#include <vip_list.h>
#include <vi_raw_dump.h>
/*******************************************************
 *  MACRO defines
 ******************************************************/
#define DMA_SETUP_2(id, raw_num)					\
	do {								\
		bufaddr = _mempool_get_addr();				\
		ispblk_dma_setaddr(ictx, id, bufaddr);			\
		bufsize = ispblk_dma_buf_get_size2(ictx, id, raw_num);	\
		_mempool_pop(bufsize);					\
	} while (0)

#define DMA_SETUP(id)						\
	do {							\
		bufaddr = _mempool_get_addr();			\
		bufsize = ispblk_dma_config(ictx, id, bufaddr); \
		_mempool_pop(bufsize);				\
	} while (0)

#define CV181X_VI_CLK(_id, _name, _gate_reg, _gate_shift)		\
	[(_id)] = {							\
		.id = _id,						\
		.name = _name,						\
		.gate.reg = _gate_reg,					\
		.gate.shift = _gate_shift				\
		}

#define CLK_REG_BASE		0x03002000
#define PORTING_TEST		1
#define VI_SHARE_MEM_SIZE	(0x2000)
#define SEM_WAIT_TIMEOUT_MS	100

#define VI_PROFILE
#define VI_MAX_LIST_NUM		(0x10)
//TODO module_param need replace
/*******************************************************
 *  Global variables
 ******************************************************/
//u32 vi_log_lv = VI_INFO | VI_ERR | VI_WARN | VI_NOTICE;// | VI_DBG;
//module_param(vi_log_lv, int, 0644);
#if PORTING_TEST //test only

int stop_stream_en;
//module_param(stop_stream_en, int, 0644);
#endif

//TODO module_param need replace
/* viproc control for sensor numbers */
static int viproc_en[2] = {1, 0};
//static int count;
//module_param_array(viproc_en, int, &count, 0664);

/* control internal patgen
 *
 * 1: enable
 * 0: disable
 */
static int csi_patgen_en[ISP_PRERAW_MAX] = {0, 0};
//module_param_array(csi_patgen_en, int, &count, 0644);

struct cvi_vi_ctx *gViCtx;
static struct cvi_vi_dev *g_vdev;
static VI_VPSS_MODE_S stVIVPSSMode;
static u8 RGBMAP_BUF_IDX = 2;

//TODO dq_lock
static spinlock_t raw_num_lock;
static spinlock_t dq_lock;
static spinlock_t snr_node_lock[ISP_PRERAW_MAX];
static spinlock_t event_lock;
static spinlock_t stream_lock;

static atomic_t dev_open_cnt;

static struct vi_clk clk_sys[] = {
		CV181X_VI_CLK(0, "clk_sys_0", 0x08, 5),
		CV181X_VI_CLK(1, "clk_sys_1", 0x08, 6),
		CV181X_VI_CLK(2, "clk_sys_2", 0x0C, 29),
		CV181X_VI_CLK(3, "clk_sys_3", 0x10, 15),
	};
static struct vi_clk clk_isp[] = {
		CV181X_VI_CLK(0, "clk_axi", 0x08, 4),
		CV181X_VI_CLK(1, "clk_csi_be", 0x10, 8),
		CV181X_VI_CLK(2, "clk_raw", 0x10, 18),
		CV181X_VI_CLK(3, "clk_isp_top", 0x08, 20),
	};

static struct vi_clk clk_mac[] = {
		CV181X_VI_CLK(0, "clk_csi_mac0", 0x08, 18),
		CV181X_VI_CLK(1, "clk_csi_mac1", 0x08, 19),
		CV181X_VI_CLK(2, "clk_csi_mac2", 0x10, 20),
	};

struct _vi_gdc_cb_param {
	MMF_CHN_S chn;
	enum GDC_USAGE usage;
};
struct cvi_gdc_mesh g_vi_mesh[VI_MAX_CHN_NUM];

/*******************************************************
 *  Internal APIs
 ******************************************************/
//TODO timer
#if 0 //(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
static void legacy_timer_emu_func(struct timer_list *t)
{
	struct legacy_timer_emu *lt = from_timer(lt, t, t);

	lt->function(lt->data);
}
#endif //(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))

/**
 * _mempool_reset - reset the byteused and assigned buffer for each dma
 *
 */
static void _vi_mempool_reset(void)
{
	u8 i = 0;

	isp_mempool.byteused = 0;

	memset(isp_bufpool, 0x0, (sizeof(struct _membuf) * ISP_PRERAW_MAX));

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		spin_lock_init(&isp_bufpool[i].pre_fe_sts_lock);
		spin_lock_init(&isp_bufpool[i].pre_be_sts_lock);
		spin_lock_init(&isp_bufpool[i].post_sts_lock);
	}
}

/**
 * _mempool_get_addr - get mempool's latest address.
 *
 * @return: the latest address of the mempool.
 */
static uint64_t _mempool_get_addr(void)
{
	return isp_mempool.base + isp_mempool.byteused;
}

/**
 * _mempool_pop - acquire a buffer-space from mempool.
 *
 * @param size: the space acquired.
 * @return: negative if no enough space; o/w, the address of the buffer needed.
 */
static int64_t _mempool_pop(uint32_t size)
{
	int64_t addr;

	size = VI_ALIGN(size);

	if ((isp_mempool.byteused + size) > isp_mempool.size) {
		vi_pr(VI_ERR, "reserved_memory(0x%x) is not enough. byteused(0x%x) alloc_size(0x%x)\n",
				isp_mempool.size, isp_mempool.byteused, size);
		return -EINVAL;
	}

	addr = isp_mempool.base + isp_mempool.byteused;
	isp_mempool.byteused += size;

	return addr;
}

void vi_wake_up(struct vi_thread_attr *event)
{
	aos_event_set(&event->event, 0x01, AOS_EVENT_OR);
}

#define tasklet_hi_schedule(v) vi_wake_up(v)

static CVI_VOID vi_gdc_callback(CVI_VOID *pParam, VB_BLK blk)
{
	struct _vi_gdc_cb_param *_pParam = pParam;

	if (!pParam)
		return;

	vi_pr(VI_DBG, "ViChn(%d) usage(%d)\n", _pParam->chn.s32ChnId, _pParam->usage);
	pthread_mutex_unlock(&g_vi_mesh[_pParam->chn.s32ChnId].lock);
	if (blk != VB_INVALID_HANDLE)
		vb_done_handler(_pParam->chn, CHN_TYPE_OUT, blk);
	aos_free(pParam);
}

//TODO for GDC
static CVI_S32 _mesh_gdc_do_op_cb(enum GDC_USAGE usage, const CVI_VOID *pUsageParam,
				struct vb_s *vb_in, PIXEL_FORMAT_E enPixFormat, CVI_U64 mesh_addr,
				CVI_BOOL sync_io, CVI_VOID *pcbParam, CVI_U32 cbParamSize,
				MOD_ID_E enModId, ROTATION_E enRotation)
{
	struct mesh_gdc_cfg cfg;
	struct base_exe_m_cb exe_cb;

	memset(&cfg, 0, sizeof(cfg));
	cfg.usage = usage;
	cfg.pUsageParam = pUsageParam;
	cfg.vb_in = vb_in;
	cfg.enPixFormat = enPixFormat;
	cfg.mesh_addr = mesh_addr;
	cfg.sync_io = sync_io;
	cfg.pcbParam = pcbParam;
	cfg.cbParamSize = cbParamSize;
	cfg.enRotation = enRotation;

	exe_cb.callee = E_MODULE_LDC;
	exe_cb.caller = E_MODULE_VI;
	exe_cb.cmd_id = LDC_CB_MESH_GDC_OP;
	exe_cb.data   = &cfg;
	return base_exe_module_cb(&exe_cb);
}

void _isp_snr_cfg_enq(struct cvi_isp_snr_update *snr_node, const enum cvi_isp_raw raw_num)
{
	unsigned long flags;
	struct _isp_snr_i2c_node *n, *q;

	if (snr_node == NULL)
		return;

	spin_lock_irqsave(&snr_node_lock[raw_num], flags);

	if (snr_node->snr_cfg_node.snsr.need_update) {
		n = malloc(sizeof(*n));
		if (n == NULL) {
			vi_pr(VI_ERR, "SNR cfg node alloc size(%lu) fail\n", sizeof(*n));
			spin_unlock_irqrestore(&snr_node_lock[raw_num], flags);
			return;
		}
		memcpy(&n->n, &snr_node->snr_cfg_node.snsr, sizeof(struct snsr_regs_s));

		while (!list_empty(&isp_snr_i2c_queue[raw_num].list)
			&& (isp_snr_i2c_queue[raw_num].num_rdy >= (VI_MAX_LIST_NUM - 1))) {
			q = list_first_entry(&isp_snr_i2c_queue[raw_num].list, struct _isp_snr_i2c_node, list);
			list_del_init(&q->list);
			--isp_snr_i2c_queue[raw_num].num_rdy;
			free(q);
		}
		list_add_tail(&n->list, &isp_snr_i2c_queue[raw_num].list);
		++isp_snr_i2c_queue[raw_num].num_rdy;
	}

	spin_unlock_irqrestore(&snr_node_lock[raw_num], flags);
}

void pre_raw_num_enq(struct _isp_sof_raw_num_q *q, struct _isp_raw_num_n *n)
{
	unsigned long flags;

	spin_lock_irqsave(&raw_num_lock, flags);
	if (!list_empty(&q->list) && (q->num_rdy >= (VI_MAX_LIST_NUM - 1))) {
		free(n);
	} else {
		list_add_tail(&n->list, &q->list);
		++q->num_rdy;
	}
	spin_unlock_irqrestore(&raw_num_lock, flags);
}

void cvi_isp_buf_queue2(struct cvi_vi_dev *vdev, struct cvi_isp_buf2 *b)
{
	unsigned long flags;

	vi_pr(VI_DBG, "buf_queue chn_id=%d\n", b->buf.index);

	spin_lock_irqsave(&vdev->qbuf_lock, flags);
	list_add_tail(&b->list, &vdev->qbuf_list[b->buf.index]);
	++vdev->qbuf_num[b->buf.index];
	spin_unlock_irqrestore(&vdev->qbuf_lock, flags);
}

struct cvi_isp_buf2 *_cvi_isp_next_buf2(struct cvi_vi_dev *vdev, const u8 chn_num)
{
	unsigned long flags;
	struct cvi_isp_buf2 *b = NULL;

	spin_lock_irqsave(&vdev->qbuf_lock, flags);
	if (!list_empty(&vdev->qbuf_list[chn_num]))
		b = list_first_entry(&vdev->qbuf_list[chn_num], struct cvi_isp_buf2, list);
	spin_unlock_irqrestore(&vdev->qbuf_lock, flags);

	return b;
}

int cvi_isp_rdy_buf_empty2(struct cvi_vi_dev *vdev, const u8 chn_num)
{
	unsigned long flags;
	int empty = 0;

	spin_lock_irqsave(&vdev->qbuf_lock, flags);
	empty = (vdev->qbuf_num[chn_num] == 0);
	spin_unlock_irqrestore(&vdev->qbuf_lock, flags);

	return empty;
}

void cvi_isp_rdy_buf_pop2(struct cvi_vi_dev *vdev, const u8 chn_num)
{
	unsigned long flags;

	spin_lock_irqsave(&vdev->qbuf_lock, flags);
	vdev->qbuf_num[chn_num]--;
	spin_unlock_irqrestore(&vdev->qbuf_lock, flags);
}

void cvi_isp_rdy_buf_remove2(struct cvi_vi_dev *vdev, const u8 chn_num)
{
	unsigned long flags;
	struct cvi_isp_buf2 *b = NULL;

	spin_lock_irqsave(&vdev->qbuf_lock, flags);
	if (!list_empty(&vdev->qbuf_list[chn_num])) {
		b = list_first_entry(&vdev->qbuf_list[chn_num], struct cvi_isp_buf2, list);
		list_del_init(&b->list);
		free(b);
	}
	spin_unlock_irqrestore(&vdev->qbuf_lock, flags);
}

void _vi_yuv_dma_setup(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	struct isp_buffer *b;
	uint32_t bufsize_yuyv = 0;
	uint8_t  i = 0;

	struct _membuf *pool = &isp_bufpool[raw_num];

	u8 total_chn = (raw_num == ISP_PRERAW_A) ?
			ctx->rawb_chnstr_num :
			ctx->total_chn_num;
	u8 chn_str = (raw_num == ISP_PRERAW_A) ? 0 : ctx->rawb_chnstr_num;

	for (; chn_str < total_chn; chn_str++) {
		enum ISP_BLK_ID_T dma;
		if (raw_num == ISP_PRERAW_C) {
			dma = (chn_str == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19;
		} else {
			dma = (chn_str == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13;
		}

		for (i = 0; i < OFFLINE_YUV_BUF_NUM; i++) {
			b = malloc(sizeof(*b));
			if (b == NULL) {
				vi_pr(VI_ERR, "yuv_buf isp_buf_%d malloc size(%lu) fail\n", i, sizeof(*b));
				return;
			}
			memset(b, 0, sizeof(*b));
			b->chn_num = chn_str;
			b->is_yuv_frm = true;
			bufsize_yuyv = ispblk_dma_yuv_bypass_config(ctx, dma, 0, raw_num);
			pool->yuv_yuyv[b->chn_num][i] = b->addr = _mempool_pop(bufsize_yuyv);

			if (i == 0)
				ispblk_dma_setaddr(ctx, dma, b->addr);

			isp_buf_queue(&pre_out_queue[b->chn_num], b);
		}
	}
}

static void _isp_preraw_fe_dma_dump(struct isp_ctx *ictx, enum cvi_isp_raw  raw_num)
{
	u8 i = 0;
	char str[64] = "PRERAW_FE";

	vi_pr(VI_INFO, "***************%s_%d************************\n", str, raw_num);
	for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++)
		vi_pr(VI_INFO, "bayer_le(0x%"PRIx64")\n", isp_bufpool[raw_num].bayer_le[i]);

	for (i = 0; i < RGBMAP_BUF_IDX; i++)
		vi_pr(VI_INFO, "rgbmap_le(0x%"PRIx64")\n", isp_bufpool[raw_num].rgbmap_le[i]);

	if (ictx->isp_pipe_cfg[raw_num].is_hdr_on) {
		for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++)
			vi_pr(VI_INFO, "bayer_se(0x%"PRIx64")\n", isp_bufpool[raw_num].bayer_se[i]);

		for (i = 0; i < RGBMAP_BUF_IDX; i++)
			vi_pr(VI_INFO, "rgbmap_se(0x%"PRIx64")\n", isp_bufpool[raw_num].rgbmap_se[i]);
	}

	if (ictx->isp_pipe_cfg[raw_num].is_yuv_bypass_path &&
		!ictx->isp_pipe_cfg[raw_num].is_offline_scaler) {
		for (i = 0; i < ISP_CHN_MAX; i++) {
			vi_pr(VI_INFO, "yuyv_yuv(0x%"PRIx64"), yuyv_yuv(0x%"PRIx64")\n",
				isp_bufpool[raw_num].yuv_yuyv[i][0], isp_bufpool[raw_num].yuv_yuyv[i][1]);
		}
	}
	vi_pr(VI_INFO, "*************************************************\n");
}

static void _isp_preraw_be_dma_dump(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	u8 i = 0;
	char str[64] = "PRERAW_BE";
	enum cvi_isp_raw raw = ISP_PRERAW_A;

	vi_pr(VI_INFO, "***************%s************************\n", str);
	vi_pr(VI_INFO, "be_rdma_le(0x%"PRIx64")\n", isp_bufpool[raw].bayer_le[0]);
	for (i = 0; i < OFFLINE_PRE_BE_BUF_NUM; i++)
		vi_pr(VI_INFO, "be_wdma_le(0x%"PRIx64")\n", isp_bufpool[raw].prebe_le[i]);

	if (ictx->is_hdr_on) {
		vi_pr(VI_INFO, "be_rdma_se(0x%"PRIx64")\n", isp_bufpool[raw].bayer_se[0]);
		for (i = 0; i < OFFLINE_PRE_BE_BUF_NUM; i++)
			vi_pr(VI_INFO, "be_wdma_se(0x%"PRIx64")\n", isp_bufpool[raw].prebe_se[i]);
	}

	for (; raw < raw_max; raw++) {
		if (raw == ISP_PRERAW_B)
			vi_pr(VI_INFO, "***********************Dual sensor dump**********************\n");

		vi_pr(VI_INFO, "af(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].af.phy_addr,
				isp_bufpool[raw].sts_mem[1].af.phy_addr);
	}
	vi_pr(VI_INFO, "*************************************************\n");
}

static void _isp_rawtop_dma_dump(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	char str[64] = "RAW_TOP";
	enum cvi_isp_raw raw = ISP_PRERAW_A;

	vi_pr(VI_INFO, "***************%s************************\n", str);
	vi_pr(VI_INFO, "rawtop_rdma_le(0x%"PRIx64")\n", isp_bufpool[raw].prebe_le[0]);

	if (ictx->is_hdr_on)
		vi_pr(VI_INFO, "rawtop_rdma_se(0x%"PRIx64")\n", isp_bufpool[raw].prebe_se[0]);

	for (; raw < raw_max; raw++) {
		if (raw == ISP_PRERAW_B)
			vi_pr(VI_INFO, "***********************Dual sensor dump**********************\n");

		vi_pr(VI_INFO, "lsc(0x%"PRIx64")\n", isp_bufpool[raw].lsc);

		vi_pr(VI_INFO, "ae_le(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].ae_le.phy_addr,
				isp_bufpool[raw].sts_mem[1].ae_le.phy_addr);
		vi_pr(VI_INFO, "gms(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].gms.phy_addr,
				isp_bufpool[raw].sts_mem[1].gms.phy_addr);
		vi_pr(VI_INFO, "awb(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].awb.phy_addr,
				isp_bufpool[raw].sts_mem[1].awb.phy_addr);
		vi_pr(VI_INFO, "lmap_le(0x%"PRIx64")\n", isp_bufpool[raw].lmap_le);

		if (ictx->isp_pipe_cfg[raw].is_hdr_on) {
			vi_pr(VI_INFO, "ae_se(0x%llx, 0x%llx)\n",
					isp_bufpool[raw].sts_mem[0].ae_se.phy_addr,
					isp_bufpool[raw].sts_mem[1].ae_se.phy_addr);
			vi_pr(VI_INFO, "lmap_se(0x%"PRIx64")\n", isp_bufpool[raw].lmap_se);
		}
	}

	vi_pr(VI_INFO, "*************************************************\n");
}

static void _isp_rgbtop_dma_dump(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	char str[64] = "RGB_TOP";
	enum cvi_isp_raw raw = ISP_PRERAW_A;

	vi_pr(VI_INFO, "***************%s************************\n", str);
	for (; raw < raw_max; raw++) {
		if (raw == ISP_PRERAW_B)
			vi_pr(VI_INFO, "***********************Dual sensor dump**********************\n");

		vi_pr(VI_INFO, "hist_edge_v(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].hist_edge_v.phy_addr,
				isp_bufpool[raw].sts_mem[1].hist_edge_v.phy_addr);
		vi_pr(VI_INFO, "manr(0x%"PRIx64"), manr_rtile(0x%"PRIx64")\n",
				isp_bufpool[raw].manr,
				isp_bufpool[raw].manr_rtile);
		vi_pr(VI_INFO, "tdnr(0x%"PRIx64", 0x%"PRIx64"), tdnr_rtile(0x%"PRIx64", 0x%"PRIx64")\n",
				isp_bufpool[raw].tdnr[0],
				isp_bufpool[raw].tdnr[1],
				isp_bufpool[raw].tdnr_rtile[0],
				isp_bufpool[raw].tdnr_rtile[1]);
	}

	vi_pr(VI_INFO, "*************************************************\n");
}

static void _isp_yuvtop_dma_dump(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	char str[64] = "YUV_TOP";
	enum cvi_isp_raw raw = ISP_PRERAW_A;

	uint64_t dci_bufaddr = 0;
	uint64_t ldci_bufaddr = 0;

	vi_pr(VI_INFO, "***************%s************************\n", str);
	for (; raw < raw_max; raw++) {
		if (raw == ISP_PRERAW_B)
			vi_pr(VI_INFO, "***********************Dual sensor dump**********************\n");
		vi_pr(VI_INFO, "dci(0x%llx, 0x%llx)\n",
				isp_bufpool[raw].sts_mem[0].dci.phy_addr,
				isp_bufpool[raw].sts_mem[1].dci.phy_addr);
		vi_pr(VI_INFO, "ldci(0x%"PRIx64")\n", isp_bufpool[raw].ldci);

		// show wasted buf size for 256B-aligned ldci bufaddr
		dci_bufaddr = isp_bufpool[raw].sts_mem[1].dci.phy_addr;
		ldci_bufaddr = isp_bufpool[raw].ldci;
		vi_pr(VI_INFO, "ldci wasted_bufsize_for_alignment(%d)\n",
			(uint32_t)(ldci_bufaddr - (dci_bufaddr + 0x200)));
	}
	vi_pr(VI_INFO, "*************************************************\n");
	vi_pr(VI_INFO, "VI total reserved memory(0x%x)\n", isp_mempool.byteused);
	vi_pr(VI_INFO, "*************************************************\n");
}

void _isp_preraw_fe_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw  raw_num)
{
	uint64_t bufaddr = 0;
	uint32_t bufsize = 0;
	uint8_t  i = 0;
	struct isp_buffer *b;

	u32 raw_le, raw_se;
	u32 rgbmap_le, rgbmap_se = 0;

	if (ictx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //YUV sensor
		if (!ictx->isp_pipe_cfg[raw_num].is_offline_scaler) //Online mode to scaler
			_vi_yuv_dma_setup(ictx, raw_num);

		goto EXIT;
	}

	if (raw_num == ISP_PRERAW_A) {
		raw_le = ISP_BLK_ID_DMA_CTL6;
		raw_se = ISP_BLK_ID_DMA_CTL7;
		rgbmap_le = ISP_BLK_ID_DMA_CTL10;
		rgbmap_se = ISP_BLK_ID_DMA_CTL11;
	} else if (raw_num == ISP_PRERAW_B) {
		raw_le = ISP_BLK_ID_DMA_CTL12;
		raw_se = ISP_BLK_ID_DMA_CTL13;
		rgbmap_le = ISP_BLK_ID_DMA_CTL16;
		rgbmap_se = ISP_BLK_ID_DMA_CTL17;
	} else {
		raw_le = ISP_BLK_ID_DMA_CTL18;
		raw_se = ISP_BLK_ID_DMA_CTL19;
		rgbmap_le = ISP_BLK_ID_DMA_CTL20;
		//No se in ISP_PRERAW_C
	}

	if (_is_be_post_online(ictx) && !ictx->isp_pipe_cfg[raw_num].is_offline_preraw) { //fe->dram->be->post
		for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++) {
			DMA_SETUP_2(raw_le, raw_num);
			b = malloc(sizeof(*b));
			if (b == NULL) {
				vi_pr(VI_ERR, "raw_le isp_buf_%d malloc size(%lu) fail\n", i, sizeof(*b));
				return;
			}
			memset(b, 0, sizeof(*b));
			b->addr = bufaddr;
			b->raw_num = raw_num;
			b->ir_idx = i;
			isp_bufpool[raw_num].bayer_le[i] = b->addr;
			isp_buf_queue(&pre_out_queue[b->raw_num], b);
		}
		ispblk_dma_setaddr(ictx, raw_le, isp_bufpool[raw_num].bayer_le[0]);

		if (ictx->isp_pipe_cfg[raw_num].is_hdr_on) {
			for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++) {
				DMA_SETUP_2(raw_se, raw_num);
				b = malloc(sizeof(*b));
				if (b == NULL) {
					vi_pr(VI_ERR, "raw_se isp_buf_%d malloc size(%lu) fail\n", i, sizeof(*b));
					return;
				}
				memset(b, 0, sizeof(*b));
				b->addr = bufaddr;
				b->raw_num = raw_num;
				b->ir_idx = i;
				isp_bufpool[raw_num].bayer_se[i] = b->addr;
				isp_buf_queue(&pre_out_se_queue[b->raw_num], b);
			}
			ispblk_dma_setaddr(ictx, raw_se, isp_bufpool[raw_num].bayer_se[0]);
		}
	}

	if (ictx->is_3dnr_on) {
		// rgbmap_le
		DMA_SETUP_2(rgbmap_le, raw_num);
		isp_bufpool[raw_num].rgbmap_le[0] = bufaddr;
		ispblk_rgbmap_dma_config(ictx, raw_num, rgbmap_le);

		//Slice buffer mode use ring buffer
		if (!(_is_fe_be_online(ictx) && ictx->is_slice_buf_on)) {
			if (!_is_all_online(ictx)) {
				for (i = 1; i < RGBMAP_BUF_IDX; i++)
					isp_bufpool[raw_num].rgbmap_le[i] = _mempool_pop(bufsize);
			}
		}

		if (ictx->isp_pipe_cfg[raw_num].is_hdr_on) {
			// rgbmap se
			DMA_SETUP_2(rgbmap_se, raw_num);
			isp_bufpool[raw_num].rgbmap_se[0] = bufaddr;
			ispblk_rgbmap_dma_config(ictx, raw_num, rgbmap_se);

			//Slice buffer mode use ring buffer
			if (!(_is_fe_be_online(ictx) && ictx->is_slice_buf_on)) {
				for (i = 1; i < RGBMAP_BUF_IDX; i++)
					isp_bufpool[raw_num].rgbmap_se[i] = _mempool_pop(bufsize);
			}
		}
	}

EXIT:
	_isp_preraw_fe_dma_dump(ictx, raw_num);
}

void _isp_preraw_be_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint64_t bufaddr = 0;
	uint32_t bufsize = 0;
	uint8_t  buf_num = 0;
	struct isp_buffer *b;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false)
		goto EXIT;

	if (ictx->is_offline_be && !ictx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
		//apply pre_fe_le buffer
		ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL4, isp_bufpool[ISP_PRERAW_A].bayer_le[0]);

		if (ictx->is_hdr_on) {
			//apply pre_fe_se buffer
			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL5, isp_bufpool[ISP_PRERAW_A].bayer_se[0]);
		}
	}

	if (_is_fe_be_online(ictx)) { //fe->be->dram->post
		if (ictx->is_slice_buf_on) {
			DMA_SETUP_2(ISP_BLK_ID_DMA_CTL22, raw);
			isp_bufpool[raw].prebe_le[0] = bufaddr;

			if (ictx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on) {
				DMA_SETUP_2(ISP_BLK_ID_DMA_CTL23, raw);
				isp_bufpool[raw].prebe_se[0] = bufaddr;
			}
		} else {
			for (buf_num = 0; buf_num < OFFLINE_PRE_BE_BUF_NUM; buf_num++) {
				DMA_SETUP_2(ISP_BLK_ID_DMA_CTL22, raw);
				b = malloc(sizeof(*b));
				if (b == NULL) {
					vi_pr(VI_ERR, "be_wdma_le isp_buf_%d malloc size(%lu) fail\n",
						buf_num, sizeof(*b));
					return;
				}
				memset(b, 0, sizeof(*b));
				b->addr = bufaddr;
				b->raw_num = raw;
				b->ir_idx = buf_num;
				isp_bufpool[raw].prebe_le[buf_num] = b->addr;
				isp_buf_queue(&pre_be_out_q, b);
			}

			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL22, isp_bufpool[raw].prebe_le[0]);

			if (ictx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on) {
				for (buf_num = 0; buf_num < OFFLINE_PRE_BE_BUF_NUM; buf_num++) {
					DMA_SETUP_2(ISP_BLK_ID_DMA_CTL23, raw);
					b = malloc(sizeof(*b));
					if (b == NULL) {
						vi_pr(VI_ERR, "be_wdma_se isp_buf_%d malloc size(%lu) fail\n",
								buf_num, sizeof(*b));
						return;
					}
					memset(b, 0, sizeof(*b));
					b->addr = bufaddr;
					b->raw_num = raw;
					b->ir_idx = buf_num;
					isp_bufpool[raw].prebe_se[buf_num] = b->addr;
					isp_buf_queue(&pre_be_out_se_q, b);
				}

				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL23, isp_bufpool[raw].prebe_se[0]);
			}
		}
	}

	for (; raw < raw_max; raw++) {
		//Be out dma

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path)
			continue;

		// af
		DMA_SETUP(ISP_BLK_ID_DMA_CTL21);
		isp_bufpool[raw].sts_mem[0].af.phy_addr = bufaddr;
		isp_bufpool[raw].sts_mem[0].af.size = bufsize;
		isp_bufpool[raw].sts_mem[1].af.phy_addr = _mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[1].af.size = bufsize;
	}
EXIT:
	_isp_preraw_be_dma_dump(ictx, raw_max);
}

void _isp_rawtop_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint64_t bufaddr = 0;
	uint32_t bufsize = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	if (_is_fe_be_online(ictx)) { //fe->be->dram->post
		//apply pre_be le buffer from DMA_CTL22
		if (ictx->is_slice_buf_on)
			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL28, isp_bufpool[ISP_PRERAW_A].prebe_le[0]);
		else
			ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL28, isp_bufpool[ISP_PRERAW_A].prebe_le[0]);

		if (ictx->is_hdr_on) {
			//apply pre_be se buffer from DMA_CTL23
			if (ictx->is_slice_buf_on)
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL29, isp_bufpool[ISP_PRERAW_A].prebe_se[0]);
			else
				ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL29, isp_bufpool[ISP_PRERAW_A].prebe_se[0]);
		}
	}

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// lsc
		DMA_SETUP(ISP_BLK_ID_DMA_CTL24);
		isp_bufpool[raw].lsc = bufaddr;

		// gms
		bufaddr = _mempool_get_addr();
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL25, bufaddr);
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL25, raw);
		_mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[0].gms.phy_addr = bufaddr;
		isp_bufpool[raw].sts_mem[0].gms.size = bufsize;
		isp_bufpool[raw].sts_mem[1].gms.phy_addr = _mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[1].gms.size = bufsize;

		// lmap_le
		DMA_SETUP_2(ISP_BLK_ID_DMA_CTL30, raw);
		isp_bufpool[raw].lmap_le = bufaddr;

		// ae le
		DMA_SETUP(ISP_BLK_ID_DMA_CTL26);
		isp_bufpool[raw].sts_mem[0].ae_le.phy_addr = bufaddr;
		isp_bufpool[raw].sts_mem[0].ae_le.size = bufsize;
		isp_bufpool[raw].sts_mem[1].ae_le.phy_addr = _mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[1].ae_le.size = bufsize;

		if (ictx->isp_pipe_cfg[raw].is_hdr_on) {
			// lmap_se
			DMA_SETUP_2(ISP_BLK_ID_DMA_CTL31, raw);
			isp_bufpool[raw].lmap_se = bufaddr;

			// ae se
			DMA_SETUP(ISP_BLK_ID_DMA_CTL27);
			isp_bufpool[raw].sts_mem[0].ae_se.phy_addr = bufaddr;
			isp_bufpool[raw].sts_mem[0].ae_se.size = bufsize;
			isp_bufpool[raw].sts_mem[1].ae_se.phy_addr = _mempool_pop(bufsize);
			isp_bufpool[raw].sts_mem[1].ae_se.size = bufsize;
		}
	}

EXIT:
	_isp_rawtop_dma_dump(ictx, raw_max);
}

void _isp_rgbtop_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint64_t bufaddr = 0;
	uint32_t bufsize = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// hist_edge_v
		DMA_SETUP_2(ISP_BLK_ID_DMA_CTL38, raw);
		isp_bufpool[raw].sts_mem[0].hist_edge_v.phy_addr = bufaddr;
		isp_bufpool[raw].sts_mem[0].hist_edge_v.size = bufsize;
		isp_bufpool[raw].sts_mem[1].hist_edge_v.phy_addr = _mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[1].hist_edge_v.size = bufsize;

		// manr
		if (ictx->is_3dnr_on) {
			DMA_SETUP_2(ISP_BLK_ID_DMA_CTL36, raw);
			isp_bufpool[raw].manr = bufaddr;
			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL37, isp_bufpool[raw].manr);

			isp_bufpool[raw].sts_mem[0].mmap.phy_addr =
				isp_bufpool[raw].sts_mem[1].mmap.phy_addr = bufaddr;
			isp_bufpool[raw].sts_mem[0].mmap.size =
				isp_bufpool[raw].sts_mem[1].mmap.size = bufsize;

			if (_is_all_online(ictx)) {
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL32, isp_bufpool[raw].rgbmap_le[0]);
			} else {
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL34, isp_bufpool[raw].rgbmap_le[0]);
				if (_is_fe_be_online(ictx) && ictx->is_slice_buf_on)
					ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL32, isp_bufpool[raw].rgbmap_le[0]);
				else
					ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL32, isp_bufpool[raw].rgbmap_le[1]);
			}

			if (ictx->isp_pipe_cfg[raw].is_hdr_on) {
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL35, isp_bufpool[raw].rgbmap_se[0]);
				if (_is_fe_be_online(ictx) && ictx->is_slice_buf_on)
					ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL33, isp_bufpool[raw].rgbmap_se[0]);
				else
					ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL33, isp_bufpool[raw].rgbmap_se[1]);
			}

			// TNR
			if (ictx->is_fbc_on) {
				u64 bufaddr_tmp = 0;

				bufaddr_tmp = _mempool_get_addr();
				//ring buffer constraint. reg_base is 256 byte-align
				bufaddr = VI_256_ALIGN(bufaddr_tmp);
				ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL44, bufaddr);
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL44, raw);
				_mempool_pop(bufsize + (u32)(bufaddr - bufaddr_tmp));

				ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL42, bufaddr);
				isp_bufpool[raw].tdnr[0] = bufaddr;

				bufaddr_tmp = _mempool_get_addr();
				//ring buffer constraint. reg_base is 256 byte-align
				bufaddr = VI_256_ALIGN(bufaddr_tmp);
				ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL43, bufaddr);
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL43, raw);
				_mempool_pop(bufsize + (u32)(bufaddr - bufaddr_tmp));

				ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL41, bufaddr);
				isp_bufpool[raw].tdnr[1] = bufaddr;
			} else {
				DMA_SETUP_2(ISP_BLK_ID_DMA_CTL44, raw);
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL42, bufaddr);
				isp_bufpool[raw].tdnr[0] = bufaddr;

				DMA_SETUP_2(ISP_BLK_ID_DMA_CTL43, raw);
				ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL41, bufaddr);
				isp_bufpool[raw].tdnr[1] = bufaddr;
			}
		}

		// ltm dma
		ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL39, isp_bufpool[raw].lmap_le);

		if (ictx->isp_pipe_cfg[raw].is_hdr_on && !ictx->isp_pipe_cfg[raw].is_hdr_detail_en)
			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL40, isp_bufpool[raw].lmap_se);
		else
			ispblk_dma_setaddr(ictx, ISP_BLK_ID_DMA_CTL40, isp_bufpool[raw].lmap_le);
	}
EXIT:
	_isp_rgbtop_dma_dump(ictx, raw_max);
}

void _isp_yuvtop_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint64_t bufaddr = 0;
	uint64_t tmp_bufaddr = 0;
	uint32_t bufsize = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// dci
		//DMA_SETUP(ISP_BLK_ID_WDMA27);
		DMA_SETUP(ISP_BLK_ID_DMA_CTL45);
		isp_bufpool[raw].sts_mem[0].dci.phy_addr = bufaddr;
		isp_bufpool[raw].sts_mem[0].dci.size = bufsize;
		isp_bufpool[raw].sts_mem[1].dci.phy_addr = _mempool_pop(bufsize);
		isp_bufpool[raw].sts_mem[1].dci.size = bufsize;

		// ldci
		//DMA_SETUP(ISP_BLK_ID_DMA_CTL48);
		//addr 256B alignment workaround
		tmp_bufaddr = _mempool_get_addr();
		bufaddr = VI_256_ALIGN(tmp_bufaddr);
		bufsize = ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL48, bufaddr);
		_mempool_pop(bufsize + (uint32_t)(bufaddr - tmp_bufaddr));

		isp_bufpool[raw].ldci = bufaddr;
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA_CTL49, isp_bufpool[raw].ldci);
	}

	if (cfg_dma) {
		if (ictx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_scaler ||
			(ictx->is_multi_sensor && ictx->isp_pipe_cfg[ISP_PRERAW_B].is_offline_scaler)) {
			//SW workaround. Need to set y/uv dma_disable = 1 before csibdg enable
			if (_is_be_post_online(ictx) && !ictx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
				ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL46, 1, 1);
				ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL47, 1, 1);
			} else {
				ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL46, 1, 0);
				ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL47, 1, 0);
			}
		} else {
			ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL46, 0, 0);
			ispblk_dma_enable(ictx, ISP_BLK_ID_DMA_CTL47, 0, 0);
		}
	}

EXIT:
	_isp_yuvtop_dma_dump(ictx, raw_max);
}

void _vi_dma_setup(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;

	for (raw_num = ISP_PRERAW_A; raw_num < raw_max; raw_num++)
		_isp_preraw_fe_dma_setup(ictx, raw_num);

	_isp_preraw_be_dma_setup(ictx, raw_max);
	_isp_rawtop_dma_setup(ictx, raw_max);
	_isp_rgbtop_dma_setup(ictx, raw_max);
	_isp_yuvtop_dma_setup(ictx, raw_max);
}

void _vi_dma_set_sw_mode(struct isp_ctx *ctx)
{
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL4, false); //be_le_rdma_ctl
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL5, false); //be_se_rdma_ctl
	//ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL10, false); //fe0 rgbmap LE
	//ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL11, false); //fe0 rgbmap SE
	ispblk_rgbmap_dma_mode(ctx, ISP_BLK_ID_DMA_CTL10); //fe0 rgbmap LE
	ispblk_rgbmap_dma_mode(ctx, ISP_BLK_ID_DMA_CTL11); //fe0 rgbmap SE
	//ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL16, false); //fe1 rgbmap LE
	//ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL17, false); //fe1 rgbmap SE
	ispblk_rgbmap_dma_mode(ctx, ISP_BLK_ID_DMA_CTL16); //fe1 rgbmap LE
	ispblk_rgbmap_dma_mode(ctx, ISP_BLK_ID_DMA_CTL17); //fe1 rgbmap SE
	//ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL20, false); //fe2 rgbmap LE
	ispblk_rgbmap_dma_mode(ctx, ISP_BLK_ID_DMA_CTL20); //fe2 rgbmap LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL22, false); //be_le_wdma_ctl
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL23, false); //be_se_wdma_ctl
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL30, false); //lmap LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL31, false); //lmap SE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL6, false); //fe0 csi0
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL7, false); //fe0 csi1
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL8, false); //fe0 csi2/fe1 ch0
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL9, false); //fe0 csi3/fe1 ch1
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL12, false);
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL13, false);
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL18, false); //fe2 ch0
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL19, false); //fe2 ch1
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL21, true); //af
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL24, true); //lsc
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL25, true); //gms
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL26, true); //aehist0
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL27, true); //aehist1
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL28, ctx->is_slice_buf_on ? false : true); //raw crop LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL29, ctx->is_slice_buf_on ? false : true); //raw crop SE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL38, false); //hist_v
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL46, false); //yuv crop y
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL47, false); //yuv crop uv
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL32, false); //MANR_P_LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL33, false); //MANR_P_SE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL34, false); //MANR_C_LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL35, false); //MANR_C_SE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL36, false); //MANR_IIR_R
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL37, false); //MANR_IIR_W
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL41, false); //TNR_Y_R
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL42, false); //TNR_C_R
	if (ctx->is_fbc_on) {
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL43, true); //TNR_Y_W
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL44, true); //TNR_C_W
	} else {
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL43, false); //TNR_Y_W
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL44, false); //TNR_C_W
	}
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL45, true); //dci
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL39, false); //LTM LE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL40, false); //LTM SE
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL48, true); //ldci_iir_w
	ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL49, true); //ldci_iir_r
}

void _vi_yuv_get_dma_size(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	uint32_t bufsize_yuyv = 0;
	uint8_t  i = 0;

	u8 total_chn = (raw_num == ISP_PRERAW_A) ?
			ctx->rawb_chnstr_num :
			ctx->total_chn_num;
	u8 chn_str = (raw_num == ISP_PRERAW_A) ? 0 : ctx->rawb_chnstr_num;

	for (; chn_str < total_chn; chn_str++) {
		enum ISP_BLK_ID_T dma;
		if (raw_num == ISP_PRERAW_C) {
			dma = (chn_str == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19;
		} else {
			dma = (chn_str == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13;
		}

		for (i = 0; i < OFFLINE_YUV_BUF_NUM; i++) {
			bufsize_yuyv = ispblk_dma_yuv_bypass_config(ctx, dma, 0, raw_num);
			_mempool_pop(bufsize_yuyv);
		}
	}
}

void _vi_pre_fe_get_dma_size(struct isp_ctx *ictx, enum cvi_isp_raw  raw_num)
{
	uint32_t bufsize = 0;
	uint8_t  i = 0;
	u32 raw_le, raw_se;
	u32 rgbmap_le, rgbmap_se = 0;

	if (raw_num == ISP_PRERAW_A) {
		raw_le = ISP_BLK_ID_DMA_CTL6;
		raw_se = ISP_BLK_ID_DMA_CTL7;
		rgbmap_le = ISP_BLK_ID_DMA_CTL10;
		rgbmap_se = ISP_BLK_ID_DMA_CTL11;
	} else if (raw_num == ISP_PRERAW_B) {
		raw_le = ISP_BLK_ID_DMA_CTL12;
		raw_se = ISP_BLK_ID_DMA_CTL13;
		rgbmap_le = ISP_BLK_ID_DMA_CTL16;
		rgbmap_se = ISP_BLK_ID_DMA_CTL17;
	} else {
		raw_le = ISP_BLK_ID_DMA_CTL18;
		raw_se = ISP_BLK_ID_DMA_CTL19;
		rgbmap_le = ISP_BLK_ID_DMA_CTL20;
		//No se in ISP_PRERAW_C
	}

	if (ictx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //YUV sensor
		if (!ictx->isp_pipe_cfg[raw_num].is_offline_scaler) //Online mode to scaler
			_vi_yuv_get_dma_size(ictx, raw_num);

		goto EXIT;
	}

	if (_is_be_post_online(ictx)) { //fe->dram->be->post
		for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++) {
			bufsize = ispblk_dma_buf_get_size2(ictx, raw_le, raw_num);
			_mempool_pop(bufsize);
		}

		if (ictx->isp_pipe_cfg[raw_num].is_hdr_on) {
			for (i = 0; i < OFFLINE_RAW_BUF_NUM; i++) {
				bufsize = ispblk_dma_buf_get_size2(ictx, raw_se, raw_num);
				_mempool_pop(bufsize);
			}
		}
	}

	// rgbmap le
	bufsize = ispblk_dma_buf_get_size2(ictx, rgbmap_le, raw_num);
	_mempool_pop(bufsize);

	//Slice buffer mode use ring buffer
	if (!(_is_fe_be_online(ictx) && ictx->is_slice_buf_on)) {
		if (!_is_all_online(ictx)) {
			for (i = 1; i < RGBMAP_BUF_IDX; i++)
				_mempool_pop(bufsize);
		}
	}

	if (ictx->isp_pipe_cfg[raw_num].is_hdr_on) {
		// rgbmap se
		bufsize = ispblk_dma_buf_get_size2(ictx, rgbmap_se, raw_num);
		_mempool_pop(bufsize);

		//Slice buffer mode use ring buffer
		if (!(_is_fe_be_online(ictx) && ictx->is_slice_buf_on)) {
			for (i = 1; i < RGBMAP_BUF_IDX; i++)
				_mempool_pop(bufsize);
		}
	}
EXIT:
	return;
}

void _vi_pre_be_get_dma_size(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint32_t bufsize = 0;
	uint8_t  buf_num = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false)
		goto EXIT;

	if (_is_fe_be_online(ictx)) { //fe->be->dram->post
		if (ictx->is_slice_buf_on) {
			bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL22, raw);
			_mempool_pop(bufsize);

			if (ictx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on) {
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL23, raw);
				_mempool_pop(bufsize);
			}
		} else {
			for (buf_num = 0; buf_num < OFFLINE_PRE_BE_BUF_NUM; buf_num++) {
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL22, raw);
				_mempool_pop(bufsize);
			}

			if (ictx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on) {
				for (buf_num = 0; buf_num < OFFLINE_PRE_BE_BUF_NUM; buf_num++) {
					bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL23, raw);
					_mempool_pop(bufsize);
				}
			}
		}
	}

	for (; raw < raw_max; raw++) {
		//Be out dma

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path)
			continue;

		// af
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL21, raw);
		_mempool_pop(bufsize);
		_mempool_pop(bufsize);
	}
EXIT:
	return;
}

void _vi_rawtop_get_dma_size(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint32_t bufsize = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// lsc
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL24, raw);
		_mempool_pop(bufsize);

		// gms
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL25, raw);
		_mempool_pop(bufsize);
		_mempool_pop(bufsize);

		// lmap_le
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL30, raw);
		_mempool_pop(bufsize);

		// ae le
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL26, raw);
		_mempool_pop(bufsize);
		_mempool_pop(bufsize);

		if (ictx->isp_pipe_cfg[raw].is_hdr_on) {
			// lmap_se
			bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL31, raw);
			_mempool_pop(bufsize);

			// ae se
			bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL27, raw);
			_mempool_pop(bufsize);
			_mempool_pop(bufsize);
		}
	}
EXIT:
	return;
}

void _vi_rgbtop_get_dma_size(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint32_t bufsize = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// hist_edge_v
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL38, raw);
		_mempool_pop(bufsize);
		_mempool_pop(bufsize);

		// manr
		if (ictx->is_3dnr_on) {
			uint64_t bufaddr = 0;
			uint64_t tmp_bufaddr = 0;

			// MANR M + H
			bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL36, raw);
			_mempool_pop(bufsize);

			if (ictx->is_fbc_on) {
				// TNR UV
				tmp_bufaddr = _mempool_get_addr();
				//ring buffer constraint. reg_base is 256 byte-align
				bufaddr = VI_256_ALIGN(tmp_bufaddr);

				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL44, raw);
				_mempool_pop(bufsize + (uint32_t)(bufaddr - tmp_bufaddr));

				// TNR Y
				tmp_bufaddr = _mempool_get_addr();
				//ring buffer constraint. reg_base is 256 byte-align
				bufaddr = VI_256_ALIGN(tmp_bufaddr);
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL43, raw);
				_mempool_pop(bufsize + (uint32_t)(bufaddr - tmp_bufaddr));
			} else {
				// TNR UV
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL44, raw);
				_mempool_pop(bufsize);

				// TNR Y
				bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL43, raw);
				_mempool_pop(bufsize);
			}
		}
	}
EXIT:
	return;
}

void _vi_yuvtop_get_dma_size(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	uint32_t bufsize = 0;
	uint64_t bufaddr = 0;
	uint64_t tmp_bufaddr = 0;

	enum cvi_isp_raw raw = ISP_PRERAW_A;

	u8 cfg_dma = false;

	//RGB path
	if (!ictx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_dma = true;
	} else if (ictx->is_multi_sensor && !ictx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_dma = true;
	}

	if (cfg_dma == false) //YUV sensor only
		goto EXIT;

	for (raw = ISP_PRERAW_A; raw < raw_max; raw++) {

		if (ictx->isp_pipe_cfg[raw].is_yuv_bypass_path) //YUV sensor
			continue;

		// dci
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL45, raw);
		_mempool_pop(bufsize);
		tmp_bufaddr = _mempool_pop(bufsize);

		// ldci
		bufaddr = VI_256_ALIGN(tmp_bufaddr);
		bufsize = ispblk_dma_buf_get_size2(ictx, ISP_BLK_ID_DMA_CTL48, raw);
		_mempool_pop(bufsize + (uint32_t)(bufaddr - tmp_bufaddr));

		vi_pr(VI_INFO, "ldci bufsize: total(%d), used(%d), wasted_for_alignment(%d)\n",
			bufsize + (uint32_t)(bufaddr - tmp_bufaddr),
			bufsize,
			(uint32_t)(bufaddr - tmp_bufaddr));
	}
EXIT:
	return;
}

void _vi_get_dma_buf_size(struct isp_ctx *ictx, enum cvi_isp_raw raw_max)
{
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;

	for (raw_num = ISP_PRERAW_A; raw_num < raw_max; raw_num++)
		_vi_pre_fe_get_dma_size(ictx, raw_num);

	_vi_pre_be_get_dma_size(ictx, raw_max);
	_vi_rawtop_get_dma_size(ictx, raw_max);
	_vi_rgbtop_get_dma_size(ictx, raw_max);
	_vi_yuvtop_get_dma_size(ictx, raw_max);
}

static void _vi_preraw_be_init(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint32_t bps[40] = {((0 << 12) | 0), ((0 << 12) | 20), ((0 << 12) | 40), ((0 << 12) | 60)};
	u8 cfg_be = false;

	if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_be = true;
	} else if (ctx->is_multi_sensor && !ctx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_be = true;
	}

	if (cfg_be) { //RGB sensor
		// preraw_vi_sel
		ispblk_preraw_vi_sel_config(ctx);
		// preraw_be_top
		ispblk_preraw_be_config(ctx, ISP_PRERAW_A);
		// preraw_be wdma ctrl
		ispblk_pre_wdma_ctrl_config(ctx, ISP_PRERAW_A);

		//ispblk_blc_set_offset(ctx, ISP_BLC_ID_BE_LE, 511, 511, 511, 511);
		ispblk_blc_set_gain(ctx, ISP_BLC_ID_BE_LE, 0x40f, 0x419, 0x419, 0x405);
		//ispblk_blc_set_2ndoffset(ctx, ISP_BLC_ID_BE_LE, 511, 511, 511, 511);
		ispblk_blc_enable(ctx, ISP_BLC_ID_BE_LE, false, false);

		if (ctx->is_hdr_on) {
			ispblk_blc_set_offset(ctx, ISP_BLC_ID_BE_SE, 511, 511, 511, 511);
			ispblk_blc_set_gain(ctx, ISP_BLC_ID_BE_SE, 0x800, 0x800, 0x800, 0x800);
			ispblk_blc_set_2ndoffset(ctx, ISP_BLC_ID_BE_SE, 511, 511, 511, 511);
			ispblk_blc_enable(ctx, ISP_BLC_ID_BE_SE, false, false);
		}

		ispblk_dpc_set_static(ctx, ISP_RAW_PATH_LE, 0, bps, 4);
		ispblk_dpc_config(ctx, ISP_RAW_PATH_LE, false, 0);

		if (ctx->is_hdr_on)
			ispblk_dpc_config(ctx, ISP_RAW_PATH_SE, true, 0);
		else {
			ispblk_dpc_config(ctx, ISP_RAW_PATH_SE, false, 0);
		}

		ispblk_af_config(ctx, true);

		if (_is_fe_be_online(ctx))
			ispblk_slice_buf_config(&vdev->ctx, ISP_PRERAW_A, vdev->ctx.is_slice_buf_on);
		else
			ispblk_slice_buf_config(&vdev->ctx, ISP_PRERAW_A, false);
	}
}

static void _isp_rawtop_init(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ictx = &vdev->ctx;

	// raw_top
	ispblk_rawtop_config(ictx, ISP_PRERAW_A);
	// raw_rdma ctrl
	ispblk_raw_rdma_ctrl_config(ictx, ISP_PRERAW_A);

	ispblk_bnr_config(ictx, ISP_BNR_OUT_B_DELAY, false, 0, 0);

	ispblk_lsc_config(ictx, false);

	ispblk_cfa_config(ictx);
	ispblk_rgbcac_config(ictx, true);
	ispblk_lcac_config(ictx, false, 0);
	ispblk_gms_config(ictx, true);

	ispblk_wbg_config(ictx, ISP_WBG_ID_RAW_TOP_LE, 0x400, 0x400, 0x400);
	ispblk_wbg_enable(ictx, ISP_WBG_ID_RAW_TOP_LE, false, false);

	ispblk_lmap_config(ictx, ISP_BLK_ID_LMAP0, true);

	ispblk_aehist_config(ictx, ISP_BLK_ID_AEHIST0, true);

	if (ictx->is_hdr_on) {
		ispblk_wbg_config(ictx, ISP_WBG_ID_RAW_TOP_SE, 0x400, 0x400, 0x400);
		ispblk_wbg_enable(ictx, ISP_WBG_ID_RAW_TOP_SE, false, false);
		ispblk_lmap_config(ictx, ISP_BLK_ID_LMAP1, true);
		ispblk_aehist_config(ictx, ISP_BLK_ID_AEHIST1, true);
	} else {
		ispblk_lmap_config(ictx, ISP_BLK_ID_LMAP1, false);
		ispblk_aehist_config(ictx, ISP_BLK_ID_AEHIST1, false);
	}
}

static void _isp_rgbtop_init(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ictx = &vdev->ctx;

	ispblk_rgbtop_config(ictx, ISP_PRERAW_A);

	ispblk_hist_v_config(ictx, true, 0);

	//ispblk_awb_config(ictx, ISP_BLK_ID_AWB2, true, ISP_AWB_LE);

	ispblk_ccm_config(ictx, ISP_BLK_ID_CCM0, false, &ccm_hw_cfg);
	ispblk_dhz_config(ictx, false);

	ispblk_ygamma_config(ictx, false, ictx->gamma_tbl_idx, ygamma_data, 0, 0);
	ispblk_ygamma_enable(ictx, false);

	ispblk_gamma_config(ictx, false, ictx->gamma_tbl_idx, gamma_data, 0);
	ispblk_gamma_enable(ictx, false);

	//ispblk_clut_config(ictx, false, c_lut_r_lut, c_lut_g_lut, c_lut_b_lut);
	ispblk_rgbdither_config(ictx, false, false, false, false);
	ispblk_csc_config(ictx);

	ispblk_manr_config(ictx, ictx->is_3dnr_on);

	if (ictx->is_hdr_on) {
		ispblk_fusion_config(ictx, true, true, ISP_FS_OUT_FS);

		ispblk_ltm_b_lut(ictx, 0, ltm_b_lut);
		ispblk_ltm_d_lut(ictx, 0, ltm_d_lut);
		ispblk_ltm_g_lut(ictx, 0, ltm_g_lut);
		ispblk_ltm_config(ictx, true, true, true, true);
	} else {
		ispblk_fusion_config(ictx, false, false, ISP_FS_OUT_LONG);
		ispblk_ltm_config(ictx, false, false, false, false);
	}
}

static void _isp_yuvtop_init(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ictx = &vdev->ctx;

	ispblk_yuvtop_config(ictx, ISP_PRERAW_A);

	ispblk_yuvdither_config(ictx, 0, false, true, true, true);
	ispblk_yuvdither_config(ictx, 1, false, true, true, true);

	ispblk_tnr_config(ictx, ictx->is_3dnr_on, 0);
	if (ictx->is_3dnr_on && ictx->is_fbc_on) {
		ispblk_fbce_config(ictx, true);
		ispblk_fbcd_config(ictx, true);
		ispblk_fbc_ring_buf_config(ictx, true);
	} else {
		ispblk_fbce_config(ictx, false);
		ispblk_fbcd_config(ictx, false);
		ispblk_fbc_ring_buf_config(ictx, false);
	}

	ispblk_ynr_config(ictx, ISP_YNR_OUT_Y_DELAY, 128);
	ispblk_cnr_config(ictx, false, false, 255, 0);
	ispblk_pre_ee_config(ictx, true);
	ispblk_ee_config(ictx, false);
	ispblk_ycur_config(ictx, false, 0, ycur_data);
	ispblk_ycur_enable(ictx, false, 0);

	ispblk_dci_config(ictx, true, ictx->gamma_tbl_idx, dci_map_lut_50, 0);
	ispblk_ldci_config(ictx, false, 0);

	ispblk_ca_config(ictx, false);
	ispblk_ca_lite_config(ictx, false);

	ispblk_crop_enable(ictx, ISP_BLK_ID_CROP4, false);
	ispblk_crop_enable(ictx, ISP_BLK_ID_CROP5, false);
}

static u32 _is_drop_next_frame(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const enum cvi_isp_pre_chn_num chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint32_t start_drop_num = ctx->isp_pipe_cfg[raw_num].drop_ref_frm_num;
	uint32_t end_drop_num = start_drop_num + ctx->isp_pipe_cfg[raw_num].drop_frm_cnt;
	u32 frm_num = 0;

	if (ctx->isp_pipe_cfg[raw_num].is_drop_next_frame) {
		//for tuning_dis, shoudn't trigger preraw;
		if ((ctx->is_multi_sensor) && (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path)) {
			if ((tuning_dis[0] > 0) && ((tuning_dis[0] - 1) != raw_num)) {
				vi_pr(VI_DBG, "input buf is not equal to current tuning number\n");
				return 1;
			}
		}

		//if sof_num in [start_sof, end_sof), shoudn't trigger preraw;
		frm_num = vdev->pre_fe_sof_cnt[raw_num][ISP_FE_CH0];

		if ((start_drop_num != 0) && (frm_num >= start_drop_num) && (frm_num < end_drop_num))
			return 1;
	}

	return 0;
}

static void _set_drop_frm_info(
	const struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	struct isp_i2c_data *i2c_data)
{
	struct isp_ctx *ctx = (struct isp_ctx *)(&vdev->ctx);

	ctx->isp_pipe_cfg[raw_num].drop_frm_cnt = i2c_data->drop_frame_cnt;

	ctx->isp_pipe_cfg[raw_num].is_drop_next_frame = true;
	ctx->isp_pipe_cfg[raw_num].drop_ref_frm_num = vdev->pre_fe_sof_cnt[raw_num][ISP_FE_CH0];

	vi_pr(VI_DBG, "raw_%d, drop_ref_frm_num=%d, drop frame=%d\n", raw_num,
				ctx->isp_pipe_cfg[raw_num].drop_ref_frm_num,
				i2c_data->drop_frame_cnt);
}

static void _clear_drop_frm_info(
	const struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = (struct isp_ctx *)(&vdev->ctx);

	ctx->isp_pipe_cfg[raw_num].drop_frm_cnt = 0;
	ctx->isp_pipe_cfg[raw_num].drop_ref_frm_num = 0;
	ctx->isp_pipe_cfg[raw_num].is_drop_next_frame = false;
}


static void _snr_i2c_update(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	struct _isp_snr_i2c_node **_i2c_n,
	const u16 _i2c_num)
{
	struct isp_ctx *ctx = (struct isp_ctx *)(&vdev->ctx);
	struct _isp_snr_i2c_node *node;
	struct _isp_snr_i2c_node *next_node;
	struct isp_i2c_data *i2c_data;
	struct isp_i2c_data *next_i2c_data;
	unsigned long flags;
	u16 i = 0, j = 0;
	// 0: Delete Node, 1: Postpone Node, 2: Do nothing
	u16 del_node = 0;
	uint32_t dev_mask = 0;
	uint32_t cmd = burst_i2c_en ? CVI_SNS_I2C_BURST_QUEUE : CVI_SNS_I2C_WRITE;
	CVI_BOOL fe_frm_equal = CVI_FALSE;
	u32 fe_frm_num = 0;

	if (vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw || vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en)
		return;

	//dual sensor case, fe/be frm_num wouldn't be the same when dump frame of rotation.
	if (_is_be_post_online(&vdev->ctx)) {
		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
			fe_frm_equal = (vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] ==
					vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1]) ? CVI_TRUE : CVI_FALSE;
		fe_frm_num = vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] - vdev->dump_frame_number[raw_num];
	} else {
		fe_frm_num = vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0];
	}

	vi_pr(VI_DBG, "raw_num=%d, fe_frm_num=%d, fe_frm_equal=%d\n",
			raw_num, fe_frm_num, fe_frm_equal);

	for (j = 0; j < _i2c_num; j++) {
		node = _i2c_n[j];

		vi_pr(VI_DBG, "raw_num=%d, i2c_num=%d, j=%d, magic_num=%d, fe_frm_num=%d, drop=%d,dump=%d\n",
				raw_num, _i2c_num, j, node->n.magic_num, fe_frm_num,
				vdev->drop_frame_number[raw_num], vdev->dump_frame_number[raw_num]);

		//magic num set by ISP team. fire i2c when magic num same as last fe frm num.
		if (((ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) &&
		     ((node->n.magic_num == fe_frm_num && fe_frm_equal == CVI_TRUE) ||
		      (node->n.magic_num_dly == fe_frm_num))) ||
		    (!(ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) &&
		     ((node->n.magic_num == fe_frm_num) ||
		      (node->n.magic_num < fe_frm_num && (j + 1) >= _i2c_num)))) {

			if (node->n.magic_num != fe_frm_num) {
				vi_pr(VI_WARN, "raw_%d exception handle, send delayed i2c data.\n", raw_num);
			}

			for (i = 0; i < node->n.regs_num; i++) {
				i2c_data = &node->n.i2c_data[i];

				vi_pr(VI_DBG, "i2c_data[%d]:addr=0x%x write:0x%x update:%d dly_frm:%d\n",
						i, i2c_data->reg_addr, i2c_data->data,
						i2c_data->update, i2c_data->dly_frm_num);

				if (i2c_data->update && (i2c_data->dly_frm_num == 0)) {
					vip_sys_cmm_cb_i2c(cmd, (void *)i2c_data);
					i2c_data->update = 0;
					if (burst_i2c_en)
						dev_mask |= BIT(i2c_data->i2c_dev);

					if (i2c_data->drop_frame)
						_set_drop_frm_info(vdev, raw_num, i2c_data);
				} else if (i2c_data->update && !(i2c_data->dly_frm_num == 0)) {
					i2c_data->dly_frm_num--;
					del_node = 1;
				}
			}
		} else if (node->n.magic_num < fe_frm_num) {
			if ((j + 1) < _i2c_num) {
				next_node = _i2c_n[j + 1];
				for (i = 0; i < next_node->n.regs_num; i++) {
					next_i2c_data = &next_node->n.i2c_data[i];
					i2c_data = &node->n.i2c_data[i];

					if (i2c_data->update && next_i2c_data->update == 0) {
						next_i2c_data->update = i2c_data->update;
						vi_pr(VI_WARN, "exception handle, i2c node merge, addr: 0x%x\n",
							i2c_data->reg_addr);
					}
				}

				del_node = 0;
			} else {
				// impossible case
			}
		} else {
			del_node = 2;
		}

		if (del_node == 0) {
			vi_pr(VI_DBG, "i2c del node and free\n");
			spin_lock_irqsave(&snr_node_lock[raw_num], flags);
			list_del_init(&node->list);
			--isp_snr_i2c_queue[raw_num].num_rdy;
			free(node);
			spin_unlock_irqrestore(&snr_node_lock[raw_num], flags);
		} else if (del_node == 1) {
			node->n.magic_num++;
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
				node->n.magic_num_dly = node->n.magic_num;
			vi_pr(VI_DBG, "postpone i2c node\n");
		}
	}

	while (dev_mask) {
		uint32_t tmp = ffs(dev_mask) - 1;

		vip_sys_cmm_cb_i2c(CVI_SNS_I2C_BURST_FIRE, (void *)&tmp);
		dev_mask &= ~BIT(tmp);
	}
}

static void _isp_snr_cfg_deq_and_fire(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num)
{
	struct list_head *pos, *temp;
	struct _isp_snr_i2c_node *i2c_n[VI_MAX_LIST_NUM];
	unsigned long flags;
	u16 i2c_num = 0;

	spin_lock_irqsave(&snr_node_lock[raw_num], flags);

	list_for_each_safe(pos, temp, &isp_snr_i2c_queue[raw_num].list) {
		i2c_n[i2c_num] = list_entry(pos, struct _isp_snr_i2c_node, list);
		i2c_num++;
	}

	spin_unlock_irqrestore(&snr_node_lock[raw_num], flags);

	_snr_i2c_update(vdev, raw_num, i2c_n, i2c_num);
}

static inline void _vi_clear_mmap_fbc_ring_base(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	//Clear mmap previous ring base to start addr after first frame done.
	if (ctx->is_3dnr_on && (ctx->isp_pipe_cfg[raw_num].first_frm_cnt == 1)) {
		manr_clear_prv_ring_base(ctx, raw_num);

		if (ctx->is_fbc_on) {
			ispblk_fbc_chg_to_sw_mode(&vdev->ctx, raw_num);
			ispblk_fbc_clear_fbcd_ring_base(&vdev->ctx, raw_num);
		}
	}
}

static void _usr_pic_timer_handler(void *timer, void *arg)
{
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)usr_pic_timer.data;
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;

	if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
		aos_timer_start(&usr_pic_timer.t);
		return;
	}

	if (atomic_read(&vdev->pre_be_state[ISP_BE_CH0]) == ISP_PRE_BE_IDLE &&
		(atomic_read(&vdev->isp_streamoff) == 0) && ctx->is_ctrl_inited) {
		struct _isp_raw_num_n  *n;

		if (_is_fe_be_online(ctx) && ctx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on) {
			struct isp_buffer *b = NULL;

			b = isp_next_buf(&pre_be_out_se_q);
			if (!b) {
				vi_pr(VI_DBG, "pre_be chn_num_%d outbuf is empty\n", ISP_FE_CH1);
				return;
			}

			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL23, b->addr);
		}

		if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1 && _is_fe_be_online(ctx)) {//raw_dump flow
			_isp_fe_be_raw_dump_cfg(vdev, raw_num, 0);
			atomic_set(&vdev->isp_raw_dump_en[raw_num], 3);
		}

		_vi_clear_mmap_fbc_ring_base(vdev, raw_num);

		vi_tuning_gamma_ips_update(ctx, raw_num);
		vi_tuning_clut_update(ctx, raw_num);
		vi_tuning_dci_update(ctx, raw_num);

		_post_rgbmap_update(ctx, raw_num, vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);

		_pre_hw_enque(vdev, raw_num, ISP_BE_CH0);

		n = malloc(sizeof(*n));
		if (n == NULL) {
			vi_pr(VI_ERR, "pre_raw_num_q kmalloc size(%d) fail\n", sizeof(*n));
			return;
		}
		n->raw_num = raw_num;
		pre_raw_num_enq(&pre_raw_num_q, n);

		vdev->vi_th[E_VI_TH_PRERAW].flag = raw_num + 1;
		vi_wake_up(&vdev->vi_th[E_VI_TH_PRERAW]);

		//if (!_is_all_online(ctx)) //Not on the fly mode
		//	tasklet_hi_schedule(&vdev->job_work);
	}

	aos_timer_start(&usr_pic_timer.t);
}

void usr_pic_time_remove(void)
{
	if (aos_timer_is_valid(&usr_pic_timer.t)) {
		aos_timer_stop(&usr_pic_timer.t);
		aos_timer_free(&usr_pic_timer.t);
	}
}

int usr_pic_timer_init(struct cvi_vi_dev *vdev)
{
	aos_status_t status;

	usr_pic_time_remove();
	usr_pic_timer.function = _usr_pic_timer_handler;
	usr_pic_timer.data = (uintptr_t)vdev;
	status = aos_timer_new(&usr_pic_timer.t, _usr_pic_timer_handler, NULL,
				vdev->usr_pic_delay, AOS_TIMER_NONE);
	if (status != 0) {
		vi_pr(VI_ERR, "create timer error\n");
		return status;
	}
	return status;
}

void vi_event_queue(struct cvi_vi_dev *vdev, const u32 type, const u32 frm_num)
{
	unsigned long flags;
	struct vi_event_k *ev_k;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	if (type >= VI_EVENT_MAX) {
		vi_pr(VI_ERR, "event queue type(%d) error\n", type);
		return;
	}

	ev_k = calloc(sizeof(*ev_k), 1);
	if (ev_k == NULL) {
		vi_pr(VI_ERR, "event queue kzalloc size(%lu) fail\n", sizeof(*ev_k));
		return;
	}

	spin_lock_irqsave(&event_lock, flags);
	ev_k->ev.type = type;
	ev_k->ev.frame_sequence = frm_num;
	ev_k->ev.timestamp = ts;

	list_add_tail(&ev_k->list, &event_q.list);
	spin_unlock_irqrestore(&event_lock, flags);

	vi_wake_up(&vdev->isp_event_wait_q);
}

void cvi_isp_dqbuf_list(struct cvi_vi_dev *vdev, const u32 frm_num, const u8 chn_id)
{
	unsigned long flags;
	struct _isp_dqbuf_n *n;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	n = calloc(sizeof(struct _isp_dqbuf_n), 1);
	if (n == NULL) {
		vi_pr(VI_ERR, "DQbuf kmalloc size(%lu) fail\n", sizeof(struct _isp_dqbuf_n));
		return;
	}
	n->chn_id	= chn_id;
	n->frm_num	= frm_num;
	n->timestamp	= ts;

	spin_lock_irqsave(&dq_lock, flags);
	list_add_tail(&n->list, &dqbuf_q.list);
	spin_unlock_irqrestore(&dq_lock, flags);
}

int vi_dqbuf(struct _vi_buffer *b)
{
	unsigned long flags;
	struct _isp_dqbuf_n *n = NULL;
	int ret = -1;

	spin_lock_irqsave(&dq_lock, flags);
	if (!list_empty(&dqbuf_q.list)) {
		n = list_first_entry(&dqbuf_q.list, struct _isp_dqbuf_n, list);
		b->chnId	= n->chn_id;
		b->sequence	= n->frm_num;
		b->timestamp	= n->timestamp;
		list_del_init(&n->list);
		free(n);
		ret = 0;
	}
	spin_unlock_irqrestore(&dq_lock, flags);

	return ret;
}

static int _vi_call_cb(u32 m_id, u32 cmd_id, void *data)
{
	struct base_exe_m_cb exe_cb;

	exe_cb.callee = m_id;
	exe_cb.caller = E_MODULE_VI;
	exe_cb.cmd_id = cmd_id;
	exe_cb.data   = (void *)data;

	return base_exe_module_cb(&exe_cb);
}

static void vi_init(void)
{
	int i, j;

	for (i = 0; i < VI_MAX_CHN_NUM; ++i) {
		gViCtx->enRotation[i] = ROTATION_0;
		gViCtx->stLDCAttr[i].bEnable = CVI_FALSE;
		pthread_mutex_init(&g_vi_mesh[i].lock, NULL);
		vi_pr(VI_DBG, "mesh mutex init %p\n", &g_vi_mesh[i]);
	}

	for (i = 0; i < VI_MAX_CHN_NUM; ++i)
		for (j = 0; j < VI_MAX_EXTCHN_BIND_PER_CHN; ++j)
			gViCtx->chn_bind[i][j] = VI_INVALID_CHN;
}

static void vi_deinit(void)
{
	int i;

	for (i = 0; i < VI_MAX_CHN_NUM; ++i) {
		gViCtx->enRotation[i] = ROTATION_0;
		gViCtx->stLDCAttr[i].bEnable = CVI_FALSE;
		pthread_mutex_destroy(&g_vi_mesh[i].lock);
		vi_pr(VI_DBG, "mesh mutex deinit %p\n", &g_vi_mesh[i]);
	}
}

static void _isp_yuv_bypass_buf_enq(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 buf_chn)
{
	struct isp_ctx *ictx = &vdev->ctx;
	struct cvi_isp_buf2 *b = NULL;
	enum ISP_BLK_ID_T dmaid;
	u64 tmp_addr = 0, i = 0;
	u8 hw_chn_num = (raw_num == ISP_PRERAW_A) ? buf_chn : (buf_chn - vdev->ctx.rawb_chnstr_num);

	cvi_isp_rdy_buf_pop2(vdev, buf_chn);
	b = _cvi_isp_next_buf2(vdev, buf_chn);
	if (b == NULL)
		return;

	vi_pr(VI_DBG, "update yuv bypass outbuf: 0x%llx raw_%d chn_num(%d)\n",
			b->buf.planes[0].addr, raw_num, buf_chn);
	if (raw_num == ISP_PRERAW_A) {
		switch (buf_chn) {
		case 0:
			dmaid = ISP_BLK_ID_DMA_CTL6;
			break;
		case 1:
			dmaid = ISP_BLK_ID_DMA_CTL7;
			break;
		case 2:
			dmaid = ISP_BLK_ID_DMA_CTL8;
			break;
		case 3:
			dmaid = ISP_BLK_ID_DMA_CTL9;
			break;
		default:
			vi_pr(VI_ERR, "PRERAW_A Wrong chn_num(%d)\n", buf_chn);
			return;
		}
	} else if (raw_num == ISP_PRERAW_B) {
		switch (hw_chn_num) {
		case 0:
			//ToDo yuv sensor flow
			dmaid = ISP_BLK_ID_DMA_CTL12;
			break;
		case 1:
			//ToDo yuv sensor flow
			dmaid = ISP_BLK_ID_DMA_CTL13;
			break;
		default:
			vi_pr(VI_ERR, "RAW_%c Wrong chn_num(%d), rawb_chnstr_num(%d)\n",
					(raw_num + 'A'), buf_chn, ictx->rawb_chnstr_num);
			return;
		}
	} else {
		switch (hw_chn_num) {
		case 0:
			dmaid = ISP_BLK_ID_DMA_CTL18;
			break;
		case 1:
			dmaid = ISP_BLK_ID_DMA_CTL19;
			break;
		default:
			vi_pr(VI_ERR, "RAW_%c Wrong chn_num(%d), rawb_chnstr_num(%d)\n",
					(raw_num + 'A'), buf_chn, ictx->rawb_chnstr_num);
			return;
		}
	}

	if (ictx->isp_pipe_cfg[raw_num].is_422_to_420) {
		for (i = 0; i < 2; i++) {
			tmp_addr = b->buf.planes[i].addr;
			if (vdev->pre_fe_frm_num[raw_num][hw_chn_num] == 0)
				ispblk_dma_yuv_bypass_config(ictx, dmaid + i, tmp_addr, raw_num);
			else
				ispblk_dma_setaddr(ictx, dmaid + i, tmp_addr);
		}
	} else {
		tmp_addr = b->buf.planes[0].addr;

		if (vdev->pre_fe_frm_num[raw_num][hw_chn_num] == 0)
			ispblk_dma_yuv_bypass_config(ictx, dmaid, tmp_addr, raw_num);
		else
			ispblk_dma_setaddr(ictx, dmaid, tmp_addr);
	}
}

static void _isp_yuv_bypass_trigger(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 hw_chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	u8 buf_chn;

	if (atomic_read(&vdev->isp_err_handle_flag) == 1)
		return;

	if (atomic_read(&vdev->isp_streamoff) == 0) {
		if (atomic_cmpxchg(&vdev->pre_fe_state[raw_num][hw_chn_num],
					ISP_PRERAW_IDLE, ISP_PRERAW_RUNNING) ==
					ISP_PRERAW_RUNNING) {
			vi_pr(VI_DBG, "fe_%d chn_num_%d is running\n", raw_num, hw_chn_num);
			return;
		}
		buf_chn = (raw_num == ISP_PRERAW_A) ? hw_chn_num : vdev->ctx.rawb_chnstr_num + hw_chn_num;

		_isp_yuv_bypass_buf_enq(vdev, raw_num, buf_chn);
		isp_pre_trig(ctx, raw_num, hw_chn_num);
	}
}

void _vi_postraw_ctrl_setup(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	u8 cfg_post = false;

	if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
		cfg_post = true;
	} else if (ctx->is_multi_sensor && !ctx->isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) {
		cfg_post = true;
	}

	if (cfg_post) { //RGB sensor
		_isp_rawtop_init(vdev);
		_isp_rgbtop_init(vdev);
		_isp_yuvtop_init(vdev);
	}

	ispblk_isptop_config(ctx);
}

void _vi_pre_fe_ctrl_setup(enum cvi_isp_raw raw_num, struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ictx = &vdev->ctx;
	u32 blc_le_id, blc_se_id = 0, wbg_le_id, wbg_se_id = 0;
//	struct cif_yuv_swap_s swap = {0};

	if (ictx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {//YUV sensor
		if (ictx->isp_pipe_cfg[raw_num].is_422_to_420) {//uyvy to yuyv to 420
//			swap.devno = raw_num;
//			swap.yc_swap = 1;
//			swap.uv_swap = 1;
//			_vi_call_cb(E_MODULE_CIF, CVI_MIPI_SET_YUV_SWAP, &swap);
		}
		ispblk_csibdg_yuv_bypass_config(ictx, raw_num);

		if (ictx->isp_pipe_cfg[raw_num].is_offline_scaler) { //vi vpss offline mode
			u8 total_chn = (raw_num == ISP_PRERAW_A) ?
					ictx->rawb_chnstr_num :
					ictx->total_chn_num;
			u8 chn_str = (raw_num == ISP_PRERAW_A) ? 0 : ictx->rawb_chnstr_num;

			for (; chn_str < total_chn; chn_str++)
				_isp_yuv_bypass_buf_enq(vdev, raw_num, chn_str);
		}
	} else { //RGB sensor
		if (raw_num == ISP_PRERAW_A) {
			blc_le_id = ISP_BLC_ID_FE0_LE;
			blc_se_id = ISP_BLC_ID_FE0_SE;
			wbg_le_id = ISP_WBG_ID_FE0_RGBMAP_LE;
			wbg_se_id = ISP_WBG_ID_FE0_RGBMAP_SE;
		} else if (raw_num == ISP_PRERAW_B) {
			blc_le_id = ISP_BLC_ID_FE1_LE;
			blc_se_id = ISP_BLC_ID_FE1_SE;
			wbg_le_id = ISP_WBG_ID_FE1_RGBMAP_LE;
			wbg_se_id = ISP_WBG_ID_FE1_RGBMAP_SE;
		} else {
			blc_le_id = ISP_BLC_ID_FE2_LE;
			wbg_le_id = ISP_WBG_ID_FE2_RGBMAP_LE;
		}

		ispblk_preraw_fe_config(ictx, raw_num);
		ispblk_csibdg_config(ictx, raw_num);
		ispblk_csibdg_crop_update(ictx, raw_num, true);

		ispblk_blc_set_gain(ictx, blc_le_id, 0x40f, 0x419, 0x419, 0x405);
		ispblk_blc_enable(ictx, blc_le_id, false, false);

		ispblk_wbg_config(ictx, wbg_le_id, 0x400, 0x400, 0x400);
		ispblk_wbg_enable(ictx, wbg_le_id, false, false);

		ispblk_rgbmap_config(ictx, ISP_BLK_ID_RGBMAP0, ictx->is_3dnr_on, raw_num);

		if (ictx->isp_pipe_cfg[raw_num].is_hdr_on && !ictx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) {
			ispblk_blc_set_gain(ictx, blc_se_id, 0x40f, 0x419, 0x419, 0x405);
			ispblk_blc_enable(ictx, blc_se_id, false, false);

			ispblk_wbg_config(ictx, wbg_se_id, 0x400, 0x400, 0x400);
			ispblk_wbg_enable(ictx, wbg_se_id, false, false);

			ispblk_rgbmap_config(ictx, ISP_BLK_ID_RGBMAP1, ictx->is_3dnr_on, raw_num);
		} else {
			if (raw_num <= ISP_PRERAW_B) {
				ispblk_rgbmap_config(ictx, ISP_BLK_ID_RGBMAP1, false, raw_num);
			}
		}
	}
}

void _vi_ctrl_init(enum cvi_isp_raw raw_num, struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ictx = &vdev->ctx;

	if (ictx->is_ctrl_inited)
		return;

	if (vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w != 0) { //MW config snr_info flow
		ictx->isp_pipe_cfg[raw_num].csibdg_width = vdev->snr_info[raw_num].snr_fmt.img_size[0].width;
		ictx->isp_pipe_cfg[raw_num].csibdg_height = vdev->snr_info[raw_num].snr_fmt.img_size[0].height;
		ictx->isp_pipe_cfg[raw_num].max_width =
						vdev->snr_info[raw_num].snr_fmt.img_size[0].max_width;
		ictx->isp_pipe_cfg[raw_num].max_height =
						vdev->snr_info[raw_num].snr_fmt.img_size[0].max_height;

		ictx->isp_pipe_cfg[raw_num].crop.w = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w;
		ictx->isp_pipe_cfg[raw_num].crop.h = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_h;
		ictx->isp_pipe_cfg[raw_num].crop.x = vdev->snr_info[raw_num].snr_fmt.img_size[0].start_x;
		ictx->isp_pipe_cfg[raw_num].crop.y = vdev->snr_info[raw_num].snr_fmt.img_size[0].start_y;

		if (vdev->snr_info[raw_num].snr_fmt.frm_num > 1) { //HDR
			ictx->isp_pipe_cfg[raw_num].crop_se.w =
							vdev->snr_info[raw_num].snr_fmt.img_size[1].active_w;
			ictx->isp_pipe_cfg[raw_num].crop_se.h =
							vdev->snr_info[raw_num].snr_fmt.img_size[1].active_h;
			ictx->isp_pipe_cfg[raw_num].crop_se.x =
							vdev->snr_info[raw_num].snr_fmt.img_size[1].start_x;
			ictx->isp_pipe_cfg[raw_num].crop_se.y =
							vdev->snr_info[raw_num].snr_fmt.img_size[1].start_y;

			ictx->isp_pipe_cfg[raw_num].is_hdr_on = true;

			vdev->ctx.is_hdr_on = true;
		}

		ictx->rgb_color_mode[raw_num] = vdev->snr_info[raw_num].color_mode;

		if ((ictx->rgb_color_mode[raw_num] == ISP_BAYER_TYPE_BGRGI) ||
			(ictx->rgb_color_mode[raw_num] == ISP_BAYER_TYPE_RGBGI)) {
			ictx->is_rgbir_sensor = true;
			ictx->isp_pipe_cfg[raw_num].is_rgbir_sensor = true;
		}
	}

	if (ictx->isp_pipe_cfg[raw_num].is_patgen_en) {
		ictx->isp_pipe_cfg[raw_num].crop.w = vdev->usr_crop.width;
		ictx->isp_pipe_cfg[raw_num].crop.h = vdev->usr_crop.height;
		ictx->isp_pipe_cfg[raw_num].crop.x = vdev->usr_crop.left;
		ictx->isp_pipe_cfg[raw_num].crop.y = vdev->usr_crop.top;
		ictx->isp_pipe_cfg[raw_num].crop_se.w = vdev->usr_crop.width;
		ictx->isp_pipe_cfg[raw_num].crop_se.h = vdev->usr_crop.height;
		ictx->isp_pipe_cfg[raw_num].crop_se.x = vdev->usr_crop.left;
		ictx->isp_pipe_cfg[raw_num].crop_se.y = vdev->usr_crop.top;

		ictx->isp_pipe_cfg[raw_num].csibdg_width	= vdev->usr_fmt.width;
		ictx->isp_pipe_cfg[raw_num].csibdg_height	= vdev->usr_fmt.height;
		ictx->isp_pipe_cfg[raw_num].max_width		= vdev->usr_fmt.width;
		ictx->isp_pipe_cfg[raw_num].max_height		= vdev->usr_fmt.height;

		ictx->rgb_color_mode[raw_num] = vdev->usr_fmt.code;

		vi_pr(VI_INFO, "patgen csibdg_w_h(%d:%d)\n",
			ictx->isp_pipe_cfg[raw_num].csibdg_width, ictx->isp_pipe_cfg[raw_num].csibdg_height);
	} else if (ictx->isp_pipe_cfg[raw_num].is_offline_preraw) {
		ictx->isp_pipe_cfg[raw_num].crop.w = vdev->usr_crop.width;
		ictx->isp_pipe_cfg[raw_num].crop.h = vdev->usr_crop.height;
		ictx->isp_pipe_cfg[raw_num].crop.x = vdev->usr_crop.left;
		ictx->isp_pipe_cfg[raw_num].crop.y = vdev->usr_crop.top;
		ictx->isp_pipe_cfg[raw_num].crop_se.w = vdev->usr_crop.width;
		ictx->isp_pipe_cfg[raw_num].crop_se.h = vdev->usr_crop.height;
		ictx->isp_pipe_cfg[raw_num].crop_se.x = vdev->usr_crop.left;
		ictx->isp_pipe_cfg[raw_num].crop_se.y = vdev->usr_crop.top;

		ictx->isp_pipe_cfg[raw_num].csibdg_width	= vdev->usr_fmt.width;
		ictx->isp_pipe_cfg[raw_num].csibdg_height	= vdev->usr_fmt.height;
		ictx->isp_pipe_cfg[raw_num].max_width		= vdev->usr_fmt.width;
		ictx->isp_pipe_cfg[raw_num].max_height		= vdev->usr_fmt.height;

		ictx->rgb_color_mode[raw_num] = vdev->usr_fmt.code;

		vi_pr(VI_INFO, "csi_bdg=%d:%d, post_crop=%d:%d:%d:%d\n",
				vdev->usr_fmt.width, vdev->usr_fmt.height,
				vdev->usr_crop.width, vdev->usr_crop.height,
				vdev->usr_crop.left, vdev->usr_crop.top);
	}

	ictx->isp_pipe_cfg[raw_num].post_img_w = ictx->isp_pipe_cfg[raw_num].crop.w;
	ictx->isp_pipe_cfg[raw_num].post_img_h = ictx->isp_pipe_cfg[raw_num].crop.h;

	/* use csibdg crop */
	ictx->crop_x = 0;
	ictx->crop_y = 0;
	ictx->crop_se_x = 0;
	ictx->crop_se_y = 0;

	if (!ictx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {
		//Postraw out size
		ictx->img_width = ictx->isp_pipe_cfg[ISP_PRERAW_A].crop.w;
		ictx->img_height = ictx->isp_pipe_cfg[ISP_PRERAW_A].crop.h;
	}

	if (raw_num == ISP_PRERAW_A) {
		if (_is_fe_be_online(ictx) && ictx->is_slice_buf_on)
			vi_calculate_slice_buf_setting(ictx, raw_num);

		isp_init(ictx);
	}

	if (raw_num == gViCtx->total_dev_num - 1)
		ictx->is_ctrl_inited = true;

}

void _vi_scene_ctrl(struct cvi_vi_dev *vdev, enum cvi_isp_raw *raw_max)
{
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;

	if (ctx->is_ctrl_inited) {
		// vi_scene_ctrl had been inited before.
		*raw_max = gViCtx->total_dev_num;

		return;
	}
#ifndef FPGA_PORTING
	*raw_max = gViCtx->total_dev_num;
	if (gViCtx->total_dev_num >= 2) { // multi sensor scenario
		// for multi sensor, fe->dram->be->post
		ctx->is_multi_sensor = true;
		ctx->is_offline_be = true;
		ctx->is_offline_postraw = false;
		ctx->is_slice_buf_on = false;
		// fbc support rgb + yuv only, default false;
		ctx->is_fbc_on = false;
		RGBMAP_BUF_IDX = 3;

		// only check yuv path
		// rgb + yuv sensor use 2 rgbmap
		// if chn format nv21, enable yuv path 422to420
		for (raw_num = ISP_PRERAW_A; raw_num < *raw_max; raw_num++) {
			if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path)
				continue;
			if (ctx->isp_pipe_cfg[raw_num].muxMode == VI_WORK_MODE_1Multiplex &&
				gViCtx->chnAttr[raw_num].enPixelFormat == PIXEL_FORMAT_NV21)
				ctx->isp_pipe_cfg[raw_num].is_422_to_420 = true;
			// it's means two senor, has yuv sensor offline2sc
			if (*raw_max == ISP_PRERAW_MAX - 1 && ctx->isp_pipe_cfg[raw_num].is_offline_scaler)
				RGBMAP_BUF_IDX = 2;
		}
	} else { // single sensor scenario
		ctx->is_multi_sensor = false;
		ctx->is_fbc_on = true;
		if (ctx->is_offline_be || ctx->is_offline_postraw) {
			ctx->is_offline_be = false;
			ctx->is_offline_postraw = true;
		}

		if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
			ctx->is_offline_be = true;
			ctx->is_offline_postraw = false;
			RGBMAP_BUF_IDX = 3;
			ctx->rgbmap_prebuf_idx = 0;
			ctx->is_slice_buf_on = false;
		} else {
			//Only single sensor with non-tile can use two rgbmap buf
			RGBMAP_BUF_IDX = 2;
			ctx->rgbmap_prebuf_idx = 1;
			if (gViCtx->devAttr[ISP_PRERAW_A].fbmEnable == 1) {
				ctx->is_slice_buf_on = false;
			} else {
				ctx->is_slice_buf_on = true;
			}
		}

		if (!ctx->is_offline_be && !ctx->is_offline_postraw) { //onthefly
			ctx->is_slice_buf_on = false;
		}

		//Currently don't support single yuv sensor online to scaler or on-the-fly
		if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) {
			ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_scaler = true;
			ctx->isp_pipe_cfg[ISP_PRERAW_A].is_422_to_420 = false;
			ctx->is_slice_buf_on = false;
		}

		//scenario must be fe->dram->be->post(synthetic mode)
		if (ctx->is_synthetic_hdr_on) {
			ctx->is_offline_be = true;
			ctx->is_offline_postraw = false;
			ctx->is_slice_buf_on = false;
			RGBMAP_BUF_IDX = 3;
		}
	}
#endif
	//if (rgbmap_sbm_en && ctx->is_slice_buf_on)
	//	ctx->is_rgbmap_sbm_on = true;
	//sbm on, rgbmapsbm off. update rgbmap after postrawdone
	//if (ctx->is_slice_buf_on && !ctx->is_rgbmap_sbm_on)
	//	ctx->rgbmap_prebuf_idx = 0;
	if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) //RGB sensor
		ctx->rawb_chnstr_num = 1;
	else if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) //YUV sensor
		ctx->rawb_chnstr_num = ctx->isp_pipe_cfg[ISP_PRERAW_A].muxMode + 1;


	if (ctx->is_multi_sensor) {
		for (raw_num = ISP_PRERAW_B; raw_num < gViCtx->total_dev_num - 1; raw_num++) {
			if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //RGB sensor
				ctx->rawb_chnstr_num++;
			else if (ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //YUV sensor
				ctx->rawb_chnstr_num += ctx->isp_pipe_cfg[raw_num].muxMode + 1;
		}
	}

	for (raw_num = ISP_PRERAW_A; raw_num < *raw_max; raw_num++) {
		if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //RGB sensor
			ctx->total_chn_num++;
		else //YUV sensor
			ctx->total_chn_num += ctx->isp_pipe_cfg[raw_num].muxMode + 1;
	}

	if (ctx->total_chn_num > 4) {
		vi_pr(VI_ERR, "[ERR] Total chn_num(%d) is wrong\n", ctx->total_chn_num);
		vi_pr(VI_ERR, "[ERR] raw_A,infMode(%d),muxMode(%d)\n",
				ctx->isp_pipe_cfg[ISP_PRERAW_A].infMode, ctx->isp_pipe_cfg[ISP_PRERAW_A].muxMode);
		if (ctx->is_multi_sensor) {
			vi_pr(VI_ERR, "[ERR] raw_B,infMode(%d),muxMode(%d)\n",
				ctx->isp_pipe_cfg[ISP_PRERAW_B].infMode, ctx->isp_pipe_cfg[ISP_PRERAW_B].muxMode);
		}
	}

	vi_pr(VI_INFO, "Total_chn_num=%d, rawb_chnstr_num=%d\n",
			ctx->total_chn_num, ctx->rawb_chnstr_num);
}

static void _vi_suspend(struct cvi_vi_dev *vdev)
{
	struct cvi_vi_ctx *pviProcCtx = NULL;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;

	pviProcCtx = (struct cvi_vi_ctx *)(vdev->shared_mem);

	if (pviProcCtx->vi_stt == VI_SUSPEND) {
		for (raw_num = ISP_PRERAW_A; raw_num < gViCtx->total_dev_num; raw_num++)
			isp_streaming(&vdev->ctx, false, raw_num);

		_vi_sw_init(vdev);
#ifndef FPGA_PORTING
		_vi_clk_ctrl(vdev, false);
#endif
	}
}

static int _vi_resume(struct cvi_vi_dev *vdev)
{
	struct cvi_vi_ctx *pviProcCtx = NULL;

	pviProcCtx = (struct cvi_vi_ctx *)(vdev->shared_mem);

	if (pviProcCtx->vi_stt == VI_SUSPEND) {
		//_vi_mempool_reset();
		//cvi_isp_sw_init(vdev);

		pviProcCtx->vi_stt = VI_RUNNING;

		_vi_clk_ctrl(vdev, true);
	}

	return 0;
}

static void _vi_update_yuvout_config(struct cvi_vi_dev *vdev, enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	bool online2sc = vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num];

	if (!ctx->isp_pipe_cfg[raw_num].is_offline_scaler)
		ispblk_yuvtop_out_config(&vdev->ctx, raw_num, !online2sc);

	//only enable bypass, we need enable dma func and disable dma
	ispblk_dma_enable(&vdev->ctx, ISP_BLK_ID_DMA_CTL46,
				online2sc || ctx->isp_pipe_cfg[raw_num].is_offline_scaler, online2sc);
	ispblk_dma_enable(&vdev->ctx, ISP_BLK_ID_DMA_CTL47,
				online2sc || ctx->isp_pipe_cfg[raw_num].is_offline_scaler, online2sc);

	vi_pr(VI_DBG, "bypass[%d]\n", online2sc);
}

void _viBWCalSet(struct cvi_vi_dev *vdev)
{
#if 0 //ToDo check
	struct isp_ctx *ctx = &vdev->ctx;
	struct cvi_vi_ctx *pviProcCtx = NULL;
	void __iomem *bw_limiter = NULL;
	u32 bwlwin = 0, data_size = 0, BW[2] = {0, 0}, total_bw = 0, bwltxn = 5, margin = 11, fps = 25;
	u32 width = ctx->isp_pipe_cfg[ISP_PRERAW_A].crop.w;
	u32 height = ctx->isp_pipe_cfg[ISP_PRERAW_A].crop.h;
	u8 raw_num = ISP_PRERAW_A;

#define DEFAULT_FPS 25

	pviProcCtx = (struct cvi_vi_ctx *)(vdev->shared_mem);

	fps = (pviProcCtx->devAttr[raw_num].snrFps) ? pviProcCtx->devAttr[raw_num].snrFps : DEFAULT_FPS;

	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
		fps = (fps >> 1);

	if (!ctx->isp_pipe_cfg[raw_num].is_hdr_on)
		data_size = (206 * width * height) / 64 + 30 * height + 49284;
	else
		data_size = (314 * width * height) / 64 + 60 * height + 49284;

	BW[0] = (fps * data_size) / 1000000 + 1;

	if (ctx->is_multi_sensor) {
		raw_num = ISP_PRERAW_B;
		fps = (pviProcCtx->devAttr[raw_num].snrFps) ? pviProcCtx->devAttr[raw_num].snrFps : DEFAULT_FPS;
		width = ctx->isp_pipe_cfg[raw_num].crop.w;
		height = ctx->isp_pipe_cfg[raw_num].crop.h;

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
			fps = (fps >> 1);

		if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {//RGB sensor
			if (!ctx->isp_pipe_cfg[raw_num].is_hdr_on)
				data_size = (206 * width * height) / 64 + 30 * height + 49284;
			else
				data_size = (314 * width * height) / 64 + 60 * height + 49284;
		} else { //YUV sensor
			data_size = (35 * width * height) / 10;
		}

		BW[1] = (fps * data_size) / 1000000 + 1;
	}

	total_bw = BW[0] + BW[1];

	//bwlwin = bwltxn * 256000 / ((((total_bw * 33) / 10) * margin) / 10);

	//ISP rdma bandwidth limiter window size
	bw_limiter = ioremap(0x0A072020, 0x4);
	iowrite32(((bwltxn << 10) | bwlwin), bw_limiter);
	vi_pr(VI_INFO, "isp rdma bw_limiter=0x%x, BW=%d, bwlwin=%d\n",
			ioread32(bw_limiter), total_bw, bwlwin);
	iounmap(bw_limiter);
#endif
}

static void _set_init_state(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	if (ctx->is_multi_sensor) {
		if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {
			if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) {
				atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_RUNNING);
			} else if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on)
				atomic_set(&vdev->pre_be_state[chn_num], ISP_PRE_BE_RUNNING);
			else if (_is_be_post_online(ctx))
				atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_RUNNING);
			else if (_is_all_online(ctx))
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_RUNNING);
		} else {
			if (_is_fe_be_online(ctx) || _is_be_post_online(ctx))
				atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_RUNNING);
		}
	}
}

int vi_start_streaming(struct cvi_vi_dev *vdev)
{
	enum cvi_isp_raw dev_num = ISP_PRERAW_A;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	enum cvi_isp_raw raw_max = ISP_PRERAW_MAX - 1;
	unsigned long flags;
	int rc = 0;
	u8 i;

	vi_pr(VI_DBG, "+\n");

	if (_vi_resume(vdev) != 0) {
		vi_pr(VI_ERR, "vi resume failed\n");
		return -1;
	}


	for (i = 0; i < gViCtx->total_dev_num; i++) {
		if (!vdev->ctx.isp_pipe_cfg[i].is_yuv_bypass_path)
			vi_tuning_buf_setup(i);
	}

	_vi_mempool_reset();
	vi_tuning_buf_clear();

	_vi_scene_ctrl(vdev, &raw_max);

	//SW workaround to disable csibdg enable first due to csibdg enable is on as default.
	for (raw_num = ISP_PRERAW_A; raw_num < ISP_PRERAW_MAX; raw_num++)
		isp_streaming(&vdev->ctx, false, raw_num);

	/* cif lvds reset */
	_vi_call_cb(E_MODULE_CIF, CIF_CB_RESET_LVDS, &dev_num);
	if (vdev->ctx.is_multi_sensor) {
		for (raw_num = ISP_PRERAW_B; raw_num < gViCtx->total_dev_num; raw_num++) {
			dev_num = raw_num;
			_vi_call_cb(E_MODULE_CIF, CIF_CB_RESET_LVDS, &dev_num);
		}
	}

	for (raw_num = ISP_PRERAW_A; raw_num < raw_max; raw_num++) {
		/* Get stagger vsync info */

		vdev->ctx.isp_pipe_cfg[raw_num].is_stagger_vsync = gViCtx->enVcWdrMode[raw_num];
		vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en = csi_patgen_en[raw_num];

		if (vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en) {
#ifndef FPGA_TEST
			vdev->usr_fmt.width = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w;
			vdev->usr_fmt.height = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_h;
			vdev->usr_crop.width = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w;
			vdev->usr_crop.height = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_h;
#else
			vdev->usr_fmt.width = 1920;
			vdev->usr_fmt.height = 1080;
			vdev->usr_crop.width = 1920;
			vdev->usr_crop.height = 1080;
#endif
			vdev->usr_fmt.code = ISP_BAYER_TYPE_BG;
			vdev->usr_crop.left = 0;
			vdev->usr_crop.top = 0;

			vi_pr(VI_WARN, "patgen enable, w_h(%d:%d), color mode(%d)\n",
					vdev->usr_fmt.width, vdev->usr_fmt.height, vdev->usr_fmt.code);
		}

		_vi_ctrl_init(raw_num, vdev);
		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw)
			_vi_pre_fe_ctrl_setup(raw_num, vdev);

		if (!vdev->ctx.is_multi_sensor) { //only single sensor maybe break
			if (_is_all_online(&vdev->ctx) ||
				(_is_fe_be_online(&vdev->ctx) && vdev->ctx.is_slice_buf_on)) {
				vi_pr(VI_INFO, "on-the-fly mode or slice_buffer is on\n");
				break;
			}
		}

		if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw) {
			if (!vdev->ctx.isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
				_set_init_state(vdev, raw_num, ISP_FE_CH0);
				isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH0);
				if (vdev->ctx.isp_pipe_cfg[raw_num].is_hdr_on &&
					!vdev->ctx.isp_pipe_cfg[raw_num].is_synthetic_hdr_on) {
					_set_init_state(vdev, raw_num, ISP_FE_CH1);
					isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH1);
				}
			} else { //YUV sensor
				u8 total_chn = (raw_num == ISP_PRERAW_A) ?
						vdev->ctx.rawb_chnstr_num :
						vdev->ctx.total_chn_num - vdev->ctx.rawb_chnstr_num;
				u8 chn_str = 0;

				for (; chn_str < total_chn; chn_str++) {
					_set_init_state(vdev, raw_num, chn_str);
					isp_pre_trig(&vdev->ctx, raw_num, chn_str);
				}
			}
		}
	}

	_vi_preraw_be_init(vdev);
	_vi_postraw_ctrl_setup(vdev);
	_vi_dma_setup(&vdev->ctx, raw_max);
	_vi_dma_set_sw_mode(&vdev->ctx);

	vi_pr(VI_INFO, "ISP scene path, be_off=%d, post_off=%d, slice_buff_on=%d\n",
			vdev->ctx.is_offline_be, vdev->ctx.is_offline_postraw, vdev->ctx.is_slice_buf_on);

	/* assume config sensor_0, set raw_num=ISP_PRERAW_A */
	raw_num = ISP_PRERAW_A;
	if (_is_all_online(&vdev->ctx)) {
		if (vdev->ctx.isp_pipe_cfg[raw_num].is_offline_scaler) { //offline mode
			if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw)
				isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH0);

			_postraw_outbuf_enq(vdev, raw_num);
		} else { //online mode
			struct sc_cfg_cb post_para = {0};

			/* VI Online VPSS sc cb trigger */
			post_para.snr_num = raw_num;
			post_para.is_tile = false;
			//if bypass frm > 0, do not care vpss state
			if (gViCtx->bypass_frm[raw_num] == 0 &&
				_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
				vi_pr(VI_INFO, "sc is not ready. try later\n");
			} else {
				atomic_set(&vdev->ol_sc_frm_done, 0);

				if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw)
					isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH0);
			}
		}
	} else if (_is_fe_be_online(&vdev->ctx) && vdev->ctx.is_slice_buf_on) {
		if (vdev->ctx.isp_pipe_cfg[raw_num].is_offline_scaler) { //offline mode
			_postraw_outbuf_enq(vdev, raw_num);

			isp_post_trig(&vdev->ctx, raw_num);

			if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw) {
				if (!vdev->ctx.isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
					isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH0);
					if (vdev->ctx.isp_pipe_cfg[raw_num].is_hdr_on)
						isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH1);
				}
			}

			atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_RUNNING);
		} else { //online mode
			struct sc_cfg_cb post_para = {0};

			/* VI Online VPSS sc cb trigger */
			post_para.snr_num = raw_num;
			post_para.is_tile = false;
			if (gViCtx->bypass_frm[raw_num] == 0 &&
				_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
				vi_pr(VI_INFO, "sc is not ready. try later\n");
			} else {
				atomic_set(&vdev->ol_sc_frm_done, 0);

				isp_post_trig(&vdev->ctx, raw_num);

				if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw) {
					isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH0);
					if (vdev->ctx.isp_pipe_cfg[raw_num].is_hdr_on)
						isp_pre_trig(&vdev->ctx, raw_num, ISP_FE_CH1);
				}

				atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_RUNNING);
			}
		}
	}

	_viBWCalSet(vdev);

	_vi_update_yuvout_config(vdev, raw_num);
#ifdef FPGA_TEST
	vi_ip_test_cases(&vdev->ctx);
#endif

	spin_lock_irqsave(&stream_lock, flags);
	for (raw_num = ISP_PRERAW_A; raw_num < gViCtx->total_dev_num; raw_num++)
		isp_streaming(&vdev->ctx, true, raw_num);
	spin_unlock_irqrestore(&stream_lock, flags);

	atomic_set(&vdev->isp_streamoff, 0);

	return rc;
}

/* abort streaming and wait for last buffer */
int vi_stop_streaming(struct cvi_vi_dev *vdev)
{
	struct cvi_isp_buf2 *cvi_vb2, *tmp;
	struct _isp_dqbuf_n *n = NULL;
	struct vi_event_k   *ev_k = NULL;
	unsigned long flags;
	unsigned long stream_flags;
	struct isp_buffer *isp_b;
	struct _isp_snr_i2c_node *i2c_n;
	struct _isp_raw_num_n    *raw_n;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	u8 i = 0, count = 10;
	u8 rc = 0;

	vi_pr(VI_INFO, "+\n");

	atomic_set(&vdev->isp_streamoff, 1);

	// disable load-from-dram at streamoff
	for (raw_num = ISP_PRERAW_A; raw_num < ISP_PRERAW_MAX; raw_num++)
		vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw = false;

	usr_pic_time_remove();

	// wait to make sure hw stopped.
	while (--count > 0) {
		if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_be_state[ISP_BE_CH0]) == ISP_PRERAW_IDLE &&
			atomic_read(&vdev->pre_be_state[ISP_BE_CH1]) == ISP_PRERAW_IDLE)
			break;
		vi_pr(VI_WARN, "wait count(%d)\n", count);
#ifdef FPGA_PORTING
		msleep(200);
#else
		usleep(100 * 1000);
#endif
	}

	if (count == 0) {
		vi_pr(VI_ERR, "isp status fe_0(ch0:%d, ch1:%d) fe_1(ch0:%d, ch1:%d) \
				fe_2(ch0:%d, ch1:%d) be(ch0:%d, ch1:%d) postraw(%d)\n",
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH0]),
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH1]),
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH0]),
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH1]),
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH0]),
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH1]),
				atomic_read(&vdev->pre_be_state[ISP_BE_CH0]),
				atomic_read(&vdev->pre_be_state[ISP_BE_CH1]),
				atomic_read(&vdev->postraw_state));
	}

#if 0
	for (i = 0; i < 2; i++) {
		/*
		 * Release all the buffers enqueued to driver
		 * when streamoff is issued
		 */
		spin_lock_irqsave(&vdev->rdy_lock, flags);
		list_for_each_entry_safe(cvi_vb2, tmp, &(vdev->rdy_queue[i]), list) {
			free(cvi_vb2);
		}
		vdev->num_rdy[i] = 0;
		INIT_LIST_HEAD(&vdev->rdy_queue[i]);
		spin_unlock_irqrestore(&vdev->rdy_lock, flags);
	}
#endif

	for (i = 0; i < ISP_FE_CHN_MAX; i++) {
		/*
		 * Release all the buffers enqueued to driver
		 * when streamoff is issued
		 */
		spin_lock_irqsave(&vdev->qbuf_lock, flags);
		list_for_each_entry_safe(cvi_vb2, tmp, &(vdev->qbuf_list[i]), list) {
			free(cvi_vb2);
		}
		vdev->qbuf_num[i] = 0;
		INIT_LIST_HEAD(&vdev->qbuf_list[i]);
		spin_unlock_irqrestore(&vdev->qbuf_lock, flags);
	}

	spin_lock_irqsave(&dq_lock, flags);
	while (!list_empty(&dqbuf_q.list)) {
		n = list_first_entry(&dqbuf_q.list, struct _isp_dqbuf_n, list);
		list_del_init(&n->list);
		free(n);
	}
	spin_unlock_irqrestore(&dq_lock, flags);

	spin_lock_irqsave(&event_lock, flags);
	while (!list_empty(&event_q.list)) {
		ev_k = list_first_entry(&event_q.list, struct vi_event_k, list);
		list_del_init(&ev_k->list);
		free(ev_k);
	}
	spin_unlock_irqrestore(&event_lock, flags);

	for (i = 0; i < ISP_CHN_MAX; i++) {
		while ((isp_b = isp_buf_remove(&pre_out_queue[i])) != NULL)
			free(isp_b);
		while ((isp_b = isp_buf_remove(&pre_out_se_queue[i])) != NULL)
			free(isp_b);
	}

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		while ((isp_b = isp_buf_remove(&raw_dump_b_dq[i])) != NULL)
			free(isp_b);
		while ((isp_b = isp_buf_remove(&raw_dump_b_se_dq[i])) != NULL)
			free(isp_b);
		while ((isp_b = isp_buf_remove(&raw_dump_b_q[i])) != NULL)
			free(isp_b);
		while ((isp_b = isp_buf_remove(&raw_dump_b_se_q[i])) != NULL)
			free(isp_b);

		spin_lock_irqsave(&snr_node_lock[i], flags);
		while (!list_empty(&isp_snr_i2c_queue[i].list)) {
			i2c_n = list_first_entry(&isp_snr_i2c_queue[i].list, struct _isp_snr_i2c_node, list);
			list_del_init(&i2c_n->list);
			free(i2c_n);
		}
		isp_snr_i2c_queue[i].num_rdy = 0;
		spin_unlock_irqrestore(&snr_node_lock[i], flags);

		while ((isp_b = isp_buf_remove(&pre_be_in_se_q[i])) != NULL)
			free(isp_b);
		while ((isp_b = isp_buf_remove(&pre_be_in_se_syn_q[i])) != NULL)
			free(isp_b);
	}

	while ((isp_b = isp_buf_remove(&pre_be_in_q)) != NULL)
		free(isp_b);
	while ((isp_b = isp_buf_remove(&pre_be_in_syn_q)) != NULL)
		free(isp_b);
	while ((isp_b = isp_buf_remove(&pre_be_out_q)) != NULL)
		free(isp_b);
	while ((isp_b = isp_buf_remove(&pre_be_out_se_q)) != NULL)
		free(isp_b);

	while ((isp_b = isp_buf_remove(&post_in_queue)) != NULL)
		free(isp_b);
	while ((isp_b = isp_buf_remove(&post_in_se_queue)) != NULL)
		free(isp_b);

	spin_lock_irqsave(&raw_num_lock, flags);
	while (!list_empty(&pre_raw_num_q.list)) {
		raw_n = list_first_entry(&pre_raw_num_q.list, struct _isp_raw_num_n, list);
		list_del_init(&raw_n->list);
		free(raw_n);
	}
	pre_raw_num_q.num_rdy = 0;
	spin_unlock_irqrestore(&raw_num_lock, flags);

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		free(isp_bufpool[i].fswdr_rpt);
		isp_bufpool[i].fswdr_rpt = 0;
	}

	spin_lock_irqsave(&stream_lock, stream_flags);
	// reset at stop for next run.
	isp_reset(&vdev->ctx);
	for (raw_num = ISP_PRERAW_A; raw_num < gViCtx->total_dev_num; raw_num++)
		isp_streaming(&vdev->ctx, false, raw_num);

	_vi_suspend(vdev);
	spin_unlock_irqrestore(&stream_lock, stream_flags);

	return rc;
}

static int _pre_be_outbuf_enque(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const u8 hw_chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
		enum ISP_BLK_ID_T pre_be_dma = (hw_chn_num == ISP_BE_CH0) ?
						ISP_BLK_ID_DMA_CTL22 : ISP_BLK_ID_DMA_CTL23;
		struct isp_queue *be_out_q = (hw_chn_num == ISP_BE_CH0) ?
						&pre_be_out_q : &pre_be_out_se_q;
		struct isp_buffer *b = NULL;

		b = isp_next_buf(be_out_q);
		if (!b) {
			vi_pr(VI_DBG, "pre_be chn_num_%d outbuf is empty\n", hw_chn_num);
			return 0;
		}

		ispblk_dma_setaddr(ctx, pre_be_dma, b->addr);
	} else if (ctx->is_multi_sensor &&
		   ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB+YUV sensor
		u8 buf_chn = vdev->ctx.rawb_chnstr_num + hw_chn_num;
		enum ISP_BLK_ID_T pre_fe_dma;
		struct isp_queue *fe_out_q = &pre_out_queue[buf_chn];
		struct isp_buffer *b = NULL;

		if (raw_num == ISP_PRERAW_C) {
			pre_fe_dma = (buf_chn == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19;
		} else {
			pre_fe_dma = (buf_chn == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13;
		}

		b = isp_next_buf(fe_out_q);
		if (!b) {
			vi_pr(VI_DBG, "pre_fe_%d buf_chn_num_%d outbuf is empty\n", raw_num, buf_chn);
			return 0;
		}

		ispblk_dma_setaddr(ctx, pre_fe_dma, b->addr);
	}

	return 1;
}

static int _pre_fe_outbuf_enque(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const enum cvi_isp_pre_chn_num fe_chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
		enum ISP_BLK_ID_T pre_fe_dma;
		struct isp_queue *fe_out_q = (fe_chn_num == ISP_FE_CH0) ?
						&pre_out_queue[raw_num] : &pre_out_se_queue[raw_num];
		struct isp_buffer *b = NULL;
		u8 trigger = false;

		if (raw_num == ISP_PRERAW_A) {
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
				pre_fe_dma = ISP_BLK_ID_DMA_CTL6;
			else
				pre_fe_dma = (fe_chn_num == ISP_FE_CH0) ?
						ISP_BLK_ID_DMA_CTL6 : ISP_BLK_ID_DMA_CTL7;
		} else if (raw_num == ISP_PRERAW_B) {
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
				pre_fe_dma = ISP_BLK_ID_DMA_CTL12;
			else
				pre_fe_dma = (fe_chn_num == ISP_FE_CH0) ?
						 ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13;
		} else {
			// if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
			// 	pre_fe_dma = ISP_BLK_ID_DMA_CTL18;
			// else
				pre_fe_dma = (fe_chn_num == ISP_FE_CH0) ?
						ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19;
		}

		if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1) {//raw_dump flow
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
				trigger = vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] ==
						vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1];
			} else {
				trigger = true;
			}

			if (trigger) {
				struct isp_queue *fe_out_q = &raw_dump_b_q[raw_num];
				u32 dmaid, dmaid_se;

				if (raw_num == ISP_PRERAW_A) {
					dmaid		= ISP_BLK_ID_DMA_CTL6;
					dmaid_se	= ISP_BLK_ID_DMA_CTL7;
				} else if (raw_num == ISP_PRERAW_B) {
					dmaid		= ISP_BLK_ID_DMA_CTL12;
					dmaid_se	= ISP_BLK_ID_DMA_CTL13;
				} else {
					dmaid		= ISP_BLK_ID_DMA_CTL18;
					dmaid_se	= ISP_BLK_ID_DMA_CTL19;
				}

				vi_pr(VI_DBG, "pre_fe raw_dump cfg start\n");

				b = isp_next_buf(fe_out_q);
				if (b == NULL) {
					vi_pr(VI_ERR, "Pre_fe_%d LE raw_dump outbuf is empty\n", raw_num);
					return 0;
				}

				ispblk_dma_setaddr(ctx, dmaid, b->addr);

				if (ctx->isp_pipe_cfg[raw_num].is_hdr_on &&
					!ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) {
					struct isp_buffer *b_se = NULL;
					struct isp_queue *fe_out_q_se = &raw_dump_b_se_q[raw_num];

					b_se = isp_next_buf(fe_out_q_se);
					if (b_se == NULL) {
						vi_pr(VI_ERR, "Pre_fe_%d SE raw_dump outbuf is empty\n", raw_num);
						return 0;
					}

					ispblk_dma_setaddr(ctx, dmaid_se, b_se->addr);
				}

				atomic_set(&vdev->isp_raw_dump_en[raw_num], 2);
			}
		} else if ((ctx->isp_pipe_cfg[raw_num].is_hdr_on &&
				ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) &&
				(atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 3)) {
			struct isp_buffer *b_se = NULL;
			struct isp_queue *fe_out_q_se = &raw_dump_b_se_q[raw_num];

			b_se = isp_next_buf(fe_out_q_se);
			if (b_se == NULL) {
				vi_pr(VI_ERR, "Pre_fe_%d SE raw_dump outbuf is empty\n", raw_num);
				return 0;
			}

			ispblk_dma_setaddr(ctx, pre_fe_dma, b_se->addr);
		} else {

			b = isp_next_buf(fe_out_q);
			if (!b) {
				vi_pr(VI_DBG, "pre_fe_%d chn_num_%d outbuf is empty\n", raw_num, fe_chn_num);
				return 0;
			}

			ispblk_dma_setaddr(ctx, pre_fe_dma, b->addr);
		}
	} else if (ctx->is_multi_sensor &&
		   ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB+YUV sensor
		u8 buf_chn = vdev->ctx.rawb_chnstr_num + fe_chn_num;
		enum ISP_BLK_ID_T pre_fe_dma;
		struct isp_queue *fe_out_q = &pre_out_queue[buf_chn];
		struct isp_buffer *b = NULL;

		if (raw_num == ISP_PRERAW_C) {
			pre_fe_dma = (buf_chn == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19;
		} else {
			pre_fe_dma = (buf_chn == ctx->rawb_chnstr_num) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13;
		}

		b = isp_next_buf(fe_out_q);
		if (!b) {
			vi_pr(VI_DBG, "pre_fe_%d buf_chn_num_%d outbuf is empty\n", raw_num, buf_chn);
			return 0;
		}

		ispblk_dma_setaddr(ctx, pre_fe_dma, b->addr);
	}

	return 1;
}

static int _postraw_inbuf_enq_check(struct cvi_vi_dev *vdev,
						enum cvi_isp_raw *raw_num,
						enum cvi_isp_chn_num *chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	struct isp_queue *in_q = NULL, *in_se_q = NULL;
	struct isp_buffer *b = NULL, *b_se = NULL;
	int ret = 0;

	if (_is_fe_be_online(ctx)) { //fe->be->dram->post
		in_q = &post_in_queue;
	} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
		in_q = ctx->is_synthetic_hdr_on ? &pre_be_in_syn_q : &pre_be_in_q;
	}

retry:
	b = isp_next_buf(in_q);
	if (b == NULL) {
		if (_is_fe_be_online(ctx)) //fe->be->dram->post
			vi_pr(VI_DBG, "Postraw input buf is empty\n");
		else if (_is_be_post_online(ctx)) { //fe->dram->be->post
			if (ctx->is_synthetic_hdr_on && in_q == &pre_be_in_syn_q) {
				vi_pr(VI_DBG, "Pre_be input syn buf is empty\n");
				in_q = &pre_be_in_q;
				goto retry;
			} else
				vi_pr(VI_DBG, "Pre_be input buf is empty\n");
		}
		ret = 1;
		return ret;
	}

	*raw_num = b->raw_num;
	*chn_num = (b->is_yuv_frm) ? b->chn_num : b->raw_num;

	vdev->ctx.isp_pipe_cfg[b->raw_num].crop.x = b->crop_le.x;
	vdev->ctx.isp_pipe_cfg[b->raw_num].crop.y = b->crop_le.y;
	vdev->ctx.isp_pipe_cfg[b->raw_num].crop.w = vdev->ctx.img_width =
							ctx->isp_pipe_cfg[b->raw_num].post_img_w;
	vdev->ctx.isp_pipe_cfg[b->raw_num].crop.h = vdev->ctx.img_height =
							ctx->isp_pipe_cfg[b->raw_num].post_img_h;

	//YUV sensor, offline return error, online than config rawtop read dma.
	if (ctx->isp_pipe_cfg[b->raw_num].is_yuv_bypass_path) {
		if (ctx->isp_pipe_cfg[b->raw_num].is_offline_scaler) {
			ret = 1;
		} else {
			ispblk_dma_yuv_bypass_config(ctx, ISP_BLK_ID_DMA_CTL28, b->addr, b->raw_num);
		}

		return ret;
	}

	isp_bufpool[b->raw_num].post_ir_busy_idx = b->ir_idx;

	if (_is_fe_be_online(ctx)) { //fe->be->dram->post
		in_se_q = &post_in_se_queue;
	} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
		in_se_q = ctx->isp_pipe_cfg[b->raw_num].is_synthetic_hdr_on
				? &pre_be_in_se_syn_q[b->raw_num]
				: &pre_be_in_se_q[b->raw_num];
	}

	if (ctx->isp_pipe_cfg[b->raw_num].is_hdr_on) {
		b_se = isp_next_buf(in_se_q);
		if (b_se == NULL) {
			if (_is_fe_be_online(ctx)) //fe->be->dram->post
				vi_pr(VI_DBG, "Postraw se input buf is empty\n");
			else if (_is_be_post_online(ctx)) { //fe->dram->be->post
				if (ctx->isp_pipe_cfg[b->raw_num].is_synthetic_hdr_on) {
					vi_pr(VI_DBG, "Pre_be se input syn buf is empty\n");
					in_q = &pre_be_in_q;
					goto retry;
				} else
					vi_pr(VI_DBG, "Pre_be se input buf is empty\n");
			}
			ret = 1;
			return ret;
		}
	}

	vdev->ctx.isp_pipe_cfg[b->raw_num].rgbmap_i.w_bit = b->rgbmap_i.w_bit;
	vdev->ctx.isp_pipe_cfg[b->raw_num].rgbmap_i.h_bit = b->rgbmap_i.h_bit;

	vdev->ctx.isp_pipe_cfg[b->raw_num].lmap_i.w_bit = b->lmap_i.w_bit;
	vdev->ctx.isp_pipe_cfg[b->raw_num].lmap_i.h_bit = b->lmap_i.h_bit;

	if (_is_fe_be_online(ctx)) { //fe->be->dram->post

		if (ctx->is_slice_buf_on)
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL28, b->addr);
		else
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL28, b->addr);
		if (ctx->isp_pipe_cfg[b->raw_num].is_hdr_on) {
			vdev->ctx.isp_pipe_cfg[b->raw_num].crop_se.x = b_se->crop_se.x;
			vdev->ctx.isp_pipe_cfg[b->raw_num].crop_se.y = b_se->crop_se.y;
			vdev->ctx.isp_pipe_cfg[b->raw_num].crop_se.w = vdev->ctx.img_width;
			vdev->ctx.isp_pipe_cfg[b->raw_num].crop_se.h = vdev->ctx.img_height;

			if (ctx->is_slice_buf_on)
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL29, b_se->addr);
			else
				ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL29, b_se->addr);
		}
	} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL4, b->addr);
		if (ctx->isp_pipe_cfg[b->raw_num].is_hdr_on)
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL5, b_se->addr);
	}

	return ret;
}

static void _postraw_outbuf_enque(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct vi_buffer *vb2_buf;
	struct cvi_isp_buf2 *b = NULL;
	struct isp_ctx *ctx = &vdev->ctx;
	u64 tmp_addr = 0, i;

	//Get the buffer for postraw output buffer
	b = _cvi_isp_next_buf2(vdev, raw_num);
	vb2_buf = &b->buf;

	vi_pr(VI_DBG, "update isp-buf: 0x%llx-0x%llx\n",
		vb2_buf->planes[0].addr, vb2_buf->planes[1].addr);

	for (i = 0; i < 2; i++) {
		tmp_addr = (u64)vb2_buf->planes[i].addr;
		if (i == 0)
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL46, tmp_addr);
		else
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL47, tmp_addr);
	}
}

static u8 _postraw_outbuf_empty(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	u8 ret = 0;

	if (cvi_isp_rdy_buf_empty2(vdev, raw_num)) {
		vi_pr(VI_DBG, "postraw chn_%d output buffer is empty\n", raw_num);
		ret = 1;
	}

	return ret;
}

void _postraw_outbuf_enq(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	if (vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num])
		return;

	cvi_isp_rdy_buf_pop2(vdev, raw_num);
	_postraw_outbuf_enque(vdev, raw_num);
}

/*
 * for postraw offline only.
 *  trig preraw if there is output buffer in preraw output.
 */
void _pre_hw_enque(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const u8 chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	//ISP frame error handling
	if (atomic_read(&vdev->isp_err_handle_flag) == 1
		&& !(ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on && chn_num == ISP_FE_CH1))
		return;

#if PORTING_TEST //test only
	if (stop_stream_en) {
		vi_pr(VI_WARN, "stop_stream_en\n");
		return;
	}
#endif

	if (atomic_read(&vdev->isp_streamoff) == 0) {
		if (_is_drop_next_frame(vdev, raw_num, chn_num)) {
			vi_pr(VI_DBG, "Pre_fe_%d chn_num_%d drop_frame_num %d\n",
					raw_num, chn_num, vdev->drop_frame_number[raw_num]);
			return;
		}
		if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) { //fe->be->dram->post
			if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
				if (atomic_cmpxchg(&vdev->pre_be_state[chn_num],
							ISP_PRE_BE_IDLE, ISP_PRE_BE_RUNNING) ==
							ISP_PRE_BE_RUNNING) {
					vi_pr(VI_DBG, "Pre_be chn_num_%d is running\n", chn_num);
					return;
				}
			} else { //YUV sensor
				if (atomic_cmpxchg(&vdev->pre_fe_state[raw_num][chn_num],
							ISP_PRERAW_IDLE, ISP_PRERAW_RUNNING) ==
							ISP_PRERAW_RUNNING) {
					vi_pr(VI_DBG, "Pre_fe_%d chn_num_%d is running\n", raw_num, chn_num);
					return;
				}
			}

			// only if fe->be->dram
			if (_pre_be_outbuf_enque(vdev, raw_num, chn_num)) {
				if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1) //raw_dump flow
					_isp_fe_be_raw_dump_cfg(vdev, raw_num, chn_num);
				isp_pre_trig(ctx, raw_num, chn_num);
			} else {
				if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //RGB sensor
					atomic_set(&vdev->pre_be_state[chn_num], ISP_PRE_BE_IDLE);
				else  //YUV sensor
					atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_IDLE);
			}
		} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
			if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
				if (atomic_read(&vdev->isp_streamon) == 0) {
					vi_pr(VI_DBG, "VI not ready\n");
					return;
				}

				if (atomic_cmpxchg(&vdev->postraw_state, ISP_POSTRAW_IDLE, ISP_POSTRAW_RUNNING)
							!= ISP_POSTRAW_IDLE) {
					vi_pr(VI_DBG, "Postraw is running\n");
					return;
				}

				if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler onffline mode
					if (_postraw_outbuf_empty(vdev, raw_num)) {
						atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
						return;
					}

					_postraw_outbuf_enq(vdev, raw_num);
				} else { //Scaler online mode
					struct sc_cfg_cb post_para = {0};

					/* VI Online VPSS sc cb trigger */
					post_para.snr_num = raw_num;
					post_para.is_tile = false;
					vi_fill_mlv_info(NULL, raw_num, &post_para.m_lv_i, false);
					if (_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
						vi_pr(VI_DBG, "snr_num_%d, SC is running\n", raw_num);
						atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
						atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
						return;
					}

					atomic_set(&vdev->ol_sc_frm_done, 0);
				}

				if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1) //raw_dump flow
					_isp_fe_be_raw_dump_cfg(vdev, raw_num, chn_num);

				isp_pre_trig(ctx, raw_num, chn_num);
			} else {
				if (atomic_cmpxchg(&vdev->pre_fe_state[raw_num][chn_num],
							ISP_PRERAW_IDLE, ISP_PRERAW_RUNNING) ==
							ISP_PRERAW_RUNNING) {
					vi_pr(VI_DBG, "Pre_fe_%d chn_num_%d is running\n", raw_num, chn_num);
					return;
				}

				// only if fe->dram
				if (_pre_fe_outbuf_enque(vdev, raw_num, chn_num))
					isp_pre_trig(ctx, raw_num, chn_num);
				else
					atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_IDLE);
			}
		} else if (_is_all_online(ctx)) {
			if (atomic_cmpxchg(&vdev->postraw_state, ISP_POSTRAW_IDLE, ISP_POSTRAW_RUNNING)
						!= ISP_POSTRAW_IDLE) {
				vi_pr(VI_DBG, "Postraw is running\n");
				return;
			}

			if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler onffline mode
				if (_postraw_outbuf_empty(vdev, raw_num)) {
					atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
					return;
				}

				_postraw_outbuf_enq(vdev, raw_num);
			}

			if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1) //raw_dump flow
				_isp_fe_be_raw_dump_cfg(vdev, raw_num, chn_num);

			isp_pre_trig(ctx, raw_num, chn_num);
		} else if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) { //fe->be->dram->post
			if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //RGB sensor
				if (atomic_cmpxchg(&vdev->pre_fe_state[raw_num][chn_num],
							ISP_PRERAW_IDLE, ISP_PRERAW_RUNNING) ==
							ISP_PRERAW_RUNNING) {
					vi_pr(VI_DBG, "Pre_fe chn_num_%d is running\n", chn_num);
					return;
				}
			} else { //YUV sensor
				if (atomic_cmpxchg(&vdev->pre_fe_state[raw_num][chn_num],
							ISP_PRERAW_IDLE, ISP_PRERAW_RUNNING) ==
							ISP_PRERAW_RUNNING) {
					vi_pr(VI_DBG, "Pre_fe_%d chn_num_%d is running\n", raw_num, chn_num);
					return;
				}
			}

			if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 1) //raw_dump flow
				_isp_fe_be_raw_dump_cfg(vdev, raw_num, chn_num);
			isp_pre_trig(ctx, raw_num, chn_num);
		}
	}
}

static inline void _swap_post_sts_buf(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	struct _membuf *pool;
	unsigned long flags;
	uint8_t idx;

	pool = &isp_bufpool[raw_num];

	spin_lock_irqsave(&pool->post_sts_lock, flags);
	if (pool->post_sts_in_use == 1) {
		spin_unlock_irqrestore(&pool->post_sts_lock, flags);
		return;
	}
	pool->post_sts_busy_idx ^= 1;
	spin_unlock_irqrestore(&pool->post_sts_lock, flags);

	if (_is_be_post_online(ctx))
		idx = pool->post_sts_busy_idx ^ 1;
	else
		idx = pool->post_sts_busy_idx;

	//gms dma
	ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL25, pool->sts_mem[idx].gms.phy_addr);

	//ae le dma
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL26, pool->sts_mem[idx].ae_le.phy_addr);
	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
		//ae se dma
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL27, pool->sts_mem[idx].ae_se.phy_addr);
	}

	//dci dma is fixed size
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL45, pool->sts_mem[idx].dci.phy_addr);
	//hist edge v dma is fixed size
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL38, pool->sts_mem[idx].hist_edge_v.phy_addr);
}

static inline void _post_rgbmap_update(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num, const u32 frm_num)
{
	u64 rdma10, rdma11, rdma8, rdma9;
	u8 cur_idx = (frm_num - ctx->rgbmap_prebuf_idx) % RGBMAP_BUF_IDX;
	u8 pre_idx = (frm_num - 1 + RGBMAP_BUF_IDX - ctx->rgbmap_prebuf_idx) % RGBMAP_BUF_IDX;

	rdma10 = isp_bufpool[raw_num].rgbmap_le[cur_idx];
	if (frm_num <= ctx->rgbmap_prebuf_idx)
		rdma8 = isp_bufpool[raw_num].rgbmap_le[0];
	else
		rdma8 = isp_bufpool[raw_num].rgbmap_le[pre_idx];

	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL34, rdma10);
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL32, rdma8);

	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
		rdma11 = isp_bufpool[raw_num].rgbmap_se[cur_idx];
		if (frm_num <= ctx->rgbmap_prebuf_idx)
			rdma9 = isp_bufpool[raw_num].rgbmap_se[0];
		else
			rdma9 = isp_bufpool[raw_num].rgbmap_se[pre_idx];

		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL35, rdma11);
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL33, rdma9);
	}
}

static inline void _post_lmap_update(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	u64 lmap_le = isp_bufpool[raw_num].lmap_le;
	u64 lmap_se = isp_bufpool[raw_num].lmap_se;

	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL39, lmap_le);
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL30, lmap_le);

	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && !ctx->isp_pipe_cfg[raw_num].is_hdr_detail_en) {
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL40, lmap_se);
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL31, lmap_se);
	} else {
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL40, lmap_le);
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL31, lmap_le);
	}
}

static inline void _post_mlsc_update(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	uint64_t lsc_dma = isp_bufpool[raw_num].lsc;

	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL24, lsc_dma);
}

static inline void _post_ldci_update(struct isp_ctx *ctx, const enum cvi_isp_raw raw_num)
{
	uint64_t ldci_dma = isp_bufpool[raw_num].ldci;

	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL49, ldci_dma);
}

static inline void _post_dma_update(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint64_t manr_addr = isp_bufpool[raw_num].manr;
	uint64_t r_uv_addr, r_y_addr;
	uint64_t w_uv_addr, w_y_addr;

	r_uv_addr = w_uv_addr = isp_bufpool[raw_num].tdnr[0];
	r_y_addr  = w_y_addr  = isp_bufpool[raw_num].tdnr[1];

	//Update rgbmap dma addr
	_post_rgbmap_update(ctx, raw_num, vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0]);

	//update lmap dma
	_post_lmap_update(ctx, raw_num);

	//update mlsc dma
	_post_mlsc_update(ctx, raw_num);

	//update ldci dma
	_post_ldci_update(ctx, raw_num);

	if (ctx->is_3dnr_on) {
		if (ctx->is_fbc_on) {
			//3dnr y
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL41, r_y_addr);
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL43, w_y_addr);

			//3dnr uv
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL42, r_uv_addr);
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL44, w_uv_addr);
		} else {
			//3dnr y
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL41, r_y_addr);
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL43, w_y_addr);

			//3dnr uv
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL42, r_uv_addr);
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL44, w_uv_addr);
		}

		//manr
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL36, manr_addr);
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL37, manr_addr);
	}

	if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) {
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL46, true, false);
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL47, true, false);
	} else {
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL46, false, false);
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL47, false, false);
	}
}

static u32 _is_first_frm_after_drop(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint32_t first_frm_num_after_drop = ctx->isp_pipe_cfg[raw_num].isp_reset_frm;
	u32 frm_num = 0;

	frm_num = vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0];

	if ((first_frm_num_after_drop != 0) && (frm_num == first_frm_num_after_drop)) {
		vi_pr(VI_DBG, "reset isp frm_num[%d]\n", frm_num);
		ctx->isp_pipe_cfg[raw_num].isp_reset_frm = 0;
		return 1;
	} else
		return 0;
}

static inline void _post_ctrl_update(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	ispblk_post_cfg_update(ctx, raw_num);

	ispblk_fusion_hdr_cfg(ctx, raw_num);

	_vi_clear_mmap_fbc_ring_base(vdev, raw_num);

	if (ctx->is_3dnr_on)
		ispblk_tnr_post_chg(ctx, raw_num);

	if (ctx->is_multi_sensor) {
		//To set apply the prev frm or not for manr/3dnr
		if (vdev->preraw_first_frm[raw_num]) {
			vdev->preraw_first_frm[raw_num] = false;
			isp_first_frm_reset(ctx, 1);
		} else {
			isp_first_frm_reset(ctx, 0);
		}
	}

	if (_is_first_frm_after_drop(vdev, raw_num)) {
		isp_first_frm_reset(ctx, 1);
	} else {
		isp_first_frm_reset(ctx, 0);
	}
}

static uint8_t _pre_be_sts_in_use_chk(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 chn_num)
{
	unsigned long flags;
	static u8 be_in_use;

	if (chn_num == ISP_BE_CH0) {
		spin_lock_irqsave(&isp_bufpool[raw_num].pre_be_sts_lock, flags);
		if (isp_bufpool[raw_num].pre_be_sts_in_use == 1) {
			be_in_use = 1;
		} else {
			be_in_use = 0;
			isp_bufpool[raw_num].pre_be_sts_busy_idx ^= 1;
		}
		spin_unlock_irqrestore(&isp_bufpool[raw_num].pre_be_sts_lock, flags);
	}

	return be_in_use;
}

static inline void _swap_pre_be_sts_buf(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	struct _membuf *pool;
	uint8_t idx;

	if (_pre_be_sts_in_use_chk(vdev, raw_num, chn_num) == 0) {
		pool = &isp_bufpool[raw_num];
		if (_is_be_post_online(ctx))
			idx = isp_bufpool[raw_num].pre_be_sts_busy_idx ^ 1;
		else
			idx = isp_bufpool[raw_num].pre_be_sts_busy_idx;

		if (chn_num == ISP_BE_CH0) {
			//af dma
			ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL21, pool->sts_mem[idx].af.phy_addr);
		}
	}
}

static inline void _pre_be_ctrl_update(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;

	ispblk_pre_be_cfg_update(ctx, raw_num);
}

static inline u8 _isp_clk_dynamic_en(struct cvi_vi_dev *vdev, bool en)
{
#if 0//ToDo
	if (clk_dynamic_en && vdev->isp_clk[5]) {
		struct isp_ctx *ctx = &vdev->ctx;

		if (en && !__clk_is_enabled(vdev->isp_clk[5])) {
			if (clk_enable(vdev->isp_clk[5])) {
				vi_pr(VI_ERR, "[ERR] ISP_CLK(%s) enable fail\n", CLK_ISP_NAME[5]);
				if (_is_fe_be_online(ctx))
					atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				else if (_is_be_post_online(ctx)) {
					atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
					atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				}

				return -1;
			}

			vi_pr(VI_DBG, "enable clk(%s)\n", CLK_ISP_NAME[5]);
		} else if (!en && __clk_is_enabled(vdev->isp_clk[5])) {
			clk_disable(vdev->isp_clk[5]);

			vi_pr(VI_DBG, "disable clk(%s)\n", CLK_ISP_NAME[5]);
		}
	} else { //check isp_top_clk is enabled
		struct isp_ctx *ctx = &vdev->ctx;

		if (!__clk_is_enabled(vdev->isp_clk[5])) {
			if (clk_enable(vdev->isp_clk[5])) {
				vip_pr(CVI_ERR, "[ERR] ISP_CLK(%s) enable fail\n", CLK_ISP_NAME[5]);
				if (_is_fe_be_online(ctx))
					atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				else if (_is_be_post_online(ctx)) {
					atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
					atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				}
			}
		}
	}
#endif
	return 0;
}

/*
 * - postraw offline -
 *  trig postraw if there is in/out buffer for postraw
 * - postraw online -
 *  trig preraw if there is output buffer for postraw
 */
static void _post_hw_enque(
	struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	enum cvi_isp_chn_num chn_num = ISP_CHN0;

	if (atomic_read(&vdev->isp_streamoff) == 1)
		return;

	if (atomic_read(&vdev->isp_err_handle_flag) == 1)
		return;

	if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) { //fe->be->dram->post
		if (atomic_cmpxchg(&vdev->postraw_state, ISP_POSTRAW_IDLE, ISP_POSTRAW_RUNNING) != ISP_POSTRAW_IDLE) {
			vi_pr(VI_DBG, "Postraw is running\n");
			return;
		}

		if (_postraw_inbuf_enq_check(vdev, &raw_num, &chn_num)) {
			atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
			return;
		}

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler online mode
			struct sc_cfg_cb post_para = {0};

			/* VI Online VPSS sc cb trigger */
			post_para.snr_num = raw_num;
			post_para.is_tile = false;
			vi_fill_mlv_info(NULL, raw_num, &post_para.m_lv_i, false);
			if (!(vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num]) &&
				_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				return;
			}

			atomic_set(&vdev->ol_sc_frm_done, 0);
			ctx->is_tile = post_para.is_tile;
		} else { //Scaler offline mode
			if (_postraw_outbuf_empty(vdev, raw_num)) {
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				return;
			}

			_postraw_outbuf_enq(vdev, raw_num);
		}

		if (_isp_clk_dynamic_en(vdev, true) < 0)
			return;

		ispblk_post_yuv_cfg_update(ctx, raw_num);

		if (ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //YUV sensor online mode
			goto YUV_POSTRAW_TILE;

		postraw_tuning_update(&vdev->ctx, raw_num);

		//Update postraw size/ctrl flow
		_post_ctrl_update(vdev, raw_num);
		//Update postraw dma size/addr
		_post_dma_update(vdev, raw_num);

YUV_POSTRAW_TILE:
		_vi_update_yuvout_config(vdev, raw_num);

		vdev->offline_raw_num = raw_num;

		ctx->cam_id = raw_num;

		isp_post_trig(ctx, raw_num);

		vi_record_post_trigger(vdev, raw_num);
	} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
		if (atomic_cmpxchg(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE, ISP_PRE_BE_RUNNING)
										!= ISP_PRE_BE_IDLE) {
			vi_pr(VI_DBG, "Pre_be ch_num_%d is running\n", ISP_BE_CH0);
			return;
		}

		if (atomic_cmpxchg(&vdev->postraw_state, ISP_POSTRAW_IDLE, ISP_POSTRAW_RUNNING) != ISP_POSTRAW_IDLE) {
			atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
			vi_pr(VI_DBG, "Postraw is running\n");
			return;
		}

		if (_postraw_inbuf_enq_check(vdev, &raw_num, &chn_num)) {
			atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
			atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
			return;
		}

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler online mode
			struct sc_cfg_cb post_para = {0};

			/* VI Online VPSS sc cb trigger */
			post_para.snr_num = raw_num;
			post_para.is_tile = false;
			vi_fill_mlv_info(NULL, raw_num, &post_para.m_lv_i, false);
			if (!(vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num]) &&
				_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
				vi_pr(VI_DBG, "snr_num_%d, SC is running\n", raw_num);
				atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				return;
			}

			atomic_set(&vdev->ol_sc_frm_done, 0);
		} else { //Scaler offline mode
			if (_postraw_outbuf_empty(vdev, raw_num)) {
				atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				return;
			}

			_postraw_outbuf_enq(vdev, raw_num);
		}

		if (_isp_clk_dynamic_en(vdev, true) < 0)
			return;

		ispblk_post_yuv_cfg_update(ctx, raw_num);

		if (ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) //YUV sensor online mode
			goto YUV_POSTRAW;

		pre_be_tuning_update(&vdev->ctx, raw_num);

		//Update pre be size/ctrl flow
		_pre_be_ctrl_update(vdev, raw_num);
		//Update pre be sts size/addr
		_swap_pre_be_sts_buf(vdev, raw_num, ISP_BE_CH0);

		postraw_tuning_update(&vdev->ctx, raw_num);

		//Update postraw size/ctrl flow
		_post_ctrl_update(vdev, raw_num);
		//Update postraw dma size/addr
		_post_dma_update(vdev, raw_num);
		//Update postraw sts awb/dci/hist_edge_v dma size/addr
		_swap_post_sts_buf(ctx, raw_num);

YUV_POSTRAW:
		_vi_update_yuvout_config(vdev, raw_num);

		vdev->offline_raw_num = raw_num;

		ctx->cam_id = raw_num;

		isp_post_trig(ctx, raw_num);
	} else if (_is_all_online(ctx)) { //on-the-fly

		if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_RUNNING) {
			vi_pr(VI_DBG, "Postraw is running\n");
			return;
		}

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler online mode
			struct sc_cfg_cb post_para = {0};

			/* VI Online VPSS sc cb trigger */
			post_para.snr_num = raw_num;
			post_para.is_tile = false;
			vi_fill_mlv_info(NULL, raw_num, &post_para.m_lv_i, false);
			if (!(vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num]) &&
				_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
				vi_pr(VI_DBG, "snr_num_%d, SC is running\n", raw_num);
				atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
				return;
			}

			atomic_set(&vdev->ol_sc_frm_done, 0);
		}

		_vi_update_yuvout_config(vdev, raw_num);

		_vi_clear_mmap_fbc_ring_base(vdev, raw_num);

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw)
			_pre_hw_enque(vdev, raw_num, ISP_FE_CH0);

		vi_tuning_gamma_ips_update(ctx, raw_num);
		vi_tuning_clut_update(ctx, raw_num);
		vi_tuning_dci_update(ctx, raw_num);
	} else if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) {
		//Things have to be done in post done
		if (atomic_cmpxchg(&ctx->is_post_done, 1, 0) == 1) { //Change is_post_done flag to 0
			if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler offline mode
				if (!_postraw_outbuf_empty(vdev, raw_num))
					_postraw_outbuf_enq(vdev, raw_num);
			}

			_vi_clear_mmap_fbc_ring_base(vdev, raw_num);

			vi_tuning_gamma_ips_update(ctx, raw_num);
			vi_tuning_clut_update(ctx, raw_num);
			vi_tuning_dci_update(ctx, raw_num);
			vi_tuning_drc_update(ctx, raw_num);

			_vi_update_yuvout_config(vdev, raw_num);
		} else { //Things have to be done in be done for fps issue
			if (atomic_cmpxchg(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE, ISP_PRE_BE_RUNNING)
				!= ISP_PRE_BE_IDLE) {
				vi_pr(VI_DBG, "BE is running\n");
				return;
			}

			if (!ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //Scaler online mode
				struct sc_cfg_cb post_para = {0};

				/* VI Online VPSS sc cb trigger */
				post_para.snr_num = raw_num;
				post_para.is_tile = false;
				//if frm < bypass_frm, don't care vpss's state.
				if (!(vdev->postraw_frame_number[raw_num] < gViCtx->bypass_frm[raw_num])) {
					vi_fill_mlv_info(NULL, raw_num, &post_para.m_lv_i, false);
					if (_vi_call_cb(E_MODULE_VPSS, VPSS_CB_VI_ONLINE_TRIGGER, &post_para) != 0) {
						vi_pr(VI_DBG, "snr_num_%d, SC is not ready\n", raw_num);
						atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
						return;
					}
				}

				atomic_set(&vdev->ol_sc_frm_done, 0);
			}

			vdev->offline_raw_num = raw_num;
			ctx->cam_id = raw_num;

			if (gViCtx->bypass_frm[raw_num] - 1 == vdev->postraw_frame_number[raw_num]) {
				atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
				vi_pr(VI_DBG, "%d stop_trigger next\n", vdev->postraw_frame_number[raw_num]);
				return;
			}

			isp_post_trig(ctx, raw_num);
			vi_record_post_trigger(vdev, raw_num);

			if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw) {
				_pre_hw_enque(vdev, raw_num, ISP_FE_CH0);
				if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
					_pre_hw_enque(vdev, raw_num, ISP_FE_CH1);
			}
		}
	}
}

static void _pre_fe_rgbmap_update(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const enum cvi_isp_pre_chn_num chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint64_t rgbmap_buf = 0;
	u8 rgbmap_idx = 0;

	// In synthetic HDR mode, always used ch0's dma ctrl.
	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) {
		if (chn_num == ISP_FE_CH0) {
			rgbmap_idx = (vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1]) % RGBMAP_BUF_IDX;
			rgbmap_buf = isp_bufpool[raw_num].rgbmap_se[rgbmap_idx];
		} else if (chn_num == ISP_FE_CH1) {
			rgbmap_idx = (vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0]) % RGBMAP_BUF_IDX;
			rgbmap_buf = isp_bufpool[raw_num].rgbmap_le[rgbmap_idx];
		}

		if (raw_num == ISP_PRERAW_A)
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL10, rgbmap_buf);
		else if (raw_num == ISP_PRERAW_B)
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL16, rgbmap_buf);
		else
			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL20, rgbmap_buf);
	} else {
		rgbmap_idx = (vdev->pre_fe_frm_num[raw_num][chn_num]) % RGBMAP_BUF_IDX;

		if (chn_num == ISP_FE_CH0) {
			rgbmap_buf = isp_bufpool[raw_num].rgbmap_le[rgbmap_idx];

			if (raw_num == ISP_PRERAW_A)
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL10, rgbmap_buf);
			else if (raw_num == ISP_PRERAW_B)
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL16, rgbmap_buf);
			else
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL20, rgbmap_buf);
		} else if (chn_num == ISP_FE_CH1) {
			rgbmap_buf = isp_bufpool[raw_num].rgbmap_se[rgbmap_idx];

			if (raw_num == ISP_PRERAW_A)
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL11, rgbmap_buf);
			else if (raw_num == ISP_PRERAW_B)
				ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL17, rgbmap_buf);
		}
	}
}

void vi_destory_thread(struct cvi_vi_dev *vdev, enum E_VI_TH th_id)
{
	if (th_id < 0 || th_id >= E_VI_TH_MAX) {
		vi_pr(VI_ERR, "No such thread_id(%d)\n", th_id);
		return;
	}

	if (vdev->vi_th[th_id].w_thread != NULL) {
		int ret = 0;

		atomic_set(&vdev->vi_th[th_id].exit_flag, 1);
		vi_wake_up(&vdev->vi_th[th_id]);
		if (!ret) {
			while (atomic_read(&vdev->vi_th[th_id].thread_exit) == 0) {
				vi_pr(VI_WARN, "wait for %s exit\n", vdev->vi_th[th_id].th_name);
				usleep(10 * 1000);
			}
		}
		pthread_join(vdev->vi_th[th_id].w_thread, NULL);
		vdev->vi_th[th_id].w_thread = NULL;

		aos_event_free(&vdev->vi_th[th_id].event);
		pthread_mutex_destroy(&vdev->vi_th[th_id].lock);
		pthread_cond_destroy(&vdev->vi_th[th_id].cond);
	}
}

int vi_create_thread(struct cvi_vi_dev *vdev, enum E_VI_TH th_id)
{
	struct sched_param param;
	pthread_attr_t attr;
	pthread_condattr_t cattr;
	int rc = 0;

	if (th_id < 0 || th_id >= E_VI_TH_MAX) {
		vi_pr(VI_ERR, "No such thread_id(%d)\n", th_id);
		return -1;
	}

	param.sched_priority = VIP_RT_PRIO;

	if (vdev->vi_th[th_id].w_thread == NULL) {
		switch (th_id) {
		case E_VI_TH_PRERAW:
			memcpy(vdev->vi_th[th_id].th_name, "cvitask_isp_pre", sizeof("cvitask_isp_pre"));
			vdev->vi_th[th_id].th_handler = _vi_preraw_thread;
			break;
		case E_VI_TH_ERR_HANDLER:
			memcpy(vdev->vi_th[th_id].th_name, "cvitask_isp_err", sizeof("cvitask_isp_err"));
			vdev->vi_th[th_id].th_handler = _vi_err_handler_thread;
			break;
		case E_VI_TH_EVENT_HANDLER:
			memcpy(vdev->vi_th[th_id].th_name, "vi_event_handler", sizeof("vi_event_handler"));
			vdev->vi_th[th_id].th_handler = _vi_event_handler_thread;
			break;
		default:
			vi_pr(VI_ERR, "No such thread(%d)\n", th_id);
			return -1;
		}

		pthread_attr_init(&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		pthread_attr_setschedparam(&attr, &param);
		pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
		pthread_condattr_init(&cattr);
		pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
		aos_event_new(&vdev->vi_th[th_id].event, 0);
		//sem_init(&vdev->vi_th[th_id].sem, 0, 0);
		pthread_mutex_init(&vdev->vi_th[th_id].lock, NULL);
		pthread_cond_init(&vdev->vi_th[th_id].cond, &cattr);

		pthread_create(&vdev->vi_th[th_id].w_thread, &attr,
					vdev->vi_th[th_id].th_handler,
					(void *)vdev);
		pthread_setname_np(vdev->vi_th[th_id].w_thread, vdev->vi_th[th_id].th_name);

		if (!vdev->vi_th[th_id].w_thread) {
			vi_pr(VI_ERR, "Unable to start %s.\n", vdev->vi_th[th_id].th_name);
			rc = -1;
			goto err_pthread_create;
		}

		vdev->vi_th[th_id].flag = 0;
		atomic_set(&vdev->vi_th[th_id].thread_exit, 0);
		atomic_set(&vdev->vi_th[th_id].exit_flag, 0);
	}

	vi_pr(VI_DBG, "success to create %s\n", vdev->vi_th[th_id].th_name);
	return rc;

err_pthread_create:
	aos_event_free(&vdev->vi_th[th_id].event);
	//sem_destroy(&vdev->vi_th[th_id].sem);
	pthread_mutex_destroy(&vdev->vi_th[th_id].lock);
	pthread_cond_destroy(&vdev->vi_th[th_id].cond);
	return rc;
}

void _vi_sw_init(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	struct cvi_vi_ctx *pviProcCtx = NULL;
	pthread_condattr_t cattr;
	u8 i = 0, j = 0;

	pviProcCtx = (struct cvi_vi_ctx *)(vdev->shared_mem);

//TODO rawreplay timer
#if 0
	timer_setup(&usr_pic_timer.t, legacy_timer_emu_func, 0);
#endif

	ctx->is_offline_be		= false;
	ctx->is_offline_postraw		= true;
	ctx->is_3dnr_on			= true;
	ctx->is_tile			= false;
	ctx->is_dpcm_on			= false;
	ctx->is_work_on_r_tile		= false;
	ctx->is_hdr_on			= false;
	ctx->is_multi_sensor		= false;
	ctx->is_yuv_sensor		= false;
	ctx->is_sublvds_path		= false;
	ctx->is_fbc_on			= false;
	ctx->is_rgbir_sensor		= false;
	ctx->is_ctrl_inited		= false;
	ctx->is_slice_buf_on		= true;
	ctx->is_synthetic_hdr_on	= false;
	ctx->rgbmap_prebuf_idx		= 1;
	ctx->cam_id			= 0;
	ctx->rawb_chnstr_num		= 1;
	ctx->total_chn_num		= 0;
	ctx->gamma_tbl_idx		= 0;
	vdev->postraw_proc_num		= 0;
	vdev->usr_pic_delay		= 0;
	vdev->isp_source		= CVI_ISP_SOURCE_DEV;
	memset(vdev->usr_pic_phy_addr, 0, sizeof(vdev->usr_pic_phy_addr));

	if (pviProcCtx->vi_stt != VI_SUSPEND)
		memset(vdev->snr_info, 0, sizeof(struct cvi_isp_snr_info) * ISP_PRERAW_MAX);

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		for (j = 0; j < ISP_BE_CHN_MAX; j++) {
			vdev->pre_be_frm_num[i][j]		= 0;
		}
		vdev->preraw_first_frm[i]			= true;
		vdev->postraw_frame_number[i]			= 0;
		vdev->drop_frame_number[i]			= 0;
		vdev->dump_frame_number[i]			= 0;
		vdev->isp_int_flag[i]				= false;
		vdev->ctx.mmap_grid_size[i]			= 3;
		vdev->isp_err_times[i]				= 0;

		memset(&ctx->isp_pipe_cfg[i], 0, sizeof(struct _isp_cfg));

		if((stVIVPSSMode.aenMode[i] == VI_ONLINE_VPSS_ONLINE) ||
			(stVIVPSSMode.aenMode[i] == VI_OFFLINE_VPSS_ONLINE)) {
			ctx->isp_pipe_cfg[i].is_offline_scaler		= false;
		} else {
			ctx->isp_pipe_cfg[i].is_offline_scaler		= true;
		}

		for (j = 0; j < ISP_FE_CHN_MAX; j++) {
			vdev->pre_fe_sof_cnt[i][j]		= 0;
			vdev->pre_fe_frm_num[i][j]		= 0;

			atomic_set(&vdev->pre_fe_state[i][j], ISP_PRERAW_IDLE);
		}

		spin_lock_init(&snr_node_lock[i]);

		atomic_set(&vdev->isp_raw_dump_en[i], 0);
		atomic_set(&vdev->isp_smooth_raw_dump_en[i], 0);
	}

	for (i = 0; i < ISP_CHN_MAX; i++) {
		INIT_LIST_HEAD(&pre_out_queue[i].rdy_queue);
		INIT_LIST_HEAD(&pre_out_se_queue[i].rdy_queue);
		pre_out_queue[i].num_rdy       = 0;
		pre_out_queue[i].raw_num       = 0;
		pre_out_se_queue[i].num_rdy    = 0;
		pre_out_se_queue[i].raw_num    = 0;
	}

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		INIT_LIST_HEAD(&raw_dump_b_dq[i].rdy_queue);
		INIT_LIST_HEAD(&raw_dump_b_se_dq[i].rdy_queue);
		raw_dump_b_dq[i].num_rdy          = 0;
		raw_dump_b_dq[i].raw_num          = i;
		raw_dump_b_se_dq[i].num_rdy       = 0;
		raw_dump_b_se_dq[i].raw_num       = i;

		INIT_LIST_HEAD(&raw_dump_b_q[i].rdy_queue);
		INIT_LIST_HEAD(&raw_dump_b_se_q[i].rdy_queue);
		raw_dump_b_q[i].num_rdy          = 0;
		raw_dump_b_q[i].raw_num          = i;
		raw_dump_b_se_q[i].num_rdy       = 0;
		raw_dump_b_se_q[i].raw_num       = i;

		INIT_LIST_HEAD(&isp_snr_i2c_queue[i].list);
		isp_snr_i2c_queue[i].num_rdy     = 0;

		INIT_LIST_HEAD(&pre_be_in_se_q[i].rdy_queue);
		pre_be_in_se_q[i].num_rdy        = 0;

		INIT_LIST_HEAD(&pre_be_in_se_syn_q[i].rdy_queue);
		pre_be_in_se_syn_q[i].num_rdy    = 0;
	}

	INIT_LIST_HEAD(&pre_raw_num_q.list);
	pre_raw_num_q.num_rdy	= 0;
	INIT_LIST_HEAD(&dqbuf_q.list);
	INIT_LIST_HEAD(&event_q.list);

	INIT_LIST_HEAD(&pre_be_in_q.rdy_queue);
	INIT_LIST_HEAD(&pre_be_in_syn_q.rdy_queue);
	INIT_LIST_HEAD(&pre_be_out_q.rdy_queue);
	INIT_LIST_HEAD(&pre_be_out_se_q.rdy_queue);
	pre_be_in_q.num_rdy	= 0;
	pre_be_in_syn_q.num_rdy	= 0;
	pre_be_out_q.num_rdy	= 0;
	pre_be_out_se_q.num_rdy	= 0;

	INIT_LIST_HEAD(&post_in_queue.rdy_queue);
	INIT_LIST_HEAD(&post_in_se_queue.rdy_queue);
	post_in_queue.num_rdy	 = 0;
	post_in_se_queue.num_rdy = 0;

	atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRERAW_IDLE);
	atomic_set(&vdev->pre_be_state[ISP_BE_CH1], ISP_PRERAW_IDLE);
	atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
	atomic_set(&vdev->isp_streamoff, 0);
	atomic_set(&vdev->isp_err_handle_flag, 0);
	atomic_set(&vdev->ol_sc_frm_done, 1);
	atomic_set(&vdev->isp_dbg_flag, 0);
	atomic_set(&vdev->ctx.is_post_done, 0);

	spin_lock_init(&buf_lock);
	spin_lock_init(&raw_num_lock);
	spin_lock_init(&dq_lock);
	spin_lock_init(&event_lock);
	spin_lock_init(&vdev->qbuf_lock);
	spin_lock_init(&stream_lock);

	pthread_condattr_init(&cattr);
	pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
	aos_event_new(&vdev->isp_event_wait_q.event, 0);
	//sem_init(&vdev->isp_event_wait_q.sem, 0, 0);
	pthread_mutex_init(&vdev->isp_event_wait_q.lock, NULL);
	pthread_cond_init(&vdev->isp_event_wait_q.cond, &cattr);
	aos_event_new(&vdev->isp_dbg_wait_q.event, 0);
//TODO need mw work
#if 0
	init_waitqueue_head(&vdev->isp_dq_wait_q);
	init_waitqueue_head(&vdev->isp_dbg_wait_q);
#endif
	vi_tuning_sw_init();
}

static void _vi_init_param(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint8_t i = 0;
	struct sched_param param;
	pthread_attr_t attr;
	pthread_condattr_t cattr;

	atomic_set(&dev_open_cnt, 0);

	memset(ctx, 0, sizeof(*ctx));

	ctx->phys_regs		= isp_get_phys_reg_bases();
	ctx->cam_id		= 0;

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		ctx->rgb_color_mode[i]				= ISP_BAYER_TYPE_GB;
		ctx->isp_pipe_cfg[i].is_patgen_en		= false;
		ctx->isp_pipe_cfg[i].is_offline_preraw		= false;
		ctx->isp_pipe_cfg[i].is_yuv_bypass_path		= false;
		ctx->isp_pipe_cfg[i].is_drop_next_frame		= false;
		ctx->isp_pipe_cfg[i].is_synthetic_hdr_on	= false;
		ctx->isp_pipe_cfg[i].isp_reset_frm		= 0;
		ctx->isp_pipe_cfg[i].is_422_to_420		= false;
		ctx->isp_pipe_cfg[i].max_height			= 0;
		ctx->isp_pipe_cfg[i].max_width			= 0;
		ctx->isp_pipe_cfg[i].csibdg_width		= 0;
		ctx->isp_pipe_cfg[i].csibdg_height		= 0;

		INIT_LIST_HEAD(&raw_dump_b_dq[i].rdy_queue);
		INIT_LIST_HEAD(&raw_dump_b_se_dq[i].rdy_queue);
		raw_dump_b_dq[i].num_rdy          = 0;
		raw_dump_b_dq[i].raw_num          = i;
		raw_dump_b_se_dq[i].num_rdy       = 0;
		raw_dump_b_se_dq[i].raw_num       = i;

		INIT_LIST_HEAD(&raw_dump_b_q[i].rdy_queue);
		INIT_LIST_HEAD(&raw_dump_b_se_q[i].rdy_queue);
		raw_dump_b_q[i].num_rdy          = 0;
		raw_dump_b_q[i].raw_num          = i;
		raw_dump_b_se_q[i].num_rdy       = 0;
		raw_dump_b_se_q[i].raw_num       = i;

		INIT_LIST_HEAD(&isp_snr_i2c_queue[i].list);
		isp_snr_i2c_queue[i].num_rdy     = 0;
		INIT_LIST_HEAD(&pre_be_in_se_q[i].rdy_queue);
		pre_be_in_se_q[i].num_rdy        = 0;
	}

	INIT_LIST_HEAD(&pre_raw_num_q.list);
	pre_raw_num_q.num_rdy = 0;

	memset(&vdev->usr_crop, 0, sizeof(vdev->usr_crop));

	for (i = 0; i < ISP_FE_CHN_MAX; i++) {
		INIT_LIST_HEAD(&vdev->qbuf_list[i]);
		vdev->qbuf_num[i] = 0;
	}

	pthread_condattr_init(&cattr);
	pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		vdev->isp_int_flag[i] = false;
		pthread_mutex_init(&vdev->isp_int_wait_q[i].lock, NULL);
		pthread_cond_init(&vdev->isp_int_wait_q[i].cond, &cattr);
		aos_event_new(&vdev->isp_int_wait_q[i].event, 0);
	}
#if 0
	//ToDo sync_task_ext
	for (i = 0; i < ISP_PRERAW_MAX; i++)
		sync_task_init(i);
#endif
	param.sched_priority = VIP_RT_PRIO;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

	if (!vdev->job_work.w_thread) {
		//sem_init(&vdev->job_work.sem, 0, 0);
		aos_event_new(&vdev->job_work.event, 0);
		pthread_mutex_init(&vdev->job_work.lock, NULL);
		pthread_cond_init(&vdev->job_work.cond, &cattr);
		pthread_create(&vdev->job_work.w_thread, &attr, _isp_post_thread, (void *)vdev);
		pthread_setname_np(vdev->job_work.w_thread, "_isp_post_thread");
	}

	atomic_set(&vdev->isp_streamon, 0);
}

static int _vi_mempool_setup(void)
{
	int ret = 0;

	_vi_mempool_reset();

	return ret;
}

static void clk_enable_disable(struct vi_clk *hw, int enable)
{
//TODO need ccf
#if 0
	u32 reg;

	reg = _reg_read(CLK_REG_BASE + hw->gate.reg);

	if (enable)
		reg |= BIT(hw->gate.shift);
	else
		reg &= ~BIT(hw->gate.shift);

	_reg_write(CLK_REG_BASE + hw->gate.reg, reg);
#endif
}

int vi_mac_clk_ctrl(struct cvi_vi_dev *vdev, u8 mac_num, u8 enable)
{
	int rc = 0;

	if (mac_num >= ARRAY_SIZE(clk_mac))
		return rc;

	clk_enable_disable(&clk_mac[mac_num], enable);

	return rc;
}


int _vi_clk_ctrl(struct cvi_vi_dev *vdev, u8 enable)
{
	u8 i = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(clk_sys); ++i) {
		clk_enable_disable(&clk_sys[i], enable);
	}

	for (i = 0; i < ARRAY_SIZE(clk_isp); ++i) {
		clk_enable_disable(&clk_isp[i], enable);
	}

	//Set axi_isp_top_clk_en 1
	vip_sys_reg_write_mask(0x4, BIT(0), BIT(0));

	return rc;
}

void _vi_sdk_release(struct cvi_vi_dev *vdev)
{
	u8 i = 0;

	vi_disable_chn(0);

	for (i = 0; i < VI_MAX_CHN_NUM; i++) {
		memset(&gViCtx->chnAttr[i], 0, sizeof(VI_CHN_ATTR_S));
		memset(&gViCtx->chnStatus[i], 0, sizeof(VI_CHN_STATUS_S));
		gViCtx->blk_size[i] = 0;
		gViCtx->bypass_frm[i] = 0;
	}

	for (i = 0; i < VI_MAX_PIPE_NUM; i++)
		gViCtx->isPipeCreated[i] = false;

	for (i = 0; i < VI_MAX_DEV_NUM; i++)
		gViCtx->isDevEnable[i] = false;

	pthread_mutex_destroy(&vdev->isp_event_wait_q.lock);
	pthread_cond_destroy(&vdev->isp_event_wait_q.cond);
	aos_event_free(&vdev->isp_event_wait_q.event);
	//sem_destroy(&vdev->job_work.sem);
	aos_event_free(&vdev->isp_dbg_wait_q.event);
}

static void _vi_release_op(struct cvi_vi_dev *vdev)
{

	u8 i = 0;

	_vi_clk_ctrl(vdev, false);

	for (i = 0; i < gViCtx->total_dev_num; i++) {
		vi_mac_clk_ctrl(vdev, i, false);
	}
}

static int _vi_create_proc(struct cvi_vi_dev *vdev)
{
	int ret = 0;

	/* vi proc setup */
	vdev->shared_mem = calloc(VI_SHARE_MEM_SIZE, 1);
	if (!vdev->shared_mem) {
		vi_pr(VI_ERR, "shared_mem alloc size(%d) failed\n", VI_SHARE_MEM_SIZE);
		return -ENOMEM;
	}

	if (vi_proc_init(vdev, vdev->shared_mem) < 0) {
		vi_pr(VI_ERR, "vi proc init failed\n");
		return -EAGAIN;
	}

	if (vi_dbg_proc_init(vdev) < 0) {
		vi_pr(VI_ERR, "vi_dbg proc init failed\n");
		return -EAGAIN;
	}
//TODO maybe use vfs
#if 0
	if (isp_proc_init(vdev) < 0) {
		vi_pr(VI_ERR, "isp proc init failed\n");
		return -EAGAIN;
	}
#endif
	return ret;
}

//TODO isp proc
static void _vi_destroy_proc(struct cvi_vi_dev *vdev)
{
	vi_proc_remove();
	vi_dbg_proc_remove();

	free(vdev->shared_mem);
	vdev->shared_mem = NULL;
#if 0
	isp_proc_remove();
#endif
}

/*******************************************************
 *  File operations for core
 ******************************************************/
static long _vi_s_ctrl(struct cvi_vi_dev *vdev, struct vi_ext_control *p)
{
	u32 id = p->id;
	long rc = -EINVAL;
	struct isp_ctx *ctx = &vdev->ctx;

	switch (id) {
	case VI_IOCTL_SDK_CTRL:
	{
		rc = vi_sdk_ctrl(vdev, p);
		break;
	}
	case VI_IOCTL_HDR:
	{
#if defined(__CV180X__)
		if (p->value == true) {
			vi_pr(VI_ERR, "only support linear mode.\n");
			break;
		}
#endif
		ctx->is_hdr_on = p->value;
		ctx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on = p->value;
		vi_pr(VI_INFO, "HDR_ON(%d) for test\n", ctx->is_hdr_on);
		rc = 0;
		break;
	}
	case VI_IOCTL_HDR_DETAIL_EN:
	{
		u32 val = 0, snr_num = 0, enable = 0;

		val = p->value;
		snr_num = val & 0x3; //bit0~1: snr_num
		enable = val & 0x4; //bit2: enable/disable

		if (snr_num < ISP_PRERAW_MAX) {
			ctx->isp_pipe_cfg[snr_num].is_hdr_detail_en = enable;
			vi_pr(VI_INFO, "HDR_DETAIL_EN(%d)\n",
				ctx->isp_pipe_cfg[snr_num].is_hdr_detail_en);
			rc = 0;
		}

		break;
	}

	case VI_IOCTL_3DNR:
		ctx->is_3dnr_on = p->value;
		vi_pr(VI_INFO, "is_3dnr_on=%d\n", ctx->is_3dnr_on);
		rc = 0;
		break;

	case VI_IOCTL_TILE:
		ctx->is_tile = p->value;
		vi_pr(VI_INFO, "TILE_ON(%d)\n", ctx->is_tile);
		rc = 0;
		break;

	case VI_IOCTL_COMPRESS_EN:
		ctx->is_dpcm_on = p->value;
		vi_pr(VI_INFO, "ISP_COMPRESS_ON(%d)\n", ctx->is_dpcm_on);
		rc = 0;
		break;

	case VI_IOCTL_CLK_CTRL:
	{
		u8 i = 0;

		_vi_clk_ctrl(vdev, true);
		for (i = 0; i < gViCtx->total_dev_num; i++) {
			vi_mac_clk_ctrl(vdev, i, true);
		}
		rc = 0;
		break;
	}
	case VI_IOCTL_STS_PUT:
	{
		u8 raw_num = 0;
		unsigned long flags;

		raw_num = p->value;

		if (raw_num >= ISP_PRERAW_MAX)
			break;

		spin_lock_irqsave(&isp_bufpool[raw_num].pre_be_sts_lock, flags);
		isp_bufpool[raw_num].pre_be_sts_in_use = 0;
		spin_unlock_irqrestore(&isp_bufpool[raw_num].pre_be_sts_lock, flags);

		rc = 0;
		break;
	}

	case VI_IOCTL_POST_STS_PUT:
	{
		u8 raw_num = 0;
		unsigned long flags;

		raw_num = p->value;

		if (raw_num >= ISP_PRERAW_MAX)
			break;

		spin_lock_irqsave(&isp_bufpool[raw_num].post_sts_lock, flags);
		isp_bufpool[raw_num].post_sts_in_use = 0;
		spin_unlock_irqrestore(&isp_bufpool[raw_num].post_sts_lock, flags);

		rc = 0;
		break;
	}

	case VI_IOCTL_USR_PIC_CFG:
	{
		struct cvi_isp_usr_pic_cfg cfg;

		memcpy(&cfg, p->ptr, sizeof(struct cvi_isp_usr_pic_cfg));

		if ((cfg.crop.width < 32) || (cfg.crop.width > 4096)
			|| (cfg.crop.left > cfg.crop.width) || (cfg.crop.top > cfg.crop.height)) {
			vi_pr(VI_ERR, "USR_PIC_CFG:(Invalid Param) w(%d) h(%d) x(%d) y(%d)",
				cfg.crop.width, cfg.crop.height, cfg.crop.left, cfg.crop.top);
		} else {
			vdev->usr_fmt = cfg.fmt;
			vdev->usr_crop = cfg.crop;

			vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].csibdg_width	= vdev->usr_fmt.width;
			vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].csibdg_height	= vdev->usr_fmt.height;
			vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].max_width		= vdev->usr_fmt.width;
			vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].max_height		= vdev->usr_fmt.height;

			rc = 0;
		}

		break;
	}

	case VI_IOCTL_USR_PIC_ONOFF:
	{
		vdev->isp_source = p->value;
		ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw =
			(vdev->isp_source == CVI_ISP_SOURCE_FE);

		vi_pr(VI_INFO, "vdev->isp_source=%d\n", vdev->isp_source);
		vi_pr(VI_INFO, "ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw=%d\n",
			ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw);

		rc = 0;
		break;
	}

	case VI_IOCTL_PUT_PIPE_DUMP:
	{
		u32 raw_num = 0;

		raw_num = p->value;

		if (isp_byr[raw_num]) {
			free(isp_byr[raw_num]);
			isp_byr[raw_num] = NULL;
		}

		if (isp_byr_se[raw_num]) {
			free(isp_byr_se[raw_num]);
			isp_byr_se[raw_num] = NULL;
		}

		rc = 0;
		break;
	}

	case VI_IOCTL_USR_PIC_PUT:
	{
		if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
#if 1
			u64 phy_addr = p->value64;

			ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA_CTL4, phy_addr);
			vdev->usr_pic_phy_addr[0] = phy_addr;
			vi_pr(VI_INFO, "\nvdev->usr_pic_phy_addr(0x%llx)\n", vdev->usr_pic_phy_addr[0]);
			rc = 0;

			if (vdev->usr_pic_delay)
				usr_pic_timer_init(vdev);
#else //for vip_FPGA test
			uint64_t bufaddr = 0;
			uint32_t bufsize = 0;

			bufaddr = _mempool_get_addr();
			bufsize = ispblk_dma_config(ctx, ISP_BLK_ID_RDMA0, bufaddr);
			_mempool_pop(bufsize);

			vi_pr(VI_WARN, "\nRDMA0 base_addr=0x%x\n", bufaddr);

			vdev->usr_pic_phy_addr = bufaddr;
			rc = 0;
#endif
		}
		break;
	}

	case VI_IOCTL_USR_PIC_TIMING:
	{
//useless rawreplay
#if 0
		if (p->value > 30)
			vdev->usr_pic_delay = msecs_to_jiffies(33);
		else if (p->value > 0)
			vdev->usr_pic_delay = msecs_to_jiffies(1000 / p->value);
		else
			vdev->usr_pic_delay = 0;

		if (!vdev->usr_pic_delay)
			usr_pic_time_remove();
#endif
		rc = 0;
		break;
	}

	case VI_IOCTL_ONLINE:
		ctx->is_offline_postraw = !p->value;
		vi_pr(VI_INFO, "is_offline_postraw=%d\n", ctx->is_offline_postraw);
		rc = 0;
		break;

	case VI_IOCTL_BE_ONLINE:
		ctx->is_offline_be = !p->value;
		vi_pr(VI_INFO, "is_offline_be=%d\n", ctx->is_offline_be);
		rc = 0;
		break;

	case VI_IOCTL_SET_SNR_CFG_NODE:
	{
		struct cvi_isp_snr_update *snr_update;
		u8			  raw_num;

		if (vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw)
			break;

		snr_update = malloc(sizeof(struct cvi_isp_snr_update));
		memcpy(snr_update, p->ptr, sizeof(struct cvi_isp_snr_update));

		raw_num = snr_update->raw_num;

		if (raw_num >= ISP_PRERAW_MAX) {
			free(snr_update);
			break;
		}

		if (vdev->ctx.isp_pipe_cfg[raw_num].is_offline_preraw ||
			vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en) {
			rc = 0;
			free(snr_update);
			break;
		}

		vi_pr(VI_DBG, "raw_num=%d, magic_num=%d, regs_num=%d, i2c_update=%d, isp_update=%d\n",
			raw_num,
			snr_update->snr_cfg_node.snsr.magic_num,
			snr_update->snr_cfg_node.snsr.regs_num,
			snr_update->snr_cfg_node.snsr.need_update,
			snr_update->snr_cfg_node.isp.need_update);

		_isp_snr_cfg_enq(snr_update, raw_num);

		free(snr_update);

		rc = 0;
		break;
	}

	case VI_IOCTL_SET_SNR_INFO:
	{
		struct cvi_isp_snr_info snr_info;

		memcpy(&snr_info, p->ptr, sizeof(struct cvi_isp_snr_info));
#if defined(__CV180X__)
		if (snr_info.raw_num >= ISP_PRERAW_B) {
			vi_pr(VI_ERR, "only support single sensor.\n");
			break;
		}
#endif
		if (snr_info.raw_num == ISP_PRERAW_B)
			viproc_en[1] = 1;
		memcpy(&vdev->snr_info[snr_info.raw_num], p->ptr, sizeof(struct cvi_isp_snr_info));
		vi_pr(VI_INFO, "raw_num=%d, color_mode=%d, frm_num=%d, snr_w:h=%d:%d, active_w:h=%d:%d\n",
			snr_info.raw_num,
			vdev->snr_info[snr_info.raw_num].color_mode,
			vdev->snr_info[snr_info.raw_num].snr_fmt.frm_num,
			vdev->snr_info[snr_info.raw_num].snr_fmt.img_size[0].width,
			vdev->snr_info[snr_info.raw_num].snr_fmt.img_size[0].height,
			vdev->snr_info[snr_info.raw_num].snr_fmt.img_size[0].active_w,
			vdev->snr_info[snr_info.raw_num].snr_fmt.img_size[0].active_h);

		rc = 0;
		break;
	}

	case VI_IOCTL_MMAP_GRID_SIZE:
	{
		struct cvi_isp_mmap_grid_size m_gd_sz;

		memcpy(&m_gd_sz, p->ptr, sizeof(struct cvi_isp_mmap_grid_size));

		m_gd_sz.grid_size = ctx->mmap_grid_size[m_gd_sz.raw_num];

		memcpy(p->ptr, &m_gd_sz, sizeof(struct cvi_isp_mmap_grid_size));

		rc = 0;
		break;
	}

	case VI_IOCTL_SET_PROC_CONTENT:
	{
		struct isp_proc_cfg proc_cfg;

		memcpy(&proc_cfg, p->ptr, sizeof(struct isp_proc_cfg));
		if (proc_cfg.buffer_size == 0)
			break;
//TODO VI_IOCTL_SET_PROC_CONTENT
#if 0
		isp_proc_setProcContent(proc_cfg.buffer, proc_cfg.buffer_size);
#endif
		rc = 0;
		break;
	}

	case VI_IOCTL_SC_ONLINE:
	{
		struct cvi_isp_sc_online sc_online;

		memcpy(&sc_online, p->ptr, sizeof(struct cvi_isp_sc_online));

		//Currently both sensor are needed to be online or offline at same time.
		ctx->isp_pipe_cfg[sc_online.raw_num].is_offline_scaler = !sc_online.is_sc_online;
		vi_pr(VI_WARN, "raw_num_%d set is_offline_scaler:%d\n",
				  sc_online.raw_num, !sc_online.is_sc_online);
		rc = 0;
		break;
	}

	case VI_IOCTL_AWB_STS_PUT:
	{
		rc = 0;
		break;
	}

	case VI_IOCTL_ENQ_BUF:
	{
		//TODO VI_IOCTL_ENQ_BUF
		struct vi_buffer    buf;
		struct cvi_isp_buf2 *qbuf;
		u8 pre_trig = false, post_trig = false;

		memcpy(&buf, p->ptr, sizeof(buf));

		qbuf = calloc(sizeof(struct cvi_isp_buf2), 1);
		if (qbuf == NULL) {
			vi_pr(VI_ERR, "QBUF kzalloc size(%lu) fail\n", sizeof(struct cvi_isp_buf2));
			rc = -ENOMEM;
			break;
		}

		vdev->chn_id = buf.index;
		memcpy(&qbuf->buf, &buf, sizeof(buf));

		if (_is_all_online(ctx) &&
			cvi_isp_rdy_buf_empty2(vdev, ISP_PRERAW_A) &&
			vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0] > 0) {
			pre_trig = true;
		} else if (_is_fe_be_online(ctx)) { //fe->be->dram->post
			if (cvi_isp_rdy_buf_empty2(vdev, vdev->chn_id) &&
				vdev->postraw_frame_number[ISP_PRERAW_A] > 0) {
				vi_pr(VI_DBG, "chn_%d buf empty, trigger post\n", vdev->chn_id);
				post_trig = true;
			}
		}

		cvi_isp_buf_queue2(vdev, qbuf);

		if (pre_trig || post_trig)
			//tasklet_hi_schedule(&vdev->job_work);

		rc = 0;
		break;
	}

	case VI_IOCTL_SET_DMA_BUF_INFO:
	{
		struct cvi_vi_dma_buf_info info;

		memcpy(&info, p->ptr, sizeof(struct cvi_vi_dma_buf_info));
		if ((info.size == 0) || (info.paddr == 0))
			break;

		isp_mempool.base = info.paddr;
		isp_mempool.size = info.size;

		vi_pr(VI_INFO, "ISP dma buf paddr(0x%"PRIx64") size=0x%x\n",
				isp_mempool.base, isp_mempool.size);

		rc = 0;
		break;
	}

	case VI_IOCTL_START_STREAMING:
	{
		if (vi_start_streaming(vdev)) {
			vi_pr(VI_ERR, "Failed to vi start streaming\n");
			break;
		}

		atomic_set(&vdev->isp_streamon, 1);

		rc = 0;
		break;
	}

	case VI_IOCTL_STOP_STREAMING:
	{
		if (vi_stop_streaming(vdev)) {
			vi_pr(VI_ERR, "Failed to vi stop streaming\n");
			break;
		}

		atomic_set(&vdev->isp_streamon, 0);

		rc = 0;
		break;
	}

	case VI_IOCTL_SET_SLICE_BUF_EN:
	{
		ctx->is_slice_buf_on = p->value;
		vi_pr(VI_INFO, "ISP_SLICE_BUF_ON(%d)\n", ctx->is_slice_buf_on);
		rc = 0;
		break;
	}
	case VI_IOCTL_ENABLE_PATTERN:
	{
		u8 raw_num = ISP_PRERAW_A;

		for (raw_num = 0; raw_num < ISP_PRERAW_MAX; raw_num++) {
			csi_patgen_en[raw_num] = 1;
		}

		vi_pr(VI_WARN, "csi_patgen_en=[%d,%d]\n", csi_patgen_en[ISP_PRERAW_A], csi_patgen_en[ISP_PRERAW_B]);
		rc = 0;
		break;
	}
	case VI_IOCTL_TUNING_PARAM:
	{
		memcpy(&tuning_dis, p->ptr, sizeof(tuning_dis));
		vi_pr(VI_INFO, "tuning_dis=[%d,%d,%d,%d]\n", tuning_dis[0], tuning_dis[1]
				, tuning_dis[2], tuning_dis[3]);
		rc = 0;
		break;
	}
	default:
		break;
	}

	return rc;
}

static long _vi_g_ctrl(struct cvi_vi_dev *vdev, struct vi_ext_control *p)
{
	u32 id = p->id;
	long rc = -EINVAL;
	struct isp_ctx *ctx = &vdev->ctx;

	switch (id) {

	case VI_IOCTL_STS_GET:
	{
		u8 raw_num;
		unsigned long flags;

		raw_num = p->value;

		if (raw_num >= ISP_PRERAW_MAX)
			break;

		spin_lock_irqsave(&isp_bufpool[raw_num].pre_be_sts_lock, flags);
		isp_bufpool[raw_num].pre_be_sts_in_use = 1;
		p->value = isp_bufpool[raw_num].pre_be_sts_busy_idx ^ 1;
		spin_unlock_irqrestore(&isp_bufpool[raw_num].pre_be_sts_lock, flags);

		rc = 0;
		break;
	}

	case VI_IOCTL_POST_STS_GET:
	{
		u8 raw_num;
		unsigned long flags;

		raw_num = p->value;

		if (raw_num >= ISP_PRERAW_MAX)
			break;

		spin_lock_irqsave(&isp_bufpool[raw_num].post_sts_lock, flags);
		isp_bufpool[raw_num].post_sts_in_use = 1;
		p->value = isp_bufpool[raw_num].post_sts_busy_idx ^ 1;
		spin_unlock_irqrestore(&isp_bufpool[raw_num].post_sts_lock, flags);

		rc = 0;
		break;
	}

	case VI_IOCTL_STS_MEM:
	{
		struct cvi_isp_sts_mem sts_mem;
		u8 raw_num = 0;

		memcpy(&sts_mem, p->ptr, sizeof(struct cvi_isp_sts_mem));

		raw_num = sts_mem.raw_num;
		if (raw_num >= ISP_PRERAW_MAX) {
			vi_pr(VI_ERR, "sts_mem wrong raw_num(%d)\n", raw_num);
			break;
		}

#if 0//PORTING_TEST //test only
		isp_bufpool[raw_num].sts_mem[0].ae_le.phy_addr = 0x11223344;
		isp_bufpool[raw_num].sts_mem[0].ae_le.size = 44800;
		isp_bufpool[raw_num].sts_mem[0].af.phy_addr = 0xaabbccdd;
		isp_bufpool[raw_num].sts_mem[0].af.size = 16320;
		isp_bufpool[raw_num].sts_mem[0].awb.phy_addr = 0x12345678;
		isp_bufpool[raw_num].sts_mem[0].awb.size = 71808;
#endif
		memcpy(p->ptr, isp_bufpool[raw_num].sts_mem,
				sizeof(struct cvi_isp_sts_mem) * 2);

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_LSC_PHY_BUF:
	{
		struct cvi_vip_memblock *isp_mem;

		isp_mem = malloc(sizeof(struct cvi_vip_memblock));
		memcpy(isp_mem, p->ptr, sizeof(struct cvi_vip_memblock));

		isp_mem->phy_addr = isp_bufpool[isp_mem->raw_num].lsc;
		isp_mem->size = ispblk_dma_config(ctx, ISP_BLK_ID_DMA_CTL24, 0);

		memcpy(p->ptr, isp_mem, sizeof(struct cvi_vip_memblock));

		free(isp_mem);

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_PIPE_DUMP:
	{
		struct cvi_vip_isp_raw_blk dump[2];

		memcpy(&dump[0], p->ptr, sizeof(struct cvi_vip_isp_raw_blk) * 2);

#if 0//PORTING_TEST //test only
		dump[0].raw_dump.phy_addr = 0x11223344;
		if (copy_to_user(p->ptr, &dump[0], sizeof(struct cvi_vip_isp_raw_blk) * 2) != 0)
			break;
		rc = 0;
#else
		rc = isp_raw_dump(vdev, &dump[0]);
		memcpy(p->ptr, &dump[0], sizeof(struct cvi_vip_isp_raw_blk) * 2);
#endif
		break;
	}

	case VI_IOCTL_AWB_STS_GET:
	{
		rc = 0;
		break;
	}

	case VI_IOCTL_GET_FSWDR_PHY_BUF:
	{
		struct cvi_vip_memblock *isp_mem;

		isp_mem = malloc(sizeof(struct cvi_vip_memblock));
		if (!isp_mem) {
			free(isp_mem);
			break;
		}

		memcpy(isp_mem, p->ptr, sizeof(struct cvi_vip_memblock));

		isp_mem->size = sizeof(struct cvi_vip_isp_fswdr_report);
		if (isp_bufpool[isp_mem->raw_num].fswdr_rpt == NULL) {
			isp_bufpool[isp_mem->raw_num].fswdr_rpt = malloc(isp_mem->size);
			if (isp_bufpool[isp_mem->raw_num].fswdr_rpt == NULL) {
				vi_pr(VI_ERR, "isp_bufpool[%d].fswdr_rpt alloc size(%d) fail\n",
					isp_mem->raw_num, isp_mem->size);
				free(isp_mem);
				break;
			}
		}
		isp_mem->vir_addr = isp_bufpool[isp_mem->raw_num].fswdr_rpt;
		isp_mem->phy_addr = (u64)(isp_bufpool[isp_mem->raw_num].fswdr_rpt);

		memcpy(p->ptr, isp_mem, sizeof(struct cvi_vip_memblock));
		free(isp_mem);

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_SCENE_INFO:
	{
		enum ISP_SCENE_INFO info = FE_ON_BE_OFF_POST_ON_SC;

		if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_scaler) {
			if (_is_fe_be_online(ctx))
				info = FE_ON_BE_OFF_POST_OFF_SC;
			else if (_is_be_post_online(ctx))
				info = FE_OFF_BE_ON_POST_OFF_SC;
			else if (_is_all_online(ctx))
				info = FE_ON_BE_ON_POST_OFF_SC;
		} else {
			if (_is_fe_be_online(ctx))
				info = FE_ON_BE_OFF_POST_ON_SC;
			else if (_is_be_post_online(ctx))
				info = FE_OFF_BE_ON_POST_ON_SC;
			else if (_is_all_online(ctx))
				info = FE_ON_BE_ON_POST_ON_SC;
		}

		p->value = info;

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_BUF_SIZE:
	{
		u32 tmp_size = 0;
		enum cvi_isp_raw raw_max = ISP_PRERAW_MAX - 1;
		u8 raw_num = 0;

		tmp_size = isp_mempool.size;
		isp_mempool.base = 0xabde2000; //tmp addr only for check aligment
		isp_mempool.size = 0x8000000;
		isp_mempool.byteused = 0;

		_vi_scene_ctrl(vdev, &raw_max);

		for (raw_num = ISP_PRERAW_A; raw_num < raw_max; raw_num++) {
			vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en = csi_patgen_en[raw_num];

			if (vdev->ctx.isp_pipe_cfg[raw_num].is_patgen_en) {
#ifndef FPGA_TEST
				vdev->usr_fmt.width = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w;
				vdev->usr_fmt.height = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_h;
				vdev->usr_crop.width = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_w;
				vdev->usr_crop.height = vdev->snr_info[raw_num].snr_fmt.img_size[0].active_h;
#else
				vdev->usr_fmt.width = 1920;
				vdev->usr_fmt.height = 1080;
				vdev->usr_crop.width = 1920;
				vdev->usr_crop.height = 1080;
#endif
				vdev->usr_fmt.code = ISP_BAYER_TYPE_BG;
				vdev->usr_crop.left = 0;
				vdev->usr_crop.top = 0;

				vi_pr(VI_WARN, "patgen enable, w_h(%d:%d), color mode(%d)\n",
						vdev->usr_fmt.width, vdev->usr_fmt.height, vdev->usr_fmt.code);
			}

			_vi_ctrl_init(raw_num, vdev);
		}

		_vi_get_dma_buf_size(ctx, raw_max);

		p->value = isp_mempool.byteused;

		vi_pr(VI_INFO, "isp_mempool need size(0x%x)\n",  isp_mempool.byteused);

		isp_mempool.base	= 0;
		isp_mempool.size	= tmp_size;
		isp_mempool.byteused	= 0;

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_TUN_ADDR:
	{
		void *tun_addr = NULL;
		u32 size;
		u8 i;

		for (i = 0; i < gViCtx->total_dev_num; i++) {
			if (!ctx->isp_pipe_cfg[i].is_yuv_bypass_path)
				vi_tuning_buf_setup(i);
		}

		tun_addr = vi_get_tuning_buf_addr(&size);

		memcpy(p->ptr, tun_addr, size);

		rc = 0;
		break;
	}

	case VI_IOCTL_CHECK_ISP_EVENT:
	{
		if (!list_empty(&event_q.list))
			rc = 0;
		break;
	}

	case VI_IOCTL_QUERY_ISP_EVENT:
	{
		struct cvi_isp_attr isp_attr;

		isp_attr.cond = &vdev->isp_event_wait_q.cond;
		isp_attr.lock = &vdev->isp_event_wait_q.lock;
		isp_attr.event = &vdev->isp_event_wait_q.event;
		memcpy(p->ptr, &isp_attr, sizeof(struct cvi_isp_attr));

		rc = 0;
		break;
	}

	case VI_IOCTL_DQEVENT:
	{
		struct vi_event ev_u = {.type = VI_EVENT_MAX};
		struct vi_event_k *ev_k;
		unsigned long flags;

		spin_lock_irqsave(&event_lock, flags);
#if 0//PORTING_TEST //test only
		struct vi_event_k *ev_test;
		static u32 frm_num, type;

		ev_test = kzalloc(sizeof(*ev_test), GFP_ATOMIC);

		ev_test->ev.dev_id = 0;
		ev_test->ev.type = type++ % (VI_EVENT_MAX - 1);
		ev_test->ev.frame_sequence = frm_num++;
		ev_test->ev.timestamp = ktime_to_timeval(ktime_get());
		list_add_tail(&ev_test->list, &event_q.list);
#endif
		if (!list_empty(&event_q.list)) {
			ev_k = list_first_entry(&event_q.list, struct vi_event_k, list);
			ev_u.dev_id		= ev_k->ev.dev_id;
			ev_u.type		= ev_k->ev.type;
			ev_u.frame_sequence	= ev_k->ev.frame_sequence;
			ev_u.timestamp		= ev_k->ev.timestamp;
			list_del_init(&ev_k->list);
			rc = 0;
			free(ev_k);
		}
		spin_unlock_irqrestore(&event_lock, flags);

		memcpy(p->ptr, &ev_u, sizeof(struct vi_event));

		break;
	}

	case VI_IOCTL_GET_CLUT_TBL_IDX:
	{
		p->value = vi_tuning_get_clut_tbl_idx();

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_IP_INFO:
	{
		memcpy(p->ptr, &ip_info_list, sizeof(struct ip_info) * IP_INFO_ID_MAX);
		rc = 0;
		break;
	}

	case VI_IOCTL_GET_RGBMAP_LE_PHY_BUF:
	{
		struct cvi_vip_memblock *isp_mem;

		isp_mem = malloc(sizeof(struct cvi_vip_memblock));
		memcpy(isp_mem, p->ptr, sizeof(struct cvi_vip_memblock));

		isp_mem->phy_addr = isp_bufpool[isp_mem->raw_num].rgbmap_le[0];
		isp_mem->size = ispblk_dma_buf_get_size2(ctx, ISP_BLK_ID_DMA_CTL10, isp_mem->raw_num);

		memcpy(p->ptr, isp_mem, sizeof(struct cvi_vip_memblock));
		free(isp_mem);

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_RGBMAP_SE_PHY_BUF:
	{
		struct cvi_vip_memblock *isp_mem;

		isp_mem = malloc(sizeof(struct cvi_vip_memblock));
		memcpy(isp_mem, p->ptr, sizeof(struct cvi_vip_memblock));

		isp_mem->phy_addr = isp_bufpool[isp_mem->raw_num].rgbmap_se[0];
		isp_mem->size = ispblk_dma_buf_get_size2(ctx, ISP_BLK_ID_DMA_CTL11, isp_mem->raw_num);

		memcpy(p->ptr, isp_mem, sizeof(struct cvi_vip_memblock));
		free(isp_mem);

		rc = 0;
		break;
	}

	case VI_IOCTL_GET_SHARE_MEM:
	{
		p->ptr = vdev->shared_mem;
		rc = 0;
		break;
	}
	default:
		break;
	}

	return rc;
}

long vi_ioctl(u_int cmd, void *arg)
{
	struct cvi_vi_dev *vdev = g_vdev;
	long ret = 0;
	struct vi_ext_control p;

	memcpy(&p, (void *)arg, sizeof(struct vi_ext_control));

	switch (cmd) {
	case VI_IOC_S_CTRL:
		ret = _vi_s_ctrl(vdev, &p);
		break;
	case VI_IOC_G_CTRL:
		ret = _vi_g_ctrl(vdev, &p);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	memcpy((void *)arg, &p, sizeof(struct vi_ext_control));

	return ret;
}

int vi_open(void)
{
	int ret = 0;
	struct cvi_vi_dev *vdev;

	vdev = g_vdev;

	if (!atomic_read(&dev_open_cnt)) {

		_vi_clk_ctrl(vdev, true);

		vi_init();

		_vi_sw_init(vdev);

		vi_pr(VI_INFO, "-\n");
	}

	atomic_inc(&dev_open_cnt);

	return ret;
}

int vi_close(void)
{
	int ret = 0;

	atomic_dec(&dev_open_cnt);

	if (!atomic_read(&dev_open_cnt)) {
		struct cvi_vi_dev *vdev;

		vdev = g_vdev;

		_vi_sdk_release(vdev);

		_vi_release_op(vdev);

		vi_deinit();

		vi_pr(VI_INFO, "-\n");
	}

	return ret;
}

#if 0
int vi_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct cvi_vi_dev *vdev;
	unsigned long vm_start = vma->vm_start;
	unsigned int vm_size = vma->vm_end - vma->vm_start;
	unsigned int offset = vma->vm_pgoff << PAGE_SHIFT;
	void *pos;

	vdev = file->private_data;
	pos = vdev->shared_mem;

	if (offset < 0 || (vm_size + offset) > VI_SHARE_MEM_SIZE)
		return -EINVAL;

	while (vm_size > 0) {
		if (remap_pfn_range(vma, vm_start, virt_to_pfn(pos), PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
		vi_pr(VI_DBG, "vi proc mmap vir(%p) phys(%#llx)\n", pos, virt_to_phys((void *) pos));
		vm_start += PAGE_SIZE;
		pos += PAGE_SIZE;
		vm_size -= PAGE_SIZE;
	}

	return 0;
}

unsigned int vi_poll(struct file *file, struct poll_table_struct *wait)
{
	struct cvi_vi_dev *vdev = file->private_data;
	unsigned long req_events = poll_requested_events(wait);
	unsigned int res = 0;
	unsigned long flags;

	if (req_events & POLLPRI) {
		/*
		 * If event buf is not empty, then notify MW to DQ event.
		 * Otherwise poll_wait.
		 */
		spin_lock_irqsave(&event_lock, flags);
		if (!list_empty(&event_q.list))
			res = POLLPRI;
		else
			poll_wait(file, &vdev->isp_event_wait_q, wait);
		spin_unlock_irqrestore(&event_lock, flags);
	}

	if (req_events & POLLIN) {
		if (atomic_read(&vdev->isp_dbg_flag)) {
			res = POLLIN | POLLRDNORM;
			atomic_set(&vdev->isp_dbg_flag, 0);
		} else {
			poll_wait(file, &vdev->isp_dbg_wait_q, wait);
		}
	}

	return res;
}
#endif

int vi_cb(void *dev, enum ENUM_MODULES_ID caller, u32 cmd, void *arg)
{
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)dev;
	struct isp_ctx *ctx = &vdev->ctx;
	int rc = -1;

	switch (cmd) {
	case VI_CB_QBUF_TRIGGER:
		vi_pr(VI_INFO, "isp_ol_sc_trig_post\n");

		vi_wake_up(&vdev->job_work);

		rc = 0;
		break;
	case VI_CB_SC_FRM_DONE:
		vi_pr(VI_DBG, "sc frm done cb\n");

		atomic_set(&vdev->ol_sc_frm_done, 1);
		vi_wake_up(&vdev->job_work);

		rc = 0;
		break;
	case VI_CB_SET_VIVPSSMODE:
	{
		//VI_VPSS_MODE_S stVIVPSSMode;
		CVI_U8   dev_num = 0;
		CVI_BOOL vi_online = CVI_FALSE;

		memcpy(&stVIVPSSMode, arg, sizeof(VI_VPSS_MODE_S));

		vi_online = (stVIVPSSMode.aenMode[0] == VI_ONLINE_VPSS_ONLINE) ||
			    (stVIVPSSMode.aenMode[0] == VI_ONLINE_VPSS_OFFLINE);

		if (vi_online) {
			ctx->is_offline_postraw = ctx->is_offline_be = !vi_online;

			vi_pr(VI_DBG, "Caller_Mod(%d) set vi_online:%d, is_offline_postraw=%d, is_offline_be=%d\n",
					caller, vi_online, ctx->is_offline_postraw, ctx->is_offline_be);
		}

		for (dev_num = 0; dev_num < VI_MAX_DEV_NUM; dev_num++) {
			CVI_BOOL is_vpss_online = (stVIVPSSMode.aenMode[dev_num] == VI_ONLINE_VPSS_ONLINE) ||
						  (stVIVPSSMode.aenMode[dev_num] == VI_OFFLINE_VPSS_ONLINE);

			ctx->isp_pipe_cfg[dev_num].is_offline_scaler = !is_vpss_online;
			vi_pr(VI_DBG, "raw_num_%d set is_offline_scaler:%d\n",
					dev_num, !is_vpss_online);
		}

		rc = 0;
		break;
	}
	case VI_CB_GDC_OP_DONE:
	{
		struct ldc_op_done_cfg *cfg =
			(struct ldc_op_done_cfg *)arg;

		vi_gdc_callback(cfg->pParam, cfg->blk);
		rc = 0;
		break;
	}
	default:
		break;
	}

	return rc;
}

/********************************************************************************
 *  VI event handler related
 *******************************************************************************/
void vi_destory_dbg_thread(struct cvi_vi_dev *vdev)
{
	atomic_set(&vdev->isp_dbg_flag, 1);
	vi_wake_up(&vdev->isp_dbg_wait_q);
}

//TODO wait debug thread
static void _vi_timeout_chk(struct cvi_vi_dev *vdev)
{
#if 0
	if (++gViCtx->timeout_cnt >= 2) {
		atomic_set(&vdev->isp_dbg_flag, 1);
		vi_wake_up(&vdev->isp_dbg_wait_q);
		gViCtx->timeout_cnt = 0;
	}
#endif
}

#ifdef VI_PROFILE
static void _vi_update_chnRealFrameRate(VI_CHN_STATUS_S *pstViChnStatus)
{
	CVI_U64 duration, curTimeUs;
	struct timespec curTime;

	clock_gettime(CLOCK_MONOTONIC, &curTime);
	curTimeUs = curTime.tv_sec * 1000000L + curTime.tv_nsec / 1000L;
	duration = curTimeUs - pstViChnStatus->u64PrevTime;

	if (duration >= 1000000) {
		pstViChnStatus->u32FrameRate = pstViChnStatus->u32FrameNum;
		pstViChnStatus->u32FrameNum = 0;
		pstViChnStatus->u64PrevTime = curTimeUs;
	}

	vi_pr(VI_DBG, "FrameRate=%d\n", pstViChnStatus->u32FrameRate);
}
#endif

void *_vi_event_handler_thread(void *arg)
{
	unsigned int actl_flags;
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)arg;
	//struct timespec event_time;
	u32 timeout = 1000;//ms
	int ret = 0, flag = 0, ret2 = CVI_SUCCESS;
	enum E_VI_TH th_id = E_VI_TH_EVENT_HANDLER;
	MMF_CHN_S chn = {.enModId = CVI_ID_VI, .s32DevId = 0, .s32ChnId = 0};
#ifdef VI_PROFILE
	struct timespec time[2];
	CVI_U32 sum = 0, duration = 0, duration_max = 0, duration_min = 1000 * 1000;
	CVI_U8 count = 0;
#endif

	while (1) {
#ifdef VI_PROFILE
		_vi_update_chnRealFrameRate(&gViCtx->chnStatus[chn.s32ChnId]);
		clock_gettime(CLOCK_MONOTONIC, &time[0]);
#endif
		//ret = sem_timedwait(&vdev->vi_th[th_id].sem, &event_time);
		ret = aos_event_get(&vdev->vi_th[th_id].event, 0x01, AOS_EVENT_OR_CLEAR, &actl_flags, 1000);
		if (vdev->vi_th[th_id].flag == 0)
			continue;

		flag = vdev->vi_th[th_id].flag;
		vdev->vi_th[th_id].flag = 0;

		if (atomic_read(&vdev->vi_th[th_id].exit_flag) == 1) {
			vi_pr(VI_DBG, "%s exit\n", vdev->vi_th[th_id].th_name);
			atomic_set(&vdev->vi_th[th_id].thread_exit, 1);
			pthread_exit(NULL);
		}

		if (ret != 0) {
			vi_pr(VI_ERR, "vi_event_handler timeout(%d)ms\n", timeout);
			_vi_timeout_chk(vdev);
			continue;
		} else {
			struct _vi_buffer b;
			VB_BLK blk = 0;

			//DQbuf from list.
			if (vi_dqbuf(&b) == CVI_FAILURE) {
				vi_pr(VI_WARN, "illegal wakeup raw_num[%d]\n", flag);
				continue;
			}
			chn.s32ChnId = b.chnId;
			ret2 = vb_dqbuf(chn, CHN_TYPE_OUT, &blk);
			if (ret2 != CVI_SUCCESS) {
				if (blk == VB_INVALID_HANDLE)
					vi_pr(VI_ERR, "chn(%d) can't get vb-blk.\n", chn.s32ChnId);
				continue;
			}

			((struct vb_s *)blk)->buf.dev_num = b.chnId;
			((struct vb_s *)blk)->buf.frm_num = b.sequence;
			((struct vb_s *)blk)->buf.u64PTS =
					(CVI_U64)b.timestamp.tv_sec * 1000000 + b.timestamp.tv_nsec / 1000; //microsec

			gViCtx->chnStatus[chn.s32ChnId].u32IntCnt++;
			gViCtx->chnStatus[chn.s32ChnId].u32FrameNum++;
			gViCtx->chnStatus[chn.s32ChnId].u32RecvPic = b.sequence;

			vi_pr(VI_DBG, "dqbuf chn_id=%d, frm_num=%d\n", b.chnId, b.sequence);

			if (gViCtx->bypass_frm[chn.s32ChnId] >= b.sequence) {
				//Release buffer if bypass_frm is not zero
				vb_release_block(blk);
				goto QBUF;
			}

			if (!gViCtx->pipeAttr[chn.s32ChnId].bYuvBypassPath) {
				vi_fill_mlv_info((struct vb_s *)blk, 0, NULL, true);
#if 0 //ToDo get dis info
				_vi_get_dis_info((VB_S *)blk, &cur_dis_data);
#endif
			}
			// TODO: extchn only support works on original frame without GDC effect.
			//_vi_handle_extchn(chn.s32ChnId, chn, blk, &bFisheyeOn);
			//if (bFisheyeOn)
				//goto VB_DONE;

			struct cvi_gdc_mesh *pmesh = &g_vi_mesh[chn.s32ChnId];
			struct vb_s *vb = (struct vb_s *)blk;

			if (pthread_mutex_trylock(&pmesh->lock) == 0) {
				if (gViCtx->stLDCAttr[chn.s32ChnId].bEnable) {
					struct _vi_gdc_cb_param cb_param = { .chn = chn, .usage = GDC_USAGE_LDC};

					if (_mesh_gdc_do_op_cb(GDC_USAGE_LDC, &gViCtx->stLDCAttr[chn.s32ChnId].stAttr
						, vb, gViCtx->chnAttr[chn.s32ChnId].enPixelFormat, pmesh->paddr
						, CVI_FALSE, &cb_param
						, sizeof(cb_param), CVI_ID_VI
						, gViCtx->enRotation[chn.s32ChnId]) != CVI_SUCCESS) {
						pthread_mutex_unlock(&pmesh->lock);
						vi_pr(VI_ERR, "gdc LDC failed.\n");
					}
					goto QBUF;
				} else if (gViCtx->enRotation[chn.s32ChnId] != ROTATION_0) {
					struct _vi_gdc_cb_param cb_param = { .chn = chn, .usage = GDC_USAGE_ROTATION};
					if (_mesh_gdc_do_op_cb(GDC_USAGE_ROTATION, NULL
						, vb, gViCtx->chnAttr[chn.s32ChnId].enPixelFormat, pmesh->paddr
						, CVI_FALSE, &cb_param
						, sizeof(cb_param), CVI_ID_VI
						, gViCtx->enRotation[chn.s32ChnId]) != CVI_SUCCESS) {
						pthread_mutex_unlock(&pmesh->lock);
						vi_pr(VI_ERR, "gdc rotation failed.\n");
					}
					goto QBUF;
				}
				pthread_mutex_unlock(&pmesh->lock);
			} else {
				vi_pr(VI_WARN, "chn(%d) drop frame due to gdc op blocked.\n",
					     chn.s32ChnId);
				// release blk if gdc not done yet
				vb_release_block(blk);
				goto QBUF;
			}
//VB_DONE:

			vb_done_handler(chn, CHN_TYPE_OUT, blk);
QBUF:
			// get another vb for next frame
			if (vi_sdk_qbuf(chn) != CVI_SUCCESS)
				vb_acquire_block(vi_sdk_qbuf, chn, gViCtx->blk_size[chn.s32ChnId], VB_INVALID_POOLID);
		}
#ifdef VI_PROFILE
		clock_gettime(CLOCK_MONOTONIC, &time[1]);
		duration = get_diff_in_us(time[0], time[1]);
		duration_max = MAX(duration, duration_max);
		duration_min = MIN(duration, duration_min);
		sum += duration;
		if (++count == 100) {
			vi_pr(VI_DBG, "VI duration(ms): average(%d), max(%d) min(%d)\n"
				, sum / count / 1000, duration_max / 1000, duration_min / 1000);
			count = 0;
			sum = duration_max = 0;
			duration_min = 1000 * 1000;
		}
#endif
	}

	return 0;
}

/*******************************************************
 *  Irq handlers
 ******************************************************/

static void _vi_err_retrig_preraw(struct cvi_vi_dev *vdev, const enum cvi_isp_raw err_raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	u8 wait_fe = ISP_PRERAW_A;
	enum cvi_isp_pre_chn_num fe_max, fe_chn;
	unsigned long flags;

	if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) {
		//bypass first garbage frame when overflow happened
		if (!ctx->isp_pipe_cfg[wait_fe].is_offline_scaler)
			ispblk_yuvtop_out_config(ctx, wait_fe, false);

		//only enable bypass, we need enable dma func and disable dma
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL46, true, true);
		ispblk_dma_enable(ctx, ISP_BLK_ID_DMA_CTL47, true, true);

		return;
	}

	for (wait_fe = ISP_PRERAW_A; wait_fe < gViCtx->total_dev_num; wait_fe++) {
		if (vdev->isp_err_times[wait_fe]++ > 30) {
			vi_pr(VI_ERR, "too much errors happened\n");
			continue;
		}

		vi_pr(VI_WARN, "fe_%d isp_pre_trig retry %d times\n", wait_fe, vdev->isp_err_times[wait_fe]);

		// yuv sensor offline to sc
		if (ctx->isp_pipe_cfg[wait_fe].is_yuv_bypass_path &&
			ctx->isp_pipe_cfg[wait_fe].is_offline_scaler) {
			fe_max = ctx->isp_pipe_cfg[wait_fe].muxMode + 1;
			for (fe_chn = ISP_FE_CH0; fe_chn < fe_max; fe_chn++) {
				u8 buf_chn = (wait_fe == ISP_PRERAW_A) ? fe_chn : vdev->ctx.rawb_chnstr_num + fe_chn;

				if (wait_fe == err_raw_num) //if yuv sensor err need push, that's right qbuf num.
					vdev->qbuf_num[buf_chn]++;

				if (cvi_isp_rdy_buf_empty2(vdev, buf_chn)) {
					vi_pr(VI_INFO, "fe_%d chn_num_%d yuv bypass outbuf is empty\n", wait_fe, buf_chn);
					continue;
				}

				spin_lock_irqsave(&stream_lock, flags);
				_isp_yuv_bypass_trigger(vdev, wait_fe, fe_chn);
				spin_unlock_irqrestore(&stream_lock, flags);
			}
		} else { //rgb sensor
			spin_lock_irqsave(&stream_lock, flags);
			if (ctx->isp_pipe_cfg[wait_fe].is_synthetic_hdr_on) {
				fe_chn = (vdev->pre_fe_frm_num[wait_fe][ISP_FE_CH0]
					== vdev->pre_fe_frm_num[wait_fe][ISP_FE_CH1]) ?
					ISP_FE_CH0 : ISP_FE_CH1;
				_pre_hw_enque(vdev, wait_fe, fe_chn);
			} else {
				_pre_hw_enque(vdev, wait_fe, ISP_FE_CH0);
				if (ctx->isp_pipe_cfg[wait_fe].is_hdr_on) {
					_pre_hw_enque(vdev, wait_fe, ISP_FE_CH1);
				}
			}
			spin_unlock_irqrestore(&stream_lock, flags);
		}
	}
}

void _vi_err_handler(struct cvi_vi_dev *vdev, const enum cvi_isp_raw err_raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uintptr_t tnr = ctx->phys_regs[ISP_BLK_ID_TNR];
	u8 wait_fe = ISP_PRERAW_A, count = 10;

	//step 1 : set frm vld = 0
	isp_frm_err_handler(ctx, err_raw_num, 1);

	atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH0], ISP_PRERAW_IDLE);
	atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH1], ISP_PRERAW_IDLE);

	//make sure disable mono for isp reset
	if (ISP_RD_BITS(tnr, REG_ISP_444_422_T, REG_5, FORCE_MONO_ENABLE) == 1) {
		ISP_WR_BITS(tnr, REG_ISP_444_422_T, REG_5, FORCE_MONO_ENABLE, 0);
	}

	//step 2 : wait to make sure post and the other fe is done.
	while (--count > 0) {
		if (_is_be_post_online(ctx)) {
			if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH1]) == ISP_PRERAW_IDLE &&
				atomic_read(&vdev->pre_be_state[ISP_BE_CH0]) == ISP_PRE_BE_IDLE &&
				atomic_read(&vdev->pre_be_state[ISP_BE_CH1]) == ISP_PRE_BE_IDLE)
				break;
			vi_pr(VI_WARN, "wait fe, be and post idle count(%d) for be_post online\n", count);
		} else if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) {
			if (ctx->is_multi_sensor) {
				if (err_raw_num == ISP_PRERAW_A) {
					wait_fe = ISP_PRERAW_B;

					if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_IDLE &&
					atomic_read(&vdev->pre_fe_state[wait_fe][ISP_FE_CH0]) == ISP_PRERAW_IDLE &&
					atomic_read(&vdev->pre_fe_state[wait_fe][ISP_FE_CH1]) == ISP_PRERAW_IDLE)
						break;
					vi_pr(VI_WARN, "wait fe_%d and post idle count(%d) for fe_be online dual\n",
							wait_fe, count);
				} else {
					if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_IDLE &&
						atomic_read(&vdev->pre_be_state[ISP_BE_CH0]) == ISP_PRE_BE_IDLE &&
						atomic_read(&vdev->pre_be_state[ISP_BE_CH1]) == ISP_PRE_BE_IDLE)
						break;
					vi_pr(VI_WARN, "wait be and post idle count(%d) for fe_be online dual\n",
							count);
				}
			} else {
				if (atomic_read(&vdev->postraw_state) == ISP_POSTRAW_IDLE)
					break;
				vi_pr(VI_WARN, "wait post idle(%d) count(%d) for fe_be online single\n",
						atomic_read(&vdev->postraw_state), count);
			}
		} else {
			break;
		}

		usleep(10 * 1000);
	}

	//If fe/be/post not done;
	if (count == 0) {
		if (ctx->is_multi_sensor) {
			vi_pr(VI_ERR, "isp status fe_0(ch0:%d, ch1:%d) fe_1(ch0:%d, ch1:%d) \
					fe_2(ch0:%d, ch1:%d) be(ch0:%d, ch1:%d) postraw(%d)\n",
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH0]),
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_A][ISP_FE_CH1]),
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH0]),
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_B][ISP_FE_CH1]),
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH0]),
					atomic_read(&vdev->pre_fe_state[ISP_PRERAW_C][ISP_FE_CH1]),
					atomic_read(&vdev->pre_be_state[ISP_BE_CH0]),
					atomic_read(&vdev->pre_be_state[ISP_BE_CH1]),
					atomic_read(&vdev->postraw_state));
		} else {
			vi_pr(VI_ERR, "isp status post(%d)\n", atomic_read(&vdev->postraw_state));
		}
		return;
	}

	if (!ctx->isp_pipe_cfg[err_raw_num].is_yuv_bypass_path) {
		//step 3 : set csibdg sw abort and wait abort done
		if (isp_frm_err_handler(ctx, err_raw_num, 3) < 0)
			return;

		//step 4 : isp sw reset and vip reset pull up
		isp_frm_err_handler(ctx, err_raw_num, 4);

		//send err cb to vpss if vpss online
		if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on &&
			!ctx->isp_pipe_cfg[err_raw_num].is_offline_scaler) { //VPSS online
			struct sc_err_handle_cb err_cb = {0};

			/* VPSS Online error handle */
			err_cb.snr_num = err_raw_num;
			if (_vi_call_cb(E_MODULE_VPSS, VPSS_CB_ONLINE_ERR_HANDLE, &err_cb) != 0) {
				vi_pr(VI_ERR, "VPSS_CB_ONLINE_ERR_HANDLE is failed\n");
			}
		}

		//step 5 : isp sw reset and vip reset pull down
		isp_frm_err_handler(ctx, err_raw_num, 5);

		//step 6 : wait ISP idle
		if (isp_frm_err_handler(ctx, err_raw_num, 6) < 0)
			return;
	} else { // yuv sensor only reset csi
		//step 4 : isp sw reset and vip reset pull up
		isp_frm_err_handler(ctx, err_raw_num, 4);
		//step 5 : isp sw reset and vip reset pull down
		isp_frm_err_handler(ctx, err_raw_num, 5);
	}

	//step 7 : reset sw state to idle
	if (_is_be_post_online(ctx)) {
		atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH0], ISP_PRERAW_IDLE);
		atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH1], ISP_PRERAW_IDLE);
	} else if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) {
		if (ctx->is_multi_sensor) {
			if (err_raw_num == ISP_PRERAW_A) {
				atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
				atomic_set(&vdev->pre_be_state[ISP_BE_CH1], ISP_PRE_BE_IDLE);
			} else {
				atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH0], ISP_PRERAW_IDLE);
				atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH1], ISP_PRERAW_IDLE);
			}
		} else {
			atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
			atomic_set(&vdev->pre_be_state[ISP_BE_CH1], ISP_PRE_BE_IDLE);
		}
	} else if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) { //slice buffer on
		atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH0], ISP_PRERAW_IDLE);
		atomic_set(&vdev->pre_fe_state[err_raw_num][ISP_FE_CH1], ISP_PRERAW_IDLE);
		atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);
		atomic_set(&vdev->pre_be_state[ISP_BE_CH1], ISP_PRE_BE_IDLE);
		atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
	}

	//step 8 : set fbcd dma to hw mode if fbc is on
	if (ctx->is_fbc_on) {
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL41, false);
		ispblk_dma_set_sw_mode(ctx, ISP_BLK_ID_DMA_CTL42, false);
	}

	//step 9 : reset first frame count
	vdev->ctx.isp_pipe_cfg[err_raw_num].first_frm_cnt = 0;
	if (ctx->is_multi_sensor)
		vdev->ctx.isp_pipe_cfg[wait_fe].first_frm_cnt = 0;

	//Let postraw trigger go
	atomic_set(&vdev->isp_err_handle_flag, 0);

	//Let preraw trgger, err_handler we stop all dev
	_vi_err_retrig_preraw(vdev, err_raw_num);
}

void *_vi_err_handler_thread(void *arg)
{
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)arg;
	enum cvi_isp_raw err_raw_num = ISP_PRERAW_A;
	enum E_VI_TH th_id = E_VI_TH_ERR_HANDLER;
	unsigned int actl_flags = 0;
	while (1) {
		if (aos_event_get(&vdev->vi_th[th_id].event, 0x01, AOS_EVENT_OR_CLEAR,
			&actl_flags, AOS_WAIT_FOREVER) != 0) {
			continue;
		}

		if (atomic_read(&vdev->vi_th[th_id].exit_flag) == 1) {
			vi_pr(VI_DBG, "%s exit\n", vdev->vi_th[th_id].th_name);
			atomic_set(&vdev->vi_th[th_id].thread_exit, 1);
			pthread_exit(NULL);
		}

		if (vdev->vi_th[th_id].flag == 0)
			continue;

		err_raw_num = vdev->vi_th[th_id].flag - 1;
		vdev->vi_th[th_id].flag = 0;

		_vi_err_handler(vdev, err_raw_num);
	}

	return 0;
}

static inline void vi_err_wake_up_th(struct cvi_vi_dev *vdev, enum cvi_isp_raw err_raw)
{
	//Stop pre/postraw trigger go
	atomic_set(&vdev->isp_err_handle_flag, 1);

	vdev->vi_th[E_VI_TH_ERR_HANDLER].flag = err_raw + 1;

	vi_wake_up(&vdev->vi_th[E_VI_TH_ERR_HANDLER]);
}

u32 isp_err_chk(
	struct cvi_vi_dev *vdev,
	struct isp_ctx *ctx,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_0 cbdg_0_sts,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_1 cbdg_1_sts,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_0 cbdg_0_sts_b,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_1 cbdg_1_sts_b,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_0 cbdg_0_sts_c,
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_1 cbdg_1_sts_c)
{
	u32 ret = 0;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	enum cvi_isp_pre_chn_num fe_chn = ISP_FE_CH0;

	if (cbdg_1_sts.bits.FIFO_OVERFLOW_INT) {
		vi_pr(VI_ERR, "CSIBDG_A fifo overflow\n");
		ctx->isp_pipe_cfg[raw_num].dg_info.bdg_fifo_of_cnt++;
		vi_err_wake_up_th(vdev, raw_num);
		ret = -1;
	}

	if (cbdg_1_sts.bits.FRAME_RESOLUTION_OVER_MAX_INT) {
		vi_pr(VI_ERR, "CSIBDG_A frm size over max\n");
		ret = -1;
	}

	if (cbdg_1_sts.bits.DMA_ERROR_INT) {
		u32 wdma_0_err = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_0_err_sts;
		u32 wdma_1_err = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_1_err_sts;
		u32 rdma_err = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.rdma_err_sts;
		u32 wdma_0_idle = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_0_idle;
		u32 wdma_1_idle = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.wdma_1_idle;
		u32 rdma_idle = ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts.rdma_idle;

		if ((wdma_0_err & 0x10) || (wdma_1_err & 0x10) || (rdma_err & 0x10)) {
			vi_pr(VI_ERR, "DMA axi error wdma0_status(0x%x) wdma1_status(0x%x) rdma_status(0x%x)\n",
					wdma_0_err, wdma_1_err, rdma_err);
			ret = -1;
		} else if ((wdma_0_err & 0x20) || (wdma_1_err & 0x20)) {
			vi_pr(VI_ERR, "DMA size mismatch\n wdma0_status(0x%x) wdma1_status(0x%x) rdma_status(0x%x)\n",
					wdma_0_err, wdma_1_err, rdma_err);
			vi_pr(VI_ERR, "wdma0_idle(0x%x) wdma1_idle(0x%x) rdma_idle(0x%x)\n",
					wdma_0_idle, wdma_1_idle, rdma_idle);
			ret = -1;
		} else if ((wdma_0_err & 0x40) || (wdma_1_err & 0x40)) {
			vi_pr(VI_WARN, "WDMA buffer full\n");
		}
	}

	if (cbdg_0_sts.bits.CH0_FRAME_WIDTH_GT_INT) {
		vi_pr(VI_ERR, "CSIBDG_A CH%d frm width greater than setting(%d)\n",
				fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
		ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[fe_chn]++;
		vi_err_wake_up_th(vdev, raw_num);
		ret = ctx->is_multi_sensor ? 0 : -1;
	}

	if (cbdg_0_sts.bits.CH0_FRAME_WIDTH_LS_INT) {
		vi_pr(VI_ERR, "CSIBDG_A CH%d frm width less than setting(%d)\n",
				fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
		ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[fe_chn]++;
		vi_err_wake_up_th(vdev, raw_num);
		ret = ctx->is_multi_sensor ? 0 : -1;
	}

	if (cbdg_0_sts.bits.CH0_FRAME_HEIGHT_GT_INT) {
		vi_pr(VI_ERR, "CSIBDG_A CH%d frm height greater than setting(%d)\n",
				fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
		ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[fe_chn]++;
		vi_err_wake_up_th(vdev, raw_num);
		ret = ctx->is_multi_sensor ? 0 : -1;
	}

	if (cbdg_0_sts.bits.CH0_FRAME_HEIGHT_LS_INT) {
		vi_pr(VI_ERR, "CSIBDG_A CH%d frm height less than setting(%d)\n",
				fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
		ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[fe_chn]++;
		vi_err_wake_up_th(vdev, raw_num);
		ret = ctx->is_multi_sensor ? 0 : -1;
	}

	if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
		fe_chn = ISP_FE_CH1;

		if (cbdg_0_sts.bits.CH1_FRAME_WIDTH_GT_INT) {
			vi_pr(VI_ERR, "CSIBDG_A CH%d frm width greater than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts.bits.CH1_FRAME_WIDTH_LS_INT) {
			vi_pr(VI_ERR, "CSIBDG_A CH%d frm width less than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts.bits.CH1_FRAME_HEIGHT_GT_INT) {
			vi_pr(VI_ERR, "CSIBDG_A CH%d frm height greater than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts.bits.CH1_FRAME_HEIGHT_LS_INT) {
			vi_pr(VI_ERR, "CSIBDG_A CH%d frm height less than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}
	}

	if (ctx->is_multi_sensor || ctx->isp_pipe_cfg[raw_num].muxMode == VI_WORK_MODE_2Multiplex) {
		raw_num = ISP_PRERAW_B;
		fe_chn = ISP_FE_CH0;

		if (cbdg_1_sts_b.bits.FIFO_OVERFLOW_INT) {
			vi_pr(VI_ERR, "CSIBDG_B fifo overflow\n");
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_fifo_of_cnt++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_1_sts_b.bits.FRAME_RESOLUTION_OVER_MAX_INT) {
			vi_pr(VI_ERR, "CSIBDG_B frm size over max\n");
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts_b.bits.CH0_FRAME_WIDTH_GT_INT) {
			vi_pr(VI_ERR, "CSIBDG_B CH%d frm width greater than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts_b.bits.CH0_FRAME_WIDTH_LS_INT) {
			vi_pr(VI_ERR, "CSIBDG_B CH%d frm width less than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts_b.bits.CH0_FRAME_HEIGHT_GT_INT) {
			vi_pr(VI_ERR, "CSIBDG_B CH%d frm height greater than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (cbdg_0_sts_b.bits.CH0_FRAME_HEIGHT_LS_INT) {
			vi_pr(VI_ERR, "CSIBDG_B CH%d frm height less than setting(%d)\n",
					fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[fe_chn]++;
			vi_err_wake_up_th(vdev, raw_num);
			ret = ctx->is_multi_sensor ? 0 : -1;
		}

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			fe_chn = ISP_FE_CH1;

			if (cbdg_0_sts_b.bits.CH1_FRAME_WIDTH_GT_INT) {
				vi_pr(VI_ERR, "CSIBDG_B CH%d frm width greater than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = ctx->is_multi_sensor ? 0 : -1;
			}

			if (cbdg_0_sts_b.bits.CH1_FRAME_WIDTH_LS_INT) {
				vi_pr(VI_ERR, "CSIBDG_B CH%d frm width less than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = ctx->is_multi_sensor ? 0 : -1;
			}

			if (cbdg_0_sts_b.bits.CH1_FRAME_HEIGHT_GT_INT) {
				vi_pr(VI_ERR, "CSIBDG_B CH%d frm height greater than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = ctx->is_multi_sensor ? 0 : -1;
			}

			if (cbdg_0_sts_b.bits.CH1_FRAME_HEIGHT_LS_INT) {
				vi_pr(VI_ERR, "CSIBDG_B CH%d frm height less than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = ctx->is_multi_sensor ? 0 : -1;
			}
		}
		if (gViCtx->total_dev_num == ISP_PRERAW_MAX) {
			raw_num = ISP_PRERAW_C;
			fe_chn = ISP_FE_CH0;

			if (cbdg_1_sts_c.bits.FIFO_OVERFLOW_INT) {
				vi_pr(VI_ERR, "CSIBDG_C fifo overflow\n");
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_fifo_of_cnt++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = -1;
			}

			if (cbdg_1_sts_c.bits.FRAME_RESOLUTION_OVER_MAX_INT) {
				vi_pr(VI_ERR, "CSIBDG_C frm size over max\n");
				ret = -1;
			}

			if (cbdg_0_sts_c.bits.CH0_FRAME_WIDTH_GT_INT) {
				vi_pr(VI_ERR, "CSIBDG_C CH%d frm width greater than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_gt_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = -1;
			}

			if (cbdg_0_sts_c.bits.CH0_FRAME_WIDTH_LS_INT) {
				vi_pr(VI_ERR, "CSIBDG_C CH%d frm width less than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_width);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_w_ls_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = -1;
			}

			if (cbdg_0_sts_c.bits.CH0_FRAME_HEIGHT_GT_INT) {
				vi_pr(VI_ERR, "CSIBDG_C CH%d frm height greater than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_gt_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = -1;
			}

			if (cbdg_0_sts_c.bits.CH0_FRAME_HEIGHT_LS_INT) {
				vi_pr(VI_ERR, "CSIBDG_C CH%d frm height less than setting(%d)\n",
						fe_chn, ctx->isp_pipe_cfg[raw_num].csibdg_height);
				ctx->isp_pipe_cfg[raw_num].dg_info.bdg_h_ls_cnt[fe_chn]++;
				vi_err_wake_up_th(vdev, raw_num);
				ret = -1;
			}
		}
	}

	return ret;
}

void *_isp_post_thread(void *data)
{
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)data;
	unsigned int actl_flags;
	while (1) {
		if (aos_event_get(&vdev->job_work.event, 0x01, AOS_EVENT_OR_CLEAR,
			&actl_flags, AOS_WAIT_FOREVER) != 0) {
			continue;
		}
		if (vdev->job_work.flag == 3) {
			vi_pr(VI_INFO, "exit\n");
			atomic_set(&vdev->job_work.thread_exit, 1);
			pthread_exit(NULL);
		}

		_post_hw_enque(vdev);
	}
	return 0;
}

void *_vi_preraw_thread(void *arg)
{
	struct cvi_vi_dev *vdev = (struct cvi_vi_dev *)arg;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	struct isp_ctx *ctx = &vdev->ctx;

	struct list_head *pos, *temp;
	struct _isp_raw_num_n  *n[VI_MAX_LIST_NUM];
	unsigned long flags;
	u32 enq_num = 0, i = 0;
	enum E_VI_TH th_id = E_VI_TH_PRERAW;
	unsigned int actl_flags = 0;
	while (1) {
		if (aos_event_get(&vdev->vi_th[th_id].event, 0x01, AOS_EVENT_OR_CLEAR,
			&actl_flags, AOS_WAIT_FOREVER) != 0) {
			continue;
		}

		if (atomic_read(&vdev->vi_th[th_id].exit_flag) == 1) {
			vi_pr(VI_DBG, "%s exit\n", vdev->vi_th[th_id].th_name);
			atomic_set(&vdev->vi_th[th_id].thread_exit, 1);
			pthread_exit(NULL);
		}

		if (vdev->vi_th[th_id].flag == 0)
			continue;

		vdev->vi_th[th_id].flag = 0;

		spin_lock_irqsave(&raw_num_lock, flags);
		list_for_each_safe(pos, temp, &pre_raw_num_q.list) {
			n[enq_num] = list_entry(pos, struct _isp_raw_num_n, list);
			enq_num++;
		}
		spin_unlock_irqrestore(&raw_num_lock, flags);

		for (i = 0; i < enq_num; i++) {
			raw_num = n[i]->raw_num;

			spin_lock_irqsave(&raw_num_lock, flags);
			list_del_init(&n[i]->list);
			--pre_raw_num_q.num_rdy;
			free(n[i]);
			spin_unlock_irqrestore(&raw_num_lock, flags);

			if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
				pre_be_tuning_update(&vdev->ctx, raw_num);
				//Update pre be sts size/addr
				_swap_pre_be_sts_buf(vdev, raw_num, ISP_BE_CH0);

				postraw_tuning_update(&vdev->ctx, raw_num);
				//Update postraw sts awb/dci/hist_edge_v dma size/addr
				_swap_post_sts_buf(ctx, raw_num);
			} else {
				_isp_snr_cfg_deq_and_fire(vdev, raw_num);

				pre_fe_tuning_update(&vdev->ctx, raw_num);

				//fe->be->dram->post or on the fly
				if (_is_fe_be_online(ctx) || _is_all_online(ctx)) {
					pre_be_tuning_update(&vdev->ctx, raw_num);

					//on the fly or slice buffer mode on
					if (_is_all_online(ctx) || ctx->is_slice_buf_on) {
						postraw_tuning_update(&vdev->ctx, raw_num);
					}
				}
			}
			if ((ctx->is_multi_sensor) && (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path)) {
				if ((tuning_dis[0] > 0) && ((tuning_dis[0] - 1) != raw_num)) {
					vi_pr(VI_DBG, "raw_%d start drop\n", raw_num);
					ctx->isp_pipe_cfg[raw_num].is_drop_next_frame = true;
				}
			}

			if (ctx->isp_pipe_cfg[raw_num].is_drop_next_frame) {
				//if !is_drop_next_frame, set is_drop_next_frame flags false;
				if (_is_drop_next_frame(vdev, raw_num, ISP_FE_CH0))
					++vdev->drop_frame_number[raw_num];
				else {
					vi_pr(VI_DBG, "raw_%d stop drop\n", raw_num);
					ctx->isp_pipe_cfg[raw_num].isp_reset_frm =
						vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] + 1;
					_clear_drop_frm_info(vdev, raw_num);
				}

				//vi onthefly and vpss online will trigger preraw in post_hw_enque
				if (_is_all_online(ctx) && !ctx->isp_pipe_cfg[raw_num].is_offline_scaler)
					continue;

				if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw) {
					_pre_hw_enque(vdev, raw_num, ISP_FE_CH0);
					if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
						_pre_hw_enque(vdev, raw_num, ISP_FE_CH1);
				}
			}
		}

		enq_num = 0;
	}

	return 0;
}

static void _isp_yuv_online_handler(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 hw_chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	struct isp_buffer *b = NULL;
	u8 buf_chn = (raw_num == ISP_PRERAW_A) ? hw_chn_num : vdev->ctx.rawb_chnstr_num + hw_chn_num;

	atomic_set(&vdev->pre_fe_state[raw_num][hw_chn_num], ISP_PRERAW_IDLE);

	b = isp_buf_remove(&pre_out_queue[buf_chn]);
	if (b == NULL) {
		vi_pr(VI_INFO, "YUV_chn_%d done outbuf is empty\n", buf_chn);
		return;
	}

	b->crop_le.x = 0;
	b->crop_le.y = 0;
	b->crop_le.w = ctx->isp_pipe_cfg[raw_num].post_img_w;
	b->crop_le.h = ctx->isp_pipe_cfg[raw_num].post_img_h;
	b->is_yuv_frm = 1;
	b->raw_num = raw_num;
	b->chn_num = buf_chn;

	if (_is_be_post_online(ctx))
		isp_buf_queue(&pre_be_in_q, b);
	else if (_is_fe_be_online(ctx))
		isp_buf_queue(&post_in_queue, b);

	// if preraw offline, let usr_pic_timer_handler do it.
	if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw)
		_pre_hw_enque(vdev, raw_num, hw_chn_num);

	if (!vdev->ctx.isp_pipe_cfg[raw_num].is_offline_scaler) { //YUV sensor online mode
		tasklet_hi_schedule(&vdev->job_work);
	}
}

static void _isp_yuv_bypass_handler(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num, const u8 hw_chn_num)
{
	u8 buf_chn = (raw_num == ISP_PRERAW_A) ? hw_chn_num : vdev->ctx.rawb_chnstr_num + hw_chn_num;

	atomic_set(&vdev->pre_fe_state[raw_num][hw_chn_num], ISP_PRERAW_IDLE);

	cvi_isp_rdy_buf_remove2(vdev, buf_chn);

	cvi_isp_dqbuf_list(vdev, vdev->pre_fe_frm_num[raw_num][hw_chn_num], buf_chn);
	vdev->vi_th[E_VI_TH_EVENT_HANDLER].flag = raw_num + 1;
	vi_wake_up(&vdev->vi_th[E_VI_TH_EVENT_HANDLER]);

	if (cvi_isp_rdy_buf_empty2(vdev, buf_chn))
		vi_pr(VI_INFO, "fe_%d chn_num_%d yuv bypass outbuf is empty\n", raw_num, buf_chn);
	else
		_isp_yuv_bypass_trigger(vdev, raw_num, hw_chn_num);
}

static inline void _vi_wake_up_preraw_th(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct _isp_raw_num_n  *n;

	n = malloc(sizeof(*n));
	if (n == NULL) {
		vi_pr(VI_ERR, "pre_raw_num_q kmalloc size(%lu) fail\n", sizeof(*n));
		return;
	}
	n->raw_num = raw_num;
	pre_raw_num_enq(&pre_raw_num_q, n);

	vdev->vi_th[E_VI_TH_PRERAW].flag = (raw_num == ISP_PRERAW_A) ? 1 : 2;
	vi_wake_up(&vdev->vi_th[E_VI_TH_PRERAW]);
}

static void _isp_sof_handler(struct cvi_vi_dev *vdev, const enum cvi_isp_raw raw_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	struct _isp_dqbuf_n *n = NULL;
	unsigned long flags;

	if (atomic_read(&vdev->isp_streamoff) == 1)
		return;

	if (!(_is_fe_be_online(ctx) && ctx->is_slice_buf_on) || ctx->isp_pipe_cfg[raw_num].is_drop_next_frame)
		_vi_wake_up_preraw_th(vdev, raw_num);

	if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 2) //raw_dump flow
		atomic_set(&vdev->isp_raw_dump_en[raw_num], 3);

	tasklet_hi_schedule(&vdev->job_work);

	if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) {
		spin_lock_irqsave(&dq_lock, flags);
		if (!list_empty(&dqbuf_q.list)) {
			n = list_first_entry(&dqbuf_q.list, struct _isp_dqbuf_n, list);
			switch (n->chn_id) {
			case ISP_PRERAW_B:
				vdev->vi_th[E_VI_TH_EVENT_HANDLER].flag = 2;
				break;
			case ISP_PRERAW_C:
				vdev->vi_th[E_VI_TH_EVENT_HANDLER].flag = 3;
				break;
			case ISP_PRERAW_A:
			default:
				vdev->vi_th[E_VI_TH_EVENT_HANDLER].flag = 1;
				break;
			}
			vi_wake_up(&vdev->vi_th[E_VI_TH_EVENT_HANDLER]);
		}
		spin_unlock_irqrestore(&dq_lock, flags);
	}
#if 0
	isp_sync_task_process(raw_num);
#endif
}

static inline void _isp_pre_fe_done_handler(
	struct cvi_vi_dev *vdev,
	const enum cvi_isp_raw raw_num,
	const enum cvi_isp_pre_chn_num chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	u32 trigger = false;

	//reset error times when fe_done
	if (unlikely(vdev->isp_err_times[raw_num])) {
		vdev->isp_err_times[raw_num] = 0;
	}

	if (ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {
		if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) { //offline mode
			vi_pr(VI_DBG, "pre_fe_%d yuv offline done chn_num=%d frm_num=%d\n",
					raw_num, chn_num, vdev->pre_fe_frm_num[raw_num][chn_num]);
			_isp_yuv_bypass_handler(vdev, raw_num, chn_num);
		} else { //YUV sensor online mode
			vi_pr(VI_DBG, "pre_fe_%d yuv online done chn_num=%d frm_num=%d\n",
					raw_num, chn_num, vdev->pre_fe_frm_num[raw_num][chn_num]);
			_isp_yuv_online_handler(vdev, raw_num, chn_num);
		}
		return;
	}

	vi_pr(VI_DBG, "pre_fe_%d frm_done chn_num=%d frm_num=%d\n",
			raw_num, chn_num, vdev->pre_fe_frm_num[raw_num][chn_num]);

	// No changed in onthefly mode or slice buffer on
	if (!_is_all_online(ctx) && !(_is_fe_be_online(ctx) && ctx->is_slice_buf_on)) {
		ispblk_tnr_rgbmap_chg(ctx, raw_num, chn_num);
		_pre_fe_rgbmap_update(vdev, raw_num, chn_num);
	}

	if (_is_fe_be_online(ctx) || _is_all_online(ctx)) { //fe->be->dram->post or on the fly mode
		if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 3) { //raw_dump flow
			struct isp_buffer *b = NULL;
			struct isp_queue *fe_out_q = (chn_num == ISP_FE_CH0) ?
							&raw_dump_b_q[raw_num] :
							&raw_dump_b_se_q[raw_num];
			struct isp_queue *raw_d_q = (chn_num == ISP_FE_CH0) ?
							&raw_dump_b_dq[raw_num] :
							&raw_dump_b_se_dq[raw_num];
			u32 x, y, w, h, dmaid;

			if (ctx->isp_pipe_cfg[raw_num].rawdump_crop.w &&
				ctx->isp_pipe_cfg[raw_num].rawdump_crop.h) {
				x = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].rawdump_crop.x :
					ctx->isp_pipe_cfg[raw_num].rawdump_crop_se.x;
				y = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].rawdump_crop.y :
					ctx->isp_pipe_cfg[raw_num].rawdump_crop_se.y;
				w = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].rawdump_crop.w :
					ctx->isp_pipe_cfg[raw_num].rawdump_crop_se.w;
				h = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].rawdump_crop.h :
					ctx->isp_pipe_cfg[raw_num].rawdump_crop_se.h;
			} else {
				x = 0;
				y = 0;
				w = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].crop.w :
					ctx->isp_pipe_cfg[raw_num].crop_se.w;
				h = (chn_num == ISP_FE_CH0) ?
					ctx->isp_pipe_cfg[raw_num].crop.h :
					ctx->isp_pipe_cfg[raw_num].crop_se.h;
			}

			if (raw_num == ISP_PRERAW_A) {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL6 : ISP_BLK_ID_DMA_CTL7);
			} else if (raw_num == ISP_PRERAW_B) {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13);
			} else {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19);
			}

			if (chn_num == ISP_FE_CH0)
				++vdev->dump_frame_number[raw_num];

			ispblk_csidbg_dma_wr_en(ctx, raw_num, chn_num, 0);

			b = isp_buf_remove(fe_out_q);
			if (b == NULL) {
				vi_pr(VI_ERR, "Pre_fe_%d chn_num_%d outbuf is empty\n", raw_num, chn_num);
				return;
			}

			b->crop_le.x = b->crop_se.x = x;
			b->crop_le.y = b->crop_se.y = y;
			b->crop_le.w = b->crop_se.w = w;
			b->crop_le.h = b->crop_se.h = h;
			b->byr_size = ispblk_dma_get_size(ctx, dmaid, w, h);
			b->frm_num = vdev->pre_fe_frm_num[raw_num][chn_num];

			isp_buf_queue(raw_d_q, b);

			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
				trigger = (vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] ==
						vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1]);
			} else
				trigger = true;

			if (trigger) {
				_isp_raw_dump_chk(vdev, raw_num, b->frm_num);
			}
		}

		atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_IDLE);

		if (_is_all_online(ctx)) {
			struct isp_grid_s_info m_info;

			m_info = ispblk_rgbmap_info(ctx, raw_num);
			ctx->isp_pipe_cfg[raw_num].rgbmap_i.w_bit = m_info.w_bit;
			ctx->isp_pipe_cfg[raw_num].rgbmap_i.h_bit = m_info.h_bit;

			m_info = ispblk_lmap_info(ctx, raw_num);
			ctx->isp_pipe_cfg[raw_num].lmap_i.w_bit = m_info.w_bit;
			ctx->isp_pipe_cfg[raw_num].lmap_i.h_bit = m_info.h_bit;
		}
	} else if (_is_be_post_online(ctx)) { //fe->dram->be->post
		struct isp_buffer *b = NULL;
		struct isp_grid_s_info m_info;
		struct isp_queue *be_in_q = NULL;
		struct isp_queue *fe_out_q = (chn_num == ISP_FE_CH0) ?
						&pre_out_queue[raw_num] : &pre_out_se_queue[raw_num];
		struct isp_queue *raw_d_q = (chn_num == ISP_FE_CH0) ?
					    &raw_dump_b_dq[raw_num] :
					    &raw_dump_b_se_dq[raw_num];

		if (ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
			be_in_q = (chn_num == ISP_FE_CH0) ? &pre_be_in_syn_q : &pre_be_in_se_syn_q[raw_num];
		else
			be_in_q = (chn_num == ISP_FE_CH0) ? &pre_be_in_q : &pre_be_in_se_q[raw_num];

		if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 3) //raw dump enable
			fe_out_q = (chn_num == ISP_FE_CH0) ? &raw_dump_b_q[raw_num] : &raw_dump_b_se_q[raw_num];

		b = isp_buf_remove(fe_out_q);
		if (b == NULL) {
			vi_pr(VI_ERR, "Pre_fe_%d chn_num_%d outbuf is empty\n", raw_num, chn_num);
			return;
		}

		if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 3) { //raw dump enablesou
			u32 w = (chn_num == ISP_FE_CH0) ?
				ctx->isp_pipe_cfg[raw_num].crop.w :
				ctx->isp_pipe_cfg[raw_num].crop_se.w;
			u32 h = (chn_num == ISP_FE_CH0) ?
				ctx->isp_pipe_cfg[raw_num].crop.h :
				ctx->isp_pipe_cfg[raw_num].crop_se.h;
			u32 dmaid;

			if (raw_num == ISP_PRERAW_A) {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL6 : ISP_BLK_ID_DMA_CTL7);
			} else if (raw_num == ISP_PRERAW_B) {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL12 : ISP_BLK_ID_DMA_CTL13);
			} else {
				dmaid = ((chn_num == ISP_FE_CH0) ? ISP_BLK_ID_DMA_CTL18 : ISP_BLK_ID_DMA_CTL19);
			}

			if (chn_num == ISP_FE_CH0)
				++vdev->dump_frame_number[raw_num];

			b->crop_le.x = b->crop_se.x = 0;
			b->crop_le.y = b->crop_se.y = 0;
			b->crop_le.w = b->crop_se.w = ctx->isp_pipe_cfg[raw_num].crop.w;
			b->crop_le.h = b->crop_se.h = ctx->isp_pipe_cfg[raw_num].crop.h;
			b->byr_size = ispblk_dma_get_size(ctx, dmaid, w, h);
			b->frm_num = vdev->pre_fe_frm_num[raw_num][chn_num];

			isp_buf_queue(raw_d_q, b);
		} else {
			m_info = ispblk_rgbmap_info(ctx, raw_num);
			b->rgbmap_i.w_bit = m_info.w_bit;
			b->rgbmap_i.h_bit = m_info.h_bit;

			m_info = ispblk_lmap_info(ctx, raw_num);
			b->lmap_i.w_bit = m_info.w_bit;
			b->lmap_i.h_bit = m_info.h_bit;

			b->is_yuv_frm	= 0;
			b->chn_num	= 0;

			isp_buf_queue(be_in_q, b);
		}

		atomic_set(&vdev->pre_fe_state[raw_num][chn_num], ISP_PRERAW_IDLE);

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			trigger = (vdev->pre_fe_frm_num[raw_num][ISP_FE_CH0] ==
					vdev->pre_fe_frm_num[raw_num][ISP_FE_CH1]);
		} else
			trigger = true;

		if (trigger) {
			//vi_pr(VI_DBG, "fe->dram->be->post trigger raw_num=%d\n", raw_num);
			if (atomic_read(&vdev->isp_raw_dump_en[raw_num]) == 3) { //raw dump flow
				_isp_raw_dump_chk(vdev, raw_num, b->frm_num);
			} else {
				tasklet_hi_schedule(&vdev->job_work);
			}
		}

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw) {
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on)
				_pre_hw_enque(vdev, raw_num, (chn_num == ISP_FE_CH0) ? ISP_FE_CH1 : ISP_FE_CH0);
			else
				_pre_hw_enque(vdev, raw_num, chn_num);
		}
	}
}

static inline void _isp_pre_be_done_handler(
	struct cvi_vi_dev *vdev,
	const u8 chn_num)
{
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	u32 type;
	u32 trigger = false;

	if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) { // fe->be->dram->post
		struct isp_buffer *b = NULL;
		struct isp_grid_s_info m_info;
		struct isp_queue *be_out_q = (chn_num == ISP_FE_CH0) ?
						&pre_be_out_q : &pre_be_out_se_q;
		struct isp_queue *post_in_q = (chn_num == ISP_FE_CH0) ?
						&post_in_queue : &post_in_se_queue;

		++vdev->pre_be_frm_num[raw_num][chn_num];
		type = (raw_num == ISP_PRERAW_A) ? VI_EVENT_PRE0_EOF : VI_EVENT_PRE1_EOF;

		vi_pr(VI_DBG, "pre_be frm_done chn_num=%d frm_num=%d\n",
				chn_num, vdev->pre_be_frm_num[raw_num][chn_num]);

		b = isp_buf_remove(be_out_q);
		if (b == NULL) {
			vi_pr(VI_ERR, "Pre_be chn_num_%d outbuf is empty\n", chn_num);
			return;
		}

		m_info = ispblk_rgbmap_info(ctx, raw_num);
		b->rgbmap_i.w_bit = m_info.w_bit;
		b->rgbmap_i.h_bit = m_info.h_bit;

		m_info = ispblk_lmap_info(ctx, raw_num);
		b->lmap_i.w_bit = m_info.w_bit;
		b->lmap_i.h_bit = m_info.h_bit;

		isp_buf_queue(post_in_q, b);

		//Pre_be done for tuning to get stt.
		_swap_pre_be_sts_buf(vdev, raw_num, chn_num);

		atomic_set(&vdev->pre_be_state[chn_num], ISP_PRE_BE_IDLE);

		if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw)
			_pre_hw_enque(vdev, raw_num, chn_num);

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			trigger = (vdev->pre_be_frm_num[raw_num][ISP_BE_CH0] ==
					vdev->pre_be_frm_num[raw_num][ISP_BE_CH1]);
		} else
			trigger = true;

		if (trigger) {
			vi_event_queue(vdev, type, vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);

			tasklet_hi_schedule(&vdev->job_work);
		}
	} else if (_is_be_post_online(ctx)) { // fe->dram->be->post
		struct isp_buffer *b = NULL;
		struct isp_queue *be_in_q;
		struct isp_queue *pre_out_q = NULL;

		if (ctx->isp_pipe_cfg[vdev->ctx.cam_id].is_synthetic_hdr_on)
			be_in_q = (chn_num == ISP_BE_CH0) ?
					&pre_be_in_syn_q : &pre_be_in_se_syn_q[vdev->ctx.cam_id];
		else
			be_in_q = (chn_num == ISP_BE_CH0) ?
					&pre_be_in_q : &pre_be_in_se_q[vdev->ctx.cam_id];

		if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
			b = isp_buf_remove(be_in_q);
			if (b == NULL) {
				vi_pr(VI_ERR, "Pre_be chn_num_%d input buf is empty\n", chn_num);
				return;
			}
			if (b->raw_num >= ISP_PRERAW_MAX) {
				vi_pr(VI_ERR, "buf raw_num_%d is wrong\n", b->raw_num);
				return;
			}
			raw_num = b->raw_num;
		}

		++vdev->pre_be_frm_num[raw_num][chn_num];
		type = (raw_num == ISP_PRERAW_A) ? VI_EVENT_PRE0_EOF : VI_EVENT_PRE1_EOF;

		vi_pr(VI_DBG, "pre_be_%d frm_done chn_num=%d frm_num=%d\n",
				raw_num, chn_num, vdev->pre_be_frm_num[raw_num][chn_num]);

		if (!ctx->isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
			pre_out_q = (chn_num == ISP_BE_CH0) ?
				&pre_out_queue[raw_num] : &pre_out_se_queue[raw_num];
			isp_buf_queue(pre_out_q, b);
		}

		atomic_set(&vdev->pre_be_state[chn_num], ISP_PRE_BE_IDLE);

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			trigger = (vdev->pre_be_frm_num[raw_num][ISP_BE_CH0] ==
					vdev->pre_be_frm_num[raw_num][ISP_BE_CH1]);
		} else
			trigger = true;

		if (trigger)
			vi_event_queue(vdev, type, vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);
	} else if (_is_all_online(ctx)) { // fly-mode
		++vdev->pre_be_frm_num[raw_num][chn_num];
		type = (raw_num == ISP_PRERAW_A) ? VI_EVENT_PRE0_EOF : VI_EVENT_PRE1_EOF;

		vi_pr(VI_DBG, "pre_be_%d frm_done chn_num=%d frm_num=%d\n",
				raw_num, chn_num, vdev->pre_be_frm_num[raw_num][chn_num]);

		//Pre_be done for tuning to get stt.
		_swap_pre_be_sts_buf(vdev, raw_num, chn_num);

		vi_event_queue(vdev, type, vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);
	} else if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) { // fe->be->dram->post
		++vdev->pre_be_frm_num[raw_num][chn_num];
		type = (raw_num == ISP_PRERAW_A) ? VI_EVENT_PRE0_EOF : VI_EVENT_PRE1_EOF;

		vi_pr(VI_DBG, "pre_be frm_done chn_num=%d frm_num=%d\n",
				chn_num, vdev->pre_be_frm_num[raw_num][chn_num]);

		//Pre_be done for tuning to get stt.
		_swap_pre_be_sts_buf(vdev, raw_num, chn_num);

		atomic_set(&vdev->pre_be_state[chn_num], ISP_PRE_BE_IDLE);

		if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
			trigger = (vdev->pre_be_frm_num[raw_num][ISP_BE_CH0] ==
					vdev->pre_be_frm_num[raw_num][ISP_BE_CH1]);
		} else
			trigger = true;

		if (trigger) {
			tasklet_hi_schedule(&vdev->job_work);

			vi_event_queue(vdev, type, vdev->pre_be_frm_num[raw_num][ISP_BE_CH0]);
		}
	}
}

static void _isp_postraw_shaw_done_handler(struct cvi_vi_dev *vdev)
{
	if (_is_fe_be_online(&vdev->ctx) && vdev->ctx.is_slice_buf_on) {
		_vi_wake_up_preraw_th(vdev, ISP_PRERAW_A);
	}
}

static void _isp_postraw_done_handler(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	enum cvi_isp_raw raw_num = ISP_PRERAW_A;
	u32 type = VI_EVENT_POST_EOF;

	if (_is_fe_be_online(ctx))
		raw_num = ctx->cam_id;

	if (_isp_clk_dynamic_en(vdev, false) < 0)
		return;

	++ctx->isp_pipe_cfg[raw_num].first_frm_cnt;

	if (_is_fe_be_online(ctx) && !ctx->is_slice_buf_on) { //fe->be->dram->post
		struct isp_buffer *ispb, *ispb_se;

		ispb = isp_buf_remove(&post_in_queue);
		if (ispb == NULL) {
			vi_pr(VI_ERR, "post_in_q is empty\n");
			return;
		}
		if (ispb->raw_num >= ISP_PRERAW_MAX) {
			vi_pr(VI_ERR, "buf raw_num_%d is wrong\n", ispb->raw_num);
			return;
		}
		raw_num = ispb->raw_num;

		if (ispb->is_yuv_frm) {
			isp_buf_queue(&pre_out_queue[ispb->chn_num], ispb);
		} else {
			//Update postraw stt gms/ae/hist_edge_v dma size/addr
			_swap_post_sts_buf(ctx, raw_num);

			isp_buf_queue(&pre_be_out_q, ispb);

			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on) {
				ispb_se = isp_buf_remove(&post_in_se_queue);
				if (ispb_se == NULL) {
					vi_pr(VI_ERR, "post_in_se_q is empty\n");
					return;
				}
				isp_buf_queue(&pre_be_out_se_q, ispb_se);
			}
		}
	} else if (_is_be_post_online(ctx)) {
		raw_num = ctx->cam_id;

		if (ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) {
			struct isp_buffer *b = NULL;
			struct isp_queue *be_in_q = &pre_be_in_q;
			struct isp_queue *pre_out_q = NULL;
			u8 chn_num = 0;

			b = isp_buf_remove(be_in_q);
			if (b == NULL) {
				vi_pr(VI_ERR, "pre_be_in_q is empty\n");
				return;
			}
			if (b->chn_num >= ISP_CHN_MAX) {
				vi_pr(VI_ERR, "buf chn_num_%d is wrong\n", b->chn_num);
				return;
			}
			chn_num = b->chn_num;

			pre_out_q = &pre_out_queue[chn_num];
			isp_buf_queue(pre_out_q, b);
		}
	} else if (_is_all_online(ctx) ||
		(_is_fe_be_online(ctx) && ctx->is_slice_buf_on)) {
		//Update postraw stt gms/ae/hist_edge_v dma size/addr
		_swap_post_sts_buf(ctx, raw_num);

		//Change post done flag to be true
		if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on)
			atomic_set(&vdev->ctx.is_post_done, 1);
	}

	atomic_set(&vdev->postraw_state, ISP_POSTRAW_IDLE);
	if (_is_be_post_online(ctx) && ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path)
		atomic_set(&vdev->pre_be_state[ISP_BE_CH0], ISP_PRE_BE_IDLE);

	++vdev->postraw_frame_number[raw_num];

	vi_pr(VI_DBG, "Postraw_%d frm_done frm_num=%d\n", raw_num, vdev->postraw_frame_number[raw_num]);

	if (!ctx->isp_pipe_cfg[raw_num].is_yuv_bypass_path) { //ISP team no need yuv post done
		if (isp_bufpool[raw_num].fswdr_rpt)
			ispblk_fswdr_update_rpt(ctx, isp_bufpool[raw_num].fswdr_rpt);

		if (raw_num == ISP_PRERAW_B)
			type = VI_EVENT_POST1_EOF;

		ctx->mmap_grid_size[raw_num] = ctx->isp_pipe_cfg[raw_num].rgbmap_i.w_bit;

		vi_event_queue(vdev, type, vdev->postraw_frame_number[raw_num]);
	}

	if (ctx->isp_pipe_cfg[raw_num].is_offline_scaler) {
		if (vdev->postraw_frame_number[raw_num] > gViCtx->bypass_frm[raw_num]) {
			cvi_isp_rdy_buf_remove2(vdev, raw_num);

			cvi_isp_dqbuf_list(vdev, vdev->postraw_frame_number[raw_num], raw_num);
			vdev->vi_th[E_VI_TH_EVENT_HANDLER].flag = raw_num + 1;
			vi_wake_up(&vdev->vi_th[E_VI_TH_EVENT_HANDLER]);
		}
	}

	if (!ctx->isp_pipe_cfg[raw_num].is_offline_preraw) {
		tasklet_hi_schedule(&vdev->job_work);

		if (!ctx->is_slice_buf_on) {
			if (ctx->isp_pipe_cfg[raw_num].is_hdr_on && ctx->isp_pipe_cfg[raw_num].is_synthetic_hdr_on) {
				if ((atomic_read(&vdev->pre_fe_state[raw_num][ISP_FE_CH0]) == ISP_PRERAW_IDLE) &&
				    (atomic_read(&vdev->pre_fe_state[raw_num][ISP_FE_CH1]) == ISP_PRERAW_IDLE))
					_pre_hw_enque(vdev, raw_num, ISP_FE_CH0);
			} else {
				_pre_hw_enque(vdev, raw_num, ISP_FE_CH0);
				if (ctx->isp_pipe_cfg[raw_num].is_hdr_on)
					_pre_hw_enque(vdev, raw_num, ISP_FE_CH1);
			}
		}
	}
}

void vi_irq_handler(struct cvi_vi_dev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_0 cbdg_0_sts, cbdg_0_sts_b, cbdg_0_sts_c;
	union REG_ISP_CSI_BDG_INTERRUPT_STATUS_1 cbdg_1_sts, cbdg_1_sts_b, cbdg_1_sts_c;
	union REG_ISP_TOP_INT_EVENT0 top_sts;
	union REG_ISP_TOP_INT_EVENT1 top_sts_1;
	union REG_ISP_TOP_INT_EVENT2 top_sts_2;
	u8 i = 0, raw_num = ISP_PRERAW_A;

	isp_intr_status(ctx, &top_sts, &top_sts_1, &top_sts_2);

	if (!atomic_read(&vdev->isp_streamon))
		return;

	vi_perf_record_dump();

	cbdg_0_sts.raw = cbdg_0_sts_b.raw = cbdg_1_sts.raw = cbdg_1_sts_b.raw = 0;
	cbdg_0_sts_c.raw = cbdg_1_sts_c.raw = 0;

	for (raw_num = ISP_PRERAW_A; raw_num < gViCtx->total_dev_num; raw_num++) {
		switch (raw_num) {
		case ISP_PRERAW_A:
		default:
			isp_csi_intr_status(ctx, raw_num, &cbdg_0_sts, &cbdg_1_sts);
			break;
		case ISP_PRERAW_B:
			isp_csi_intr_status(ctx, raw_num, &cbdg_0_sts_b, &cbdg_1_sts_b);
			break;
		case ISP_PRERAW_C:
			isp_csi_intr_status(ctx, raw_num, &cbdg_0_sts_c, &cbdg_1_sts_c);
			break;
		}
	}

	for (raw_num = ISP_PRERAW_A; raw_num < gViCtx->total_dev_num; raw_num++) {
		switch (raw_num) {
		case ISP_PRERAW_A:
		default:
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_0 = cbdg_0_sts.raw;
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_1 = cbdg_1_sts.raw;
			break;
		case ISP_PRERAW_B:
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_0 = cbdg_0_sts_b.raw;
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_1 = cbdg_1_sts_b.raw;
			break;
		case ISP_PRERAW_C:
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_0 = cbdg_0_sts_c.raw;
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_int_sts_1 = cbdg_1_sts_c.raw;
			break;
		}

		ctx->isp_pipe_cfg[raw_num].dg_info.fe_sts = ispblk_fe_dbg_info(ctx, raw_num);
		if (raw_num == ISP_PRERAW_A) {
			ctx->isp_pipe_cfg[raw_num].dg_info.be_sts = ispblk_be_dbg_info(ctx);
			ctx->isp_pipe_cfg[raw_num].dg_info.post_sts = ispblk_post_dbg_info(ctx);
			ctx->isp_pipe_cfg[raw_num].dg_info.dma_sts = ispblk_dma_dbg_info(ctx);
		}

		for (i = 0; i < ISP_FE_CHN_MAX; i++)
			ctx->isp_pipe_cfg[raw_num].dg_info.bdg_chn_debug[i] = ispblk_csibdg_chn_dbg(ctx, raw_num, i);
	}


	if (isp_err_chk(vdev, ctx, cbdg_0_sts, cbdg_1_sts, cbdg_0_sts_b, cbdg_1_sts_b,
		cbdg_0_sts_c, cbdg_1_sts_c) == -1)
		return;

	//if (top_sts.bits.INT_DMA_ERR)
	//	vi_pr(VI_ERR, "DMA error\n");

	/* pre_fe0 ch0 frame start */
	if (top_sts_2.bits.FRAME_START_FE0 & 0x1) {

		vi_record_sof_perf(vdev, ISP_PRERAW_A, ISP_FE_CH0);

		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw)
			++vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH0];

		vi_pr(VI_INFO, "pre_fe_%d sof chn_num=%d frm_num=%d\n",
				ISP_PRERAW_A, ISP_FE_CH0, vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH0]);

		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_yuv_bypass_path) { //RGB sensor
			if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_offline_preraw) {
				_isp_sof_handler(vdev, ISP_PRERAW_A);
				vi_event_queue(vdev, VI_EVENT_PRE0_SOF,
							vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH0]);
			}
		} else if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_A].is_offline_scaler) { //YUV sensor online mode
			//ISP team no need sof event by yuv sensor
			//_post_hw_enque(vdev);
			tasklet_hi_schedule(&vdev->job_work);
		}
	}

	/* pre_fe0 ch1 frame start */
	if (top_sts_2.bits.FRAME_START_FE0 & 0x2) {
		++vdev->pre_fe_sof_cnt[ISP_PRERAW_A][ISP_FE_CH1];

		//_isp_sof_handler(ISP_PRERAW_A);
	}

	/* pre_fe1 ch0 frame start */
	if (top_sts_2.bits.FRAME_START_FE1 & 0x1) {
		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_B].is_offline_preraw)
			++vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH0];

		vi_pr(VI_INFO, "pre_fe_%d sof chn_num=%d frm_num=%d\n",
				ISP_PRERAW_B, ISP_FE_CH0, vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH0]);


		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_B].is_yuv_bypass_path) { //RGB sensor
			_isp_sof_handler(vdev, ISP_PRERAW_B);
			vi_event_queue(vdev, VI_EVENT_PRE1_SOF, vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH0]);
		} else if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_B].is_offline_scaler) { //YUV sensor online mode
			//ISP team no need sof event by yuv sensor
			//_post_hw_enque(vdev);
			tasklet_hi_schedule(&vdev->job_work);
		}
	}

	/* pre_fe1 ch1 frame start */
	if (top_sts_2.bits.FRAME_START_FE1 & 0x2) {
		++vdev->pre_fe_sof_cnt[ISP_PRERAW_B][ISP_FE_CH1];

		//_isp_sof_handler(ISP_PRERAW_B);
	}

	/* pre_fe2 ch0 frame start */
	if (top_sts_2.bits.FRAME_START_FE2 & 0x1) {
		++vdev->pre_fe_sof_cnt[ISP_PRERAW_C][ISP_FE_CH0];
		 vi_pr(VI_INFO, "pre_fe_%d sof chn_num=%d frm_num=%d\n",
				ISP_PRERAW_C, ISP_FE_CH0, vdev->pre_fe_sof_cnt[ISP_PRERAW_C][ISP_FE_CH0]);

		if (!vdev->ctx.isp_pipe_cfg[ISP_PRERAW_C].is_offline_scaler) { //YUV sensor online mode
			//ISP team no need sof event by yuv sensor
			tasklet_hi_schedule(&vdev->job_work);
		}
	}

	/* pre_fe2 ch1 frame start */
	if (top_sts_2.bits.FRAME_START_FE1 & 0x2) {
		++vdev->pre_fe_sof_cnt[ISP_PRERAW_C][ISP_FE_CH1];

		//_isp_sof_handler(ISP_PRERAW_C);
	}

	if (!ctx->is_synthetic_hdr_on) {
		//HW limit
		//On hdr and stagger vsync mode, need to re-trigger pq vld after ch0 pq done.
		/* pre_fe0 ch0 pq done */
		if (top_sts_1.bits.PQ_DONE_FE0 & 0x1) {
			if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on &&
			    ctx->isp_pipe_cfg[ISP_PRERAW_A].is_stagger_vsync) {
				ISP_WR_BITS(ctx->phys_regs[ISP_BLK_ID_PRE_RAW_FE0],
					REG_PRE_RAW_FE_T, PRE_RAW_FRAME_VLD, FE_PQ_VLD_CH1, 1);
			}
		}

		/* pre_fe1 ch0 pq done */
		if (top_sts_1.bits.PQ_DONE_FE1 & 0x1) {
			if (ctx->isp_pipe_cfg[ISP_PRERAW_B].is_hdr_on &&
			    ctx->isp_pipe_cfg[ISP_PRERAW_B].is_stagger_vsync) {
				ISP_WR_BITS(ctx->phys_regs[ISP_BLK_ID_PRE_RAW_FE1],
					REG_PRE_RAW_FE_T, PRE_RAW_FRAME_VLD, FE_PQ_VLD_CH1, 1);
			}
		}
	}

	/* pre_fe0 ch0 frm_done */
	if (top_sts.bits.FRAME_DONE_FE0 & 0x1) {
		vi_record_fe_perf(vdev, ISP_PRERAW_A, ISP_FE_CH0);

		// In synthetic HDR mode, we assume that the first SOF is long exposure frames,
		// and the second SOF is short exposure frames.
		if (ctx->isp_pipe_cfg[ISP_PRERAW_A].is_hdr_on && ctx->isp_pipe_cfg[ISP_PRERAW_A].is_synthetic_hdr_on) {
			if (vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0] ==
			    vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH1]) { // LE
				++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0];

				_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH0);
			} else { // SE
				++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH1];

				_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH1);
			}
		} else {
			++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH0];

			_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH0);
		}
	}

	/* pre_fe0 ch1 frm_done */
	if (top_sts.bits.FRAME_DONE_FE0 & 0x2) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH1];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH1);
	}

	/* pre_fe0 ch2 frm_done */
	if (top_sts.bits.FRAME_DONE_FE0 & 0x4) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH2];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH2);
	}

	/* pre_fe0 ch3 frm_done */
	if (top_sts.bits.FRAME_DONE_FE0 & 0x8) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_A][ISP_FE_CH3];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_A, ISP_FE_CH3);
	}

	/* pre_fe1 ch0 frm_done */
	if (top_sts.bits.FRAME_DONE_FE1 & 0x1) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_B][ISP_FE_CH0];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_B, ISP_FE_CH0);
	}

	/* pre_fe1 ch1 frm_done */
	if (top_sts.bits.FRAME_DONE_FE1 & 0x2) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_B][ISP_FE_CH1];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_B, ISP_FE_CH1);
	}

	/* pre_fe2 ch0 frm_done */
	if (top_sts.bits.FRAME_DONE_FE2 & 0x1) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_C][ISP_FE_CH0];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_C, ISP_FE_CH0);
	}

	/* pre_fe2 ch1 frm_done */
	if (top_sts.bits.FRAME_DONE_FE2 & 0x2) {
		++vdev->pre_fe_frm_num[ISP_PRERAW_C][ISP_FE_CH1];

		_isp_pre_fe_done_handler(vdev, ISP_PRERAW_C, ISP_FE_CH1);
	}

	/* pre_be ch0 frm done */
	if (top_sts.bits.FRAME_DONE_BE & 0x1) {
		vi_record_be_perf(vdev, ISP_PRERAW_A, ISP_BE_CH0);

		_isp_pre_be_done_handler(vdev, ISP_BE_CH0);
	}

	/* pre_be ch1 frm done */
	if (top_sts.bits.FRAME_DONE_BE & 0x2) {
		_isp_pre_be_done_handler(vdev, ISP_BE_CH1);
	}

	/* post shadow up done */
	if (top_sts.bits.SHAW_DONE_POST) {
		if (_is_fe_be_online(ctx) && ctx->is_slice_buf_on) {
			_isp_postraw_shaw_done_handler(vdev);
		}
	}

	/* post frm done */
	if (top_sts.bits.FRAME_DONE_POST) {
		vi_record_post_end(vdev, ISP_PRERAW_A);

		_isp_postraw_done_handler(vdev);
	}
}

/*******************************************************
 *  Common interface for core
 ******************************************************/
int vi_create_instance(struct cvi_vi_dev *pdev)
{
	int ret = 0;
	struct cvi_vi_dev *vdev;
	struct mod_ctx_s  ctx_s;

	vdev = pdev;

	vi_set_base_addr(vdev->reg_base);

	ret = _vi_mempool_setup();
	if (ret) {
		vi_pr(VI_ERR, "Failed to setup isp memory\n");
		goto err;
	}

	ret = _vi_create_proc(vdev);
	if (ret) {
		vi_pr(VI_ERR, "Failed to create proc\n");
		goto err;
	}

	gViCtx = (struct cvi_vi_ctx *)vdev->shared_mem;
	g_vdev = vdev;

	_vi_init_param(vdev);

	ret = vi_create_thread(vdev, E_VI_TH_PRERAW);
	if (ret) {
		vi_pr(VI_ERR, "Failed to create preraw thread\n");
		goto err;
	}

	ret = vi_create_thread(vdev, E_VI_TH_ERR_HANDLER);
	if (ret) {
		vi_pr(VI_ERR, "Failed to create err_handler thread\n");
		goto err;
	}

	ctx_s.modID = CVI_ID_VI;
	ctx_s.ctx_num = 0;
	ctx_s.ctx_info = (void *)gViCtx;

	ret = base_set_mod_ctx(&ctx_s);
	if (ret) {
		vi_pr(VI_ERR, "Failed to set mod ctx\n");
		goto err;
	}

err:
	return ret;
}

int vi_destroy_instance(struct cvi_vi_dev *pdev)
{
	int ret = 0, i = 0;
	struct cvi_vi_dev *vdev;

	vdev = pdev;

	_vi_destroy_proc(vdev);

	for (i = 0; i < E_VI_TH_MAX; i++)
		vi_destory_thread(vdev, i);

	vi_tuning_buf_release();

	//for raw_dump
	for (i = 0; i < ISP_PRERAW_MAX; i++) {
		vdev->isp_int_flag[i]		= false;
		aos_event_free(&vdev->isp_int_wait_q[i].event);
		pthread_mutex_destroy(&vdev->isp_int_wait_q[i].lock);
		pthread_cond_destroy(&vdev->isp_int_wait_q[i].cond);
	}

	for (i = 0; i < ISP_PRERAW_MAX; i++) {
//TODO sync_task
#if 0
		sync_task_exit(i);
#endif
		free(isp_bufpool[i].fswdr_rpt);
		isp_bufpool[i].fswdr_rpt = 0;
	}

//TODO wait comebine postraw_thread
	//tasklet_kill(&vdev->job_work);
	aos_event_free(&vdev->job_work.event);
	pthread_mutex_destroy(&vdev->job_work.lock);
	pthread_cond_destroy(&vdev->job_work.cond);

	return ret;
}
