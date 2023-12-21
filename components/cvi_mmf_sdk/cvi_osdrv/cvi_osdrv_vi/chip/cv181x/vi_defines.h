#ifndef __VI_DEFINES_H__
#define __VI_DEFINES_H__

#ifdef __cplusplus
	extern "C" {
#endif

#include "pthread.h"
#include "aos/kernel.h"
#include <vi_tun_cfg.h>
#include <vi_isp.h>
#include <vip/vi_drv.h>
#include <vip_list.h>
#include <vip_atomic.h>
#include <vip_spinlock.h>
#include <semaphore.h>

#define FPGA_TEST

enum E_VI_TH {
	E_VI_TH_PRERAW,
	E_VI_TH_ERR_HANDLER,
	E_VI_TH_EVENT_HANDLER,
	E_VI_TH_MAX
};

struct vi_thread_attr {
	char th_name[32];
	pthread_t          w_thread;
	atomic_t           thread_exit;
	atomic_t           exit_flag;
	//sem_t              sem;
	aos_event_t        event;
	pthread_mutex_t    lock;
	pthread_cond_t     cond;
	u32                flag;
	void *(*th_handler)(void *arg);
};

struct vi_gate {
	u32		reg;
	s8		shift;
	u64		flags;
};

struct vi_clk
{
	u32		id;
	const char 	*name;
	struct vi_gate	gate;
};

/**
 * struct cvi_vi - VI IP abstraction
 */
struct cvi_vi_dev {
	void __iomem			*reg_base;
	int				irq_num;
	void				*shared_mem;
	struct isp_ctx			ctx;
	struct cvi_isp_mbus_framefmt	usr_fmt;
	struct cvi_isp_rect		usr_crop;
	struct list_head		rdy_queue[ISP_PRERAW_MAX];
	spinlock_t			rdy_lock;
	u8				num_rdy[ISP_PRERAW_MAX];
	u8				chn_id;
	u64				usr_pic_phy_addr[3];
	unsigned long			usr_pic_delay;
	enum cvi_isp_source		isp_source;
	struct cvi_isp_snr_info		snr_info[ISP_PRERAW_MAX];
	atomic_t			isp_raw_dump_en[ISP_PRERAW_MAX];
	atomic_t			isp_smooth_raw_dump_en[ISP_PRERAW_MAX];
	u32				isp_int_flag[ISP_PRERAW_MAX];
	struct vi_thread_attr		isp_int_wait_q[ISP_PRERAW_MAX];
	struct vi_thread_attr		isp_dq_wait_q;
	struct vi_thread_attr		isp_event_wait_q;
	struct vi_thread_attr		isp_dbg_wait_q;
	atomic_t			isp_dbg_flag;
	atomic_t			isp_err_handle_flag;
	enum cvi_isp_raw		offline_raw_num;
	struct vi_thread_attr		job_work;
	struct list_head		qbuf_list[ISP_FE_CHN_MAX];
	spinlock_t			qbuf_lock;
	u8				qbuf_num[ISP_FE_CHN_MAX];
	u32				pre_fe_sof_cnt[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	u32				pre_fe_frm_num[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	u32				pre_be_frm_num[ISP_PRERAW_MAX][ISP_BE_CHN_MAX];
	bool				preraw_first_frm[ISP_PRERAW_MAX];
	u32				isp_err_times[ISP_PRERAW_MAX];
	u32				postraw_frame_number[ISP_PRERAW_MAX];
	u32				drop_frame_number[ISP_PRERAW_MAX];
	u32				dump_frame_number[ISP_PRERAW_MAX];
	u8				postraw_proc_num;
	atomic_t			pre_fe_state[ISP_PRERAW_MAX][ISP_FE_CHN_MAX];
	atomic_t			pre_be_state[ISP_BE_CHN_MAX];
	atomic_t			postraw_state;
	atomic_t			isp_streamoff;
	atomic_t			isp_streamon;
	atomic_t			ol_sc_frm_done;
	struct vi_thread_attr		vi_th[E_VI_TH_MAX];
};

#ifdef __cplusplus
}
#endif

#endif /* __VI_DEFINES_H__ */
