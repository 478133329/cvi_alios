/**
 * @file ipcm_port_sys.c
 * @author allen.huang (allen.huang@cvitek.com)
 * @brief 
 * @version 0.1
 * @date 2023-01-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <aos/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ipcm_port_common.h"
#include "arch_helpers.h"
#include "ipcm_system.h"
#include "ipcm.h"

typedef struct _IPCM_SYS_CTX {
	unsigned int b_stop;
	unsigned int timeout;
} IPCM_SYS_CTX;

static aos_sem_t sem;

static IPCM_SYS_CTX _sys_ctx;

static s32 _sys_recv_handle(void *data)
{
	ipcm_debug("recv sys msg\n");
	aos_sem_signal(&sem);
	return 0;
}

extern int get_dump_uart_handle(void **handle, unsigned int *len);

static s32 _msg_proc_get_log(MsgData *msg, void *priv)
{
	void *uart_handle;
	u32 uart_len;
	s32 ret;
	MsgData _msg = {};

	ipcm_debug("%s %lx\n", __func__, *(unsigned long*)msg);
	ret = get_dump_uart_handle(&uart_handle, &uart_len);
	if (ret) {
		ipcm_err("get_dump_uart_handle fail ret:%d\n", ret);
		return ret;
	}

	ipcm_debug("%p %d\n", uart_handle, uart_len);

	_msg.port_id = PORT_SYSTEM;
	_msg.msg_id = IPCM_MSG_GET_LOG_RSP;
	_msg.func_type = MSG_TYPE_RAW_PARAM;
	_msg.msg_param.param = (unsigned long int)uart_handle;
	flush_dcache_range((uintptr_t)uart_handle, uart_len);

	return ipcm_send_msg(&_msg);
}

static s32 _msg_proc_get_sysinfo(MsgData *msg, void *priv)
{
	return 0;
}

static msg_proc_item _port_sys_proc_table[] = {
	{IPCM_MSG_GET_SYSINFO, _msg_proc_get_sysinfo},
	{IPCM_MSG_GET_LOG, _msg_proc_get_log},
};

static msg_proc_info _port_sys_proc_info = {
	PORT_SYSTEM,
	sizeof(_port_sys_proc_table) / sizeof(msg_proc_item),
	&_port_sys_proc_table[0]
};

static void _ipcm_sys_proc(void *arg)
{
	IPCM_SYS_CTX *ctx;
	MsgData tmsg;
	unsigned int timeout = 0;
	int ret = 0;
	MSGPROC_FN fn;

	if (arg == NULL) {
		ipcm_err("param err.\n");
		return;
	}

	ctx = (IPCM_SYS_CTX *)arg;
	timeout = ctx->timeout;

	while (!ctx->b_stop) {
		ret = aos_sem_wait(&sem, timeout);
		if (!ret) {
			while (!ipcm_read_msg(PORT_SYSTEM, &tmsg)) {
				ipcm_warning("resv msg:%lx\n", *(long *)&tmsg);
				fn = port_get_msg_fn(tmsg.msg_id, &_port_sys_proc_info);
				if (fn) {
					fn(&tmsg, NULL);
				}
			}
		}
	}

	return;
}

s32 ipcm_sys_init(void)
{
	s32 ret;

	ret = ipcm_port_init(PORT_SYSTEM, MSG_QUEUE_LEN, _sys_recv_handle);
	if (ret) {
		ipcm_err("ipcm_port_init failed.\n");
		return -EFAULT;
	}

	ret = aos_sem_new(&sem, 0);
	if (ret) {
		ipcm_err("sem new fail ret(%d).\n", ret);
	}
	ret = aos_sem_is_valid(&sem);
	if (ret == 0) {
		printf("sem is invalid\n");
	}

	_sys_ctx.timeout = 3000;
	_sys_ctx.b_stop = 0;
	ret = aos_task_new("sys proc", _ipcm_sys_proc, &_sys_ctx, 8192);
	if (ret) {
		ipcm_err("aos_task_new fail ret:%d\n", ret);
		return ret;
	}

	return ret;
}

s32 ipcm_sys_uninit(void)
{
	_sys_ctx.b_stop = 1;
	if (aos_sem_is_valid(&sem))
		aos_sem_free(&sem);
	return ipcm_port_uninit(PORT_SYSTEM);
}
