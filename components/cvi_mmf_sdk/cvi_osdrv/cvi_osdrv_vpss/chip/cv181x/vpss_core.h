#ifndef _VPSS_CORE_H_
#define _VPSS_CORE_H_

#include <vip_list.h>
#include <vip_atomic.h>
#include <vip_spinlock.h>

#include <vpss_uapi.h>
#include <cvi_vip_sc.h>
#include "scaler.h"
#include "vpss_common.h"
#include <vpss_cb.h>

/*vpss Interrupt*/
#define VPSS_INT 21


#define CVI_VIP_DRV_NAME "cvi_vip_driver"
#define CVI_VIP_DVC_NAME "cvi_vip_cv181x"

#define DEVICE_FROM_DTS 1

#define VIP_MAX_PLANES 3

#define VIP_CLK_RATIO_MASK(CLK_NAME) VIP_SYS_REG_CK_COEF_##CLK_NAME##_MASK
#define VIP_CLK_RATIO_OFFSET(CLK_NAME) VIP_SYS_REG_CK_COEF_##CLK_NAME##_OFFSET
#define VIP_CLK_RATIO_CONFIG(CLK_NAME, RATIO) \
	vip_sys_reg_write_mask(VIP_SYS_REG_CK_COEF_##CLK_NAME, \
		VIP_CLK_RATIO_MASK(CLK_NAME), \
		RATIO << VIP_CLK_RATIO_OFFSET(CLK_NAME))

/* Instance is already queued on the job_queue */
#define TRANS_QUEUED        (1 << 0)
/* Instance is currently running in hardware */
#define TRANS_RUNNING       (1 << 1)
/* Instance is currently aborting */
#define TRANS_ABORT         (1 << 2)

enum cvi_img_type {
	CVI_VIP_IMG_D = 0,
	CVI_VIP_IMG_V,
	CVI_VIP_IMG_MAX,
};

enum cvi_img_2_sc_bounding {
	CVI_VIP_IMG_2_SC_NONE = 0x00,
	CVI_VIP_IMG_2_SC_D = 0x01,
	CVI_VIP_IMG_2_SC_V = 0x02,
	CVI_VIP_IMG_2_SC_ALL = 0x03,
};

enum cvi_sc_type {
	CVI_VIP_SC_D = 0,
	CVI_VIP_SC_V1,
	CVI_VIP_SC_V2,
#ifdef __CV181X__
	CVI_VIP_SC_V3,
#endif
	CVI_VIP_SC_MAX,
};

enum cvi_vip_state {
	CVI_VIP_IDLE,
	CVI_VIP_RUNNING,
};

/* struct cvi_vip_fmt
 * @fourcc: pixel format.
 * @fmt: sclr driver's relative format.
 * @buffers: number of buffers.
 * @bit_depth: number of bits per pixel per plane.
 * @plane_sub_h: plane horizontal subsample.
 * @plane_sub_v: plane vertical subsample.
 */
struct cvi_vip_fmt {
	u32 fourcc;
	enum sclr_format fmt;
	u8  buffers;
	u32 bit_depth[VIP_MAX_PLANES];
	u8 plane_sub_h;
	u8 plane_sub_v;
};

/* buffer for one video frame */
struct vpss_img_buffer {
	u8 index;
	u64 phy_addr[3];
	struct list_head list;
};

/**
 * @rdy_queue: list of vpss_img_buffer
 * @rdy_lock: spinlock for rdy_queue
 * @num_rdy: number of buffer in rdy_queue
 * @fmt: format
 */
#define DEFINE_BASE_VDEV_PARAMS \
	struct list_head                rdy_queue;  \
	spinlock_t                      rdy_lock;   \
	u8                              num_rdy;

#define DEFINE_SC_VDEV_PARAMS \
	spinlock_t                      rdy_lock;   \
	struct list_head                rdy_queue[VPSS_ONLINE_NUM];  \
	u8                              num_rdy[VPSS_ONLINE_NUM];    \
	const struct cvi_vip_fmt         *fmt;      \
	u8				job_grp

struct cvi_base_vdev {
	DEFINE_BASE_VDEV_PARAMS;
};

/**
 * @dev_idx:  index of the device
 * @img_type: the type of this img_vdev
 * @sc_bounding: which sc instances are bounding with this img_vdev
 * @input_type: input type(isp, dwa, mem,...)
 * @job_flags: job status
 * @job_lock: lock of job
 * @src_size: img's input size
 * @crop_rect: img's output size, only work if input_type is mem
 */
struct cvi_img_vdev {
	//DEFINE_BASE_VDEV_PARAMS;
	struct list_head                rdy_queue;
	spinlock_t                      rdy_lock;
	u8                              num_rdy;

	// private data
	struct clk *clk;
	u8 dev_idx;
	enum sclr_img_in img_type;
	enum cvi_img_2_sc_bounding sc_bounding;
	enum cvi_input_type input_type;
	unsigned long job_flags;
	spinlock_t job_lock;
	//struct tasklet_struct job_work;
	pthread_t job_thread;
	pthread_mutex_t jobwork_lock;
	pthread_cond_t job_cond;

	struct cvi_vpss_frmsize src_size;
	struct cvi_vpss_rect crop_rect;
	struct cvi_vpss_grp_cfg vpss_grp_cfg[VPSS_ONLINE_NUM];
	u8 job_grp;	// snr_num
	u8 next_job_grp;

