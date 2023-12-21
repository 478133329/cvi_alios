
#include <aos/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ipcm_port_common.h"
#include "arch_helpers.h"
#include "ipcm_anonymous.h"
#include "ipcm.h"

typedef struct _ipcm_anon_ctx {
	unsigned int b_stop;
	unsigned int timeout;
	aos_sem_t sem;
	ANON_MSGPROC_FN handler;
	void *handler_data;
} ipcm_anon_ctx;

static ipcm_anon_ctx _anon_ctx = {};

static s32 _anon_recv_handle(void *data)
{
	ipcm_debug("recv sys msg\n");
	aos_sem_signal(&_anon_ctx.sem);
	return 0;
}

static void _ipcm_anon_proc(void *arg)
{
	ipcm_anon_ctx *ctx;
	MsgData tmsg;
	unsigned int timeout = 0;
	int ret = 0;

	if (arg == NULL) {
		ipcm_err("param err.\n");
		return;
	}

	ctx = (ipcm_anon_ctx *)arg;
	timeout = ctx->timeout;

	while (!ctx->b_stop) {
		ret = aos_sem_wait(&_anon_ctx.sem, timeout);
		if (!ret) {
			while (!ipcm_read_msg(PORT_ANON_ST, &tmsg)) {
				ipcm_debug("resv msg:%lx\n", *(long *)&tmsg);
				if (ctx->handler && (tmsg.port_id >= PORT_ANON_ST) && (tmsg.port_id < PORT_BUTT)) {
					ipcm_anon_msg_t anon_msg;
					anon_msg.port_id = tmsg.port_id;
					anon_msg.msg_id = tmsg.msg_id;
					anon_msg.data_type = tmsg.func_type;
					if (tmsg.func_type == MSG_TYPE_SHM) {
						anon_msg.data = ipcm_get_data_by_pos(tmsg.msg_param.msg_ptr.data_pos);
						anon_msg.size = tmsg.msg_param.msg_ptr.remaining_rd_len;
						ipcm_inv_data(anon_msg.data, anon_msg.size);
					} else {
						anon_msg.data = (void *)(unsigned long)tmsg.msg_param.param;
						anon_msg.size = 4;
					}
					ctx->handler(ctx->handler_data, &anon_msg);
				} else {
					ipcm_warning("anon recv param err, handle(%p), port id(%u) msg id(%u)\n",
						ctx->handler, tmsg.port_id, tmsg.msg_id);
				}
				// release pool buff
				if (tmsg.func_type == MSG_TYPE_SHM) {
					ipcm_release_buff_by_pos(tmsg.msg_param.msg_ptr.data_pos);
				}
			}
		}
	}

	return;
}

s32 ipcm_anon_init(void)
{
	int ret = 0;

	ret = ipcm_port_init(PORT_ANON_ST, MSG_QUEUE_LEN, _anon_recv_handle);

	ret = aos_sem_new(&_anon_ctx.sem, 0);
	if (ret) {
		ipcm_err("sem new fail ret(%d).\n", ret);
	}
	ret = aos_sem_is_valid(&_anon_ctx.sem);
	if (ret == 0) {
		ipcm_err("sem is invalid\n");
	}

	_anon_ctx.timeout = 3000;
	_anon_ctx.b_stop = 0;
	ret = aos_task_new("anon proc", _ipcm_anon_proc, &_anon_ctx, 8192);
	if (ret) {
		ipcm_err("aos_task_new fail ret:%d\n", ret);
		return ret;
	}

    return ret;
}

s32 ipcm_anon_uninit(void)
{
	_anon_ctx.b_stop = 1;
	if (aos_sem_is_valid(&_anon_ctx.sem))
		aos_sem_free(&_anon_ctx.sem);
	return ipcm_port_uninit(PORT_ANON_ST);
}

s32 ipcm_anon_register_handle(ANON_MSGPROC_FN handler, void *data)
{
    if (handler) {
        _anon_ctx.handler = handler;
        _anon_ctx.handler_data = data;
        return 0;
    }
    ipcm_err("ipcm_anon_register_handle handler is null.\n");
    return -EINVAL;
}

s32 ipcm_anon_deregister_handle(void)
{
    _anon_ctx.handler = NULL;
    _anon_ctx.handler_data = NULL;
    return 0;
}

s32 ipcm_anon_send_msg(MsgData *msg)
{
	if (msg == NULL) {
		ipcm_err("data is null.\n");
		return -EINVAL;
	}
    if (msg->port_id < PORT_ANON_ST || msg->port_id >= PORT_BUTT) {
        ipcm_err("port_id(%u) out of range, min(%u) max(%u).\n", msg->port_id,
            PORT_ANON_ST, PORT_BUTT-1);
        return -EINVAL;
    }

	return ipcm_send_msg(msg);
}

s32 ipcm_anon_send_param(u8 port_id, u8 msg_id, u32 param)
{
    MsgData tmsg = {};

    if (port_id < PORT_ANON_ST || port_id >= PORT_BUTT) {
        ipcm_err("port_id(%u) out of range, min(%u) max(%u).\n", port_id,
            PORT_ANON_ST, PORT_BUTT-1);
        return -EINVAL;
    }

    tmsg.port_id = port_id;
    tmsg.msg_id = msg_id;
    tmsg.func_type = MSG_TYPE_RAW_PARAM;
    tmsg.msg_param.param = param;

    return ipcm_send_msg(&tmsg);
}

