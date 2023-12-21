
#include <aos/kernel.h>
#include <errno.h>
#include "mailbox.h"
#include "ipcm.h"
#include "ipcm_port_common.h"
#include "mmio.h"

#define IPCM_MSG_BACKTRACE_LVL 4

int backtrace_now_get(void *trace[], int size, int offset);

typedef struct _ipcm_port_common_ctx {
    POOLHANDLE pool_shm;
	aos_mutex_t m_mutex[IPCM_DATA_SPIN_MAX];
	u32 pool_paddr;
	u32 pool_size;
#if IPCM_PROC_SUPPORT
	u32 pool_block_total;
	void **gtrace;
	void **ftrace;
	aos_mutex_t trace_mutex;
#endif
} ipcm_port_common_ctx;

static ipcm_port_common_ctx _port_ctx = {};

static u32 _init_status = 0;

int ipcm_print_pool_proc(void)
{
#if IPCM_PROC_SUPPORT
	int i,j = 0;
	void **trace;

	if (_port_ctx.gtrace) {
		ipcm_info("pool get trace:");
		for (i=0; i<_port_ctx.pool_block_total; i++) {
			ipcm_info("\nid:%d", i);
			trace = (void **)((char *)_port_ctx.gtrace + (i * IPCM_MSG_BACKTRACE_LVL * sizeof(void *)));
			for (j=0; j<IPCM_MSG_BACKTRACE_LVL; j++) {
				ipcm_info(" <- %p", trace[j]);
			}
		}
	}

	if (_port_ctx.ftrace) {
		ipcm_info("\npool free trace:");
		for (i=0; i<_port_ctx.pool_block_total; i++) {
			ipcm_info("\nid:%d", i);
			trace = (void **)((char *)_port_ctx.ftrace + (i * IPCM_MSG_BACKTRACE_LVL * sizeof(void *)));
			for (j=0; j<IPCM_MSG_BACKTRACE_LVL; j++) {
				ipcm_info(" <- %p", trace[j]);
			}
		}
	}
	ipcm_info("\n");
#endif
	return 0;
}

static int _ipcm_record_pool_bt(POOLHANDLE handle, void **trace_base, void *data)
{
#if IPCM_PROC_SUPPORT
	u32 pos;
	u32 block_idx;
	void **trace;

	if (data == NULL || trace_base == NULL) {
		ipcm_err("data is null.\n");
		return -EFAULT;
	}
	pos = pool_get_data_pos(_port_ctx.pool_shm, data);
	block_idx = pool_get_block_idx_by_pos(_port_ctx.pool_shm, pos);
	trace = (void **)((char *)trace_base + (block_idx * IPCM_MSG_BACKTRACE_LVL * sizeof(void *)));
	aos_mutex_lock(&_port_ctx.trace_mutex, AOS_WAIT_FOREVER);
	backtrace_now_get(trace, IPCM_MSG_BACKTRACE_LVL, 1);
	aos_mutex_unlock(&_port_ctx.trace_mutex);
#endif
	return 0;
}

static int _ipcm_record_pool_bt_bypos(POOLHANDLE handle, void **trace_base, u32 pos)
{
#if IPCM_PROC_SUPPORT
	u32 block_idx;
	void **trace;

	if (trace_base == NULL) {
		ipcm_err("data is null.\n");
		return -EFAULT;
	}

	block_idx = pool_get_block_idx_by_pos(_port_ctx.pool_shm, pos);
	trace = (void **)((char *)trace_base + (block_idx * IPCM_MSG_BACKTRACE_LVL * sizeof(void *)));
	aos_mutex_lock(&_port_ctx.trace_mutex, AOS_WAIT_FOREVER);
	backtrace_now_get(trace, IPCM_MSG_BACKTRACE_LVL, 1);
	aos_mutex_unlock(&_port_ctx.trace_mutex);
#endif
	return 0;
}