	bool is_tile;
	bool is_work_on_r_tile;
	u8 tile_mode;
	bool is_online_from_isp;
	bool isp_triggered;
	atomic_t is_streaming;
	struct sc_cfg_cb post_para;
	bool is_cmdq;
	u32 irq_cnt[VPSS_ONLINE_NUM];
	u32 isp_trig_cnt[VPSS_ONLINE_NUM];
	u32 isp_trig_fail_cnt[VPSS_ONLINE_NUM];
	u32 user_trig_cnt;
	u32 user_trig_fail_cnt;
	u32 overflow_cnt;
	struct timespec ts_end;
	uint64_t ts_start_us;
	uint64_t ts_end_us;
	uint64_t hw_duration_us;
	u32 frame_number[VPSS_ONLINE_NUM];
	u8 IntMask;

	struct sclr_img_in_sb_cfg img_sb_cfg;
	u64 sb_phy_addr[3];
};

/**
 * @dev_idx:  index of the device
 * @img_src:  bounding source of this sc_vdev
 * @sink_rect: sc's output rectangle, only if out-2-mem
 * @compose_out: sc's output size,
 * @crop_rect: sc's crop rectangle
 * @sc_coef: sc's scaling coefficient, such as bicubic, bilinear, ...
 * @tile_mode: sc's tile mode, both/left/right.
 * @is_streaming: to know if sc is stream-on.
 * @bind_fb: if sc use fb's gop.
 */
struct cvi_sc_vdev {
	DEFINE_SC_VDEV_PARAMS;

	// private data
	struct clk *clk;
	u8 dev_idx;
	enum cvi_img_type img_src;
	atomic_t job_state;

	struct cvi_vpss_rect sink_rect;
	struct cvi_vpss_rect compose_out;
	struct cvi_vpss_rect crop_rect;
	enum cvi_sc_scaling_coef sc_coef;

	struct cvi_vpss_chn_cfg vpss_chn_cfg[VPSS_ONLINE_NUM];

	u8 tile_mode;
	atomic_t is_streaming;
	atomic_t buf_empty[VPSS_ONLINE_NUM];
	bool bind_fb;
	bool is_cmdq;
	bool sb_enabled;
	bool sb_vc_ready;
	u64 sb_phy_addr[3];
};

/**
 * @irq_num_scl:  irq_num of scl got from dts.
 * @irq_num_dwa:  irq_num of dwa got from dts.
 * @irq_num_isp:  irq_num of isp got from dts.
 * @dwa_vdev:     dev of dwa.
 * @img_vdev:     dev of sc-image.
 * @sc_vdev:      dev of sc-core.
 * @disp_vdev:    dev of sc-disp.
 * @isp_vdev:     dev of isp.
 * @disp_online:  if sc-core and sc-disp is online(no frame-buf).
 */
struct cvi_vip_dev {
	//struct miscdevice miscdev;
	spinlock_t lock;
	pthread_mutex_t  mutex;

	unsigned int irq_num_scl;

	struct clk *clk_sys[3];
	struct clk *clk_sc_top;
	u32 clk_sys1_freq;

	struct cvi_img_vdev img_vdev[CVI_VIP_IMG_MAX];
	struct cvi_sc_vdev sc_vdev[CVI_VIP_SC_MAX];

	bool disp_online;

	void *shared_mem;
	sem_t wait_sem;
	pthread_cond_t wait;
	pthread_cond_t vi_reset_wait;
	pthread_mutex_t  wait_lock;    //for wait
	pthread_mutex_t  reset_lock;    //for vi_reset_wait
	bool reset_done;
	pthread_t          v_thread;     //vpss tid
	struct task_struct *thread;
};

struct vpss_sc_buffer {
	__u32 index;
	__u32 length;
	struct cvi_vip_plane planes[3];
	struct list_head list;
};

extern int request_irq(unsigned int irqn, void *handler, unsigned long flags, const char *name, void *priv);
const struct cvi_vip_fmt *cvi_vip_get_format(u32 pixelformat);
void cvi_vip_buf_queue(struct cvi_base_vdev *vdev, struct vpss_img_buffer *b);
struct vpss_img_buffer *cvi_vip_next_buf(struct cvi_base_vdev *vdev);
struct vpss_img_buffer *cvi_vip_buf_remove(struct cvi_base_vdev *vdev);
void cvi_vip_buf_cancel(struct cvi_base_vdev *vdev);
int cvi_vip_try_schedule(struct cvi_img_vdev *idev, u8 grp_id);
void cvi_vip_job_finish(struct cvi_img_vdev *idev);
bool cvi_vip_job_is_queued(struct cvi_img_vdev *idev);
int cvi_vip_set_rgn_cfg(const u8 inst, u8 layer, const struct cvi_rgn_cfg *cfg, const struct sclr_size *size);

extern bool __clk_is_enabled(struct clk *clk);
extern int debug;

void cvi_sc_frm_done_cb(struct cvi_vip_dev *dev);
void cvi_sc_trigger_post(void *arg);
struct cvi_vpss_ctx **vpss_get_shdw_ctx(void);
struct cvi_vip_dev *vpss_get_dev(void);

CVI_VOID vpss_img_sb_qbuf(struct cvi_img_vdev *idev, struct cvi_buffer *buf, struct vpss_grp_sbm_cfg *sbm_cfg);

int vpss_core_init(void);
int vpss_open(void);
int vpss_close(void);
void vpss_core_exit(void);

#endif /* _VPSS_CORE_H_ */
