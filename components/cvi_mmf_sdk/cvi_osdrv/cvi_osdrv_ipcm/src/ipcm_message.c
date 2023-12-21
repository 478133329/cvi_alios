
// #define _DEBUG

#include <aos/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ipcm_port_common.h"
#include "ipcm_message.h"
#include "ipcm.h"
#include "mailbox.h"
#include "arch_helpers.h"

typedef struct _ipcm_msg_ctx {
	aos_sem_t sem;
	MsgData cur_msg;
	u32 cur_data_pos;
	s32 pre_msg_data_pos;
	u8 open_recv_send_log;
} ipcm_msg_ctx;

u8 cur_msg_id;
unsigned long long t_recv;

static ipcm_msg_ctx _msg_ctx = {};

static s32 dev_recv_handle(void *data)
{
	if (_msg_ctx.open_recv_send_log || ipcm_log_level_debug()) {
		if (data)
			ipcm_info("alios recv msg:%lx\n", *(unsigned long int *)data);
		else
			ipcm_warning("alios recved msg, but data is null.\n");
	}
	t_recv = timer_get_boot_us();
	aos_sem_signal(&_msg_ctx.sem);
	return 0;
}

int ipcm_msg_set_rs_log_stat(u8 stat)
{
	_msg_ctx.open_recv_send_log = stat;
	return 0;
}

s32 ipcm_msg_init(void)
{
	int ret = 0;

	ret = ipcm_port_init(PORT_MSG, MSG_QUEUE_LEN, dev_recv_handle);
	if (ret) {
		ipcm_err("ipcm_port_init failed.\n");
		return -EFAULT;
	}
	ipcm_debug("sizeof(MsgData): %lu\n", sizeof(MsgData));

	ret = aos_sem_new(&_msg_ctx.sem, 0);
	if (ret) {
		ipcm_err("sem new fail ret(%d).\n", ret);
	}
	ret = aos_sem_is_valid(&_msg_ctx.sem);
	if (ret == 0) {
		printf("sem is invalid\n");
	}

	_msg_ctx.open_recv_send_log = 0;
	_msg_ctx.pre_msg_data_pos = -1;

	return 0;
}

s32 ipcm_msg_uninit(void)
{
	// release the last pool buff
	if (-1 != _msg_ctx.pre_msg_data_pos)
		ipcm_release_buff_by_pos(_msg_ctx.pre_msg_data_pos);

	if (aos_sem_is_valid(&_msg_ctx.sem))
		aos_sem_free(&_msg_ctx.sem);
	ipcm_port_uninit(PORT_MSG);
	return 0;
}

void *ipcm_msg_get_buff(u32 size)
{
	return ipcm_get_buff(size);
}

s32 ipcm_msg_release_buff(void *data)
{
	return ipcm_release_buff(data);
}

void *ipcm_msg_get_data(MsgData *data)
{
	return ipcm_get_msg_data(data);
}

s32 ipcm_msg_inv_data(void *data, u32 size)
{
	return ipcm_inv_data(data, size);
}

s32 ipcm_msg_flush_data(void *data, u32 size)
{
	return ipcm_flush_data(data, size);
}

s32 ipcm_msg_write_msg(MsgData *data)
{
	s32 ret = ipcm_send_msg(data);
	if (_msg_ctx.open_recv_send_log || ipcm_log_level_debug()) {
		if (data)
			ipcm_info("alios send msg:%lx\n", *(unsigned long int *)data);
		else
			ipcm_warning("alis send msg, but data is null.\n");
	}

	return ret;
}

s32 ipcm_msg_read_msg(MsgData *data)
{
	return ipcm_read_msg(PORT_MSG, data);
}

s32 ipcm_msg_poll(int32_t timeout)
{
	int ret = aos_sem_wait(&_msg_ctx.sem, timeout);
	if (ret) {
	    // ipcm_debug("sem wait fail ret(%d).\n", ret);
	}
	return ret;
}

s32 ipcm_msg_read_data(void **data, u32 len)
{
	s32 ret = -1;

	// ipcm_debug("remain_len(%d) cur_pos(%d)\n",
	// 	_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len, _msg_ctx.cur_data_pos);
	if (_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len != 0) {
		*data = ipcm_get_data_by_pos(_msg_ctx.cur_data_pos);
		if (len >= _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len) {
			s32 tmp = _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len;

			_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len = 0;
			// _msg_ctx.cur_data_pos += _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len;
			return tmp;
		}
		// len < _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len
		_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len -= len;
		_msg_ctx.cur_data_pos += len;
		return len;
	}
	{
		// _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len == 0
		ret = ipcm_msg_read_msg(&_msg_ctx.cur_msg);
		if (ret < 0) {
			// ipcm_err("read msg err.\n");
			return ret;
		}
		cur_msg_id = _msg_ctx.cur_msg.msg_id;
		if (_msg_ctx.cur_msg.func_type) {
			*data = (void *)(unsigned long)_msg_ctx.cur_msg.msg_param.param;
			_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len = 0;
			return sizeof(_msg_ctx.cur_msg.msg_param.param);
		}

		// _msg_ctx.pre_msg_data_pos will record previous data pos if it valid
		// after read msg success, release previous buff, ensure release one time
		if (-1 != _msg_ctx.pre_msg_data_pos) {
			ipcm_release_buff_by_pos(_msg_ctx.pre_msg_data_pos);
		}
		_msg_ctx.pre_msg_data_pos = _msg_ctx.cur_msg.msg_param.msg_ptr.data_pos;

		_msg_ctx.cur_data_pos = _msg_ctx.cur_msg.msg_param.msg_ptr.data_pos;
		*data = ipcm_get_data_by_pos(_msg_ctx.cur_data_pos);
		ipcm_inv_data(*data, _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len);
		if (len >= _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len) {
			s32 tmp = _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len;
			_msg_ctx.cur_data_pos += tmp;
			_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len = 0;
			return tmp;
		}
		// len < _msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len
		_msg_ctx.cur_msg.msg_param.msg_ptr.remaining_rd_len -= len;
		_msg_ctx.cur_data_pos += len;
		return len;
	}
	return ret;
}

s32 ipcm_msg_pack_data(void *data, u32 len, MsgData *msg)
{
	return ipcm_data_packed(data, len, msg);
}

s32 ipcm_msg_data_lock(u8 lock_id)
{
	return ipcm_data_lock(lock_id);
}

s32 ipcm_msg_data_unlock(u8 lock_id)
{
	return ipcm_data_unlock(lock_id);
}