static s32 pool_get_param_from_sram(void)
{
	unsigned int tpu_sram_ipcm_base = TPU_SRAM_IPCM_BASE;
	if(mmio_read_32(tpu_sram_ipcm_base+0x38) == 0 || mmio_read_32(tpu_sram_ipcm_base + 0x3C) == 0)
	{
		ipcm_err("%s failed\n", __func__);
		return -1;
	}

	_port_ctx.pool_paddr = (u32)mmio_read_32(tpu_sram_ipcm_base + 0x38);
	_port_ctx.pool_size = (u32)mmio_read_32(tpu_sram_ipcm_base + 0x3C);
	ipcm_info("ipcm_pool_addr = 0x%x(0x%x), ipcm_pool_size = 0x%x(0x%x)\n",
			_port_ctx.pool_paddr, IPCM_POOL_ADDR, _port_ctx.pool_size, IPCM_POOL_SIZE);
	return 0;
}

s32 ipcm_port_common_init(BlockConfig *config, u32 num)
{
    int i = 0;
	u32 block_total;
	s32 ret = 0;

	if (_init_status) {
		ipcm_warning("ipcm port has been inited.\n");
		// return _port_ctx.port_fd;
		return 0;
	}

	ret = pool_get_param_from_sram();
	if (ret) {
		_port_ctx.pool_paddr = IPCM_POOL_ADDR;
		_port_ctx.pool_size = IPCM_POOL_SIZE;
	}

	inv_dcache_range(_port_ctx.pool_paddr, _port_ctx.pool_size);
	ret = ipcm_init(config, num, _port_ctx.pool_paddr, _port_ctx.pool_size);
	if (ret) {
		ipcm_err("ipcm_init failed.\n");
		return ret;
	}

	_port_ctx.pool_shm = (POOLHANDLE)(long)_port_ctx.pool_paddr;

	for (i=0; i<IPCM_DATA_SPIN_MAX; i++) {
		aos_mutex_new(&_port_ctx.m_mutex[i]);
	}

#if IPCM_PROC_SUPPORT
	block_total = pool_get_block_total(_port_ctx.pool_shm);
	_port_ctx.pool_block_total = block_total;

	_port_ctx.gtrace = malloc(block_total * IPCM_MSG_BACKTRACE_LVL * sizeof(void *));

	_port_ctx.ftrace = malloc(block_total * IPCM_MSG_BACKTRACE_LVL * sizeof(void *));

	aos_mutex_new(&_port_ctx.trace_mutex);
#endif
	ipcm_set_rtos_boot_bit(RTOS_IPCM_DONE, 1);

	_init_status = 1;

    return 0;
}

s32 ipcm_port_common_uninit(void)
{
	int i = 0;

	if (0 == _init_status) {
		ipcm_warning("ipcm port has not been inited.\n");
		return -EFAULT;
	}

	for (i=0; i<IPCM_DATA_SPIN_MAX; i++) {
		if (aos_mutex_is_valid(&_port_ctx.m_mutex[i]))
			aos_mutex_free(&_port_ctx.m_mutex[i]);
	}

#if IPCM_PROC_SUPPORT
	if (aos_mutex_is_valid(&_port_ctx.trace_mutex))
		aos_mutex_free(&_port_ctx.trace_mutex);
	if (_port_ctx.gtrace) {
		free(_port_ctx.gtrace);
		_port_ctx.gtrace = NULL;
	}
	if (_port_ctx.ftrace) {
		free(_port_ctx.ftrace);
		_port_ctx.ftrace = NULL;
	}
#endif
	ipcm_uninit();

	ipcm_set_rtos_boot_bit(RTOS_IPCM_DONE, 0);

	_init_status = 0;

    return 0;
}

s32 ipcm_release_buff_by_pos(u32 pos)
{
    s32 ret = 0;

    ret = pool_release_buff_by_pos(_port_ctx.pool_shm, pos);
    _ipcm_record_pool_bt_bypos(_port_ctx.pool_shm, _port_ctx.ftrace, pos);

    return ret;
}

s32 ipcm_inv_data(void *data, u32 size)
{
	inv_dcache_range(_port_ctx.pool_paddr + (data-_port_ctx.pool_shm), size);
    return 0;
}

s32 ipcm_flush_data(void *data, u32 size)
{
    flush_dcache_range(_port_ctx.pool_paddr+(data-_port_ctx.pool_shm), size);
    return 0;
}

s32 ipcm_data_lock(u8 lock_id)
{
	s32 ret = 0;
    u8 id = lock_id % IPCM_DATA_SPIN_MAX;

	aos_mutex_lock(&_port_ctx.m_mutex[id], AOS_WAIT_FOREVER);
	ret = ipcm_data_spin_lock(id);
	if (ret)
		aos_mutex_unlock(&_port_ctx.m_mutex[id]);

	return ret;
}

s32 ipcm_data_unlock(u8 lock_id)
{
	s32 ret = 0;
    u8 id = lock_id % IPCM_DATA_SPIN_MAX;

	ret = ipcm_data_spin_unlock(id);
	if (!ret)
		aos_mutex_unlock(&_port_ctx.m_mutex[id]);

	return ret;
}

void *ipcm_get_buff(u32 size)
{
	void *data;
	data = pool_get_buff(_port_ctx.pool_shm, size);
	_ipcm_record_pool_bt(_port_ctx.pool_shm, _port_ctx.gtrace, data);

	return data;
}

s32 ipcm_release_buff(void *data)
{
	s32 ret;

	ret = pool_release_buff(_port_ctx.pool_shm, data);
	_ipcm_record_pool_bt(_port_ctx.pool_shm, _port_ctx.ftrace, data);

	return ret;
}

s32 ipcm_data_packed(void *data, u32 len, MsgData *msg)
{
	if (data == NULL || msg == NULL) {
		ipcm_err("data or msg is null.\n");
		return -EFAULT;
	}
	msg->msg_param.msg_ptr.data_pos = pool_get_data_pos(_port_ctx.pool_shm, data);
	msg->msg_param.msg_ptr.remaining_rd_len = len;
	ipcm_flush_data(data, len);

    return 0;
}

void *ipcm_get_msg_data(MsgData *msg)
{
	if (msg == NULL) {
		ipcm_err("data is null.\n");
		return NULL;
	}
	return pool_get_data_by_pos(_port_ctx.pool_shm, msg->msg_param.msg_ptr.data_pos);
}

s32 ipcm_common_send_msg(MsgData *msg)
{
	if (msg == NULL) {
		ipcm_err("data is null.\n");
		return -1;
	}

	return ipcm_send_msg(msg);
}

void *ipcm_get_data_by_pos(u32 data_pos)
{
    return pool_get_data_by_pos(_port_ctx.pool_shm, data_pos);
}

void *ipcm_get_user_addr(u32 paddr)
{
    return (void *)(long)paddr;
}

u32 get_param_bin_addr(void)
{
	return *(u32 *)(_port_ctx.pool_shm + _port_ctx.pool_size - 4*4);
}

u32 get_param_bak_bin_addr(void)
{
	return *(u32 *)(_port_ctx.pool_shm + _port_ctx.pool_size - 4*3);
}

u32 get_pq_bin_addr(void)
{
	return *(u32 *)(_port_ctx.pool_shm + _port_ctx.pool_size - 4*2);
}

int ipcm_set_snd_cpu(int cpu_id)
{
	return mailbox_set_snd_cpu(cpu_id);
}

s32 ipcm_set_rtos_boot_bit(RTOS_BOOT_STATUS_E stage, u8 stat)
{
	u32 val;

	if (stage >= RTOS_BOOT_STATUS_BUTT) {
		ipcm_err("stage(%d) not equal, max(<%d)\n", stage, RTOS_BOOT_STATUS_BUTT);
		return -EINVAL;
	}

	val = *(uint32_t *)RTOS_BOOT_STATUS_REG;
	if (stat) {
		val |= (1 << stage);
	} else {
		val &= (~(1<<stage));
	}

	*(uint32_t volatile *)RTOS_BOOT_STATUS_REG = val;

	return 0;
}

s32 ipcm_get_rtos_boot_status(u32 *stat)
{
	if (NULL == stat) {
		ipcm_err("stat is null.\n");
		return -EINVAL;
	}
	*stat = *(uint32_t *)RTOS_BOOT_STATUS_REG;
	// ipcm_info("stat:%d\n", *stat);
	return 0;
}
