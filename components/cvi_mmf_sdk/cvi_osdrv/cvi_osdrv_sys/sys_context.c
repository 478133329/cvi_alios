#include "stdio.h"
#include "base_ctx.h"
#include "sys_context.h"
#include "sys.h"
#include "queue.h"
#include "cvi_errno.h"
#include "cvi_debug.h"

#define BIND_NODE_MAXNUM 64

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
		(var) && ((tvar) = TAILQ_NEXT((var), field), 1);	\
		(var) = (tvar))
#endif

#define CHN_MATCH(x, y) (((x)->enModId == (y)->enModId) && ((x)->s32DevId == (y)->s32DevId)             \
	&& ((x)->s32ChnId == (y)->s32ChnId))

struct bind_t {
	TAILQ_ENTRY(bind_t) tailq;
	BIND_NODE_S *node;
};

TAILQ_HEAD(bind_head, bind_t) binds;
static pthread_mutex_t bind_lock;

static struct sys_ctx_info ctx_info;
struct _BIND_NODE_S bind_nodes[BIND_NODE_MAXNUM];

extern struct cvi_venc_vb_ctx	venc_vb_ctx[VENC_MAX_CHN_NUM];

struct cvi_vdec_vb_ctx	vdec_vb_ctx[VENC_MAX_CHN_NUM];

int32_t sys_ctx_init(void)
{
	TAILQ_INIT(&binds);
	pthread_mutex_init(&bind_lock, NULL);
	memset(&ctx_info, 0, sizeof(struct sys_ctx_info));
	memset(bind_nodes, 0, sizeof(bind_nodes));

	return 0;
}

struct sys_ctx_info *sys_get_ctx(void)
{
	return &ctx_info;
}

VPSS_MODE_E sys_ctx_get_vpssmode(void)
{
	return ctx_info.mode_cfg.vpss_mode.enMode;
}

void *sys_ctx_get_sysinfo(void)
{
	return (void *)(&ctx_info.sys_info);
}

void sys_ctx_release_bind(void)
{
	struct bind_t *item, *item_tmp;

	pthread_mutex_lock(&bind_lock);
	TAILQ_FOREACH_SAFE(item, &binds, tailq, item_tmp) {
		TAILQ_REMOVE(&binds, item, tailq);
		free(item);
	}
	memset(bind_nodes, 0, sizeof(bind_nodes));
	pthread_mutex_unlock(&bind_lock);
}

int32_t sys_ctx_bind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn)
{
	struct bind_t *item, *item_tmp;
	int32_t ret = 0, i;

	CVI_TRACE_SYS(CVI_DBG_DEBUG, "src(mId=%d, dId=%d, cId=%d), dst(mId=%d, dId=%d, cId=%d)\n",
		pstSrcChn->enModId, pstSrcChn->s32DevId, pstSrcChn->s32ChnId,
		pstDestChn->enModId, pstDestChn->s32DevId, pstDestChn->s32ChnId);

	pthread_mutex_lock(&bind_lock);
	TAILQ_FOREACH_SAFE(item, &binds, tailq, item_tmp) {
		if (!CHN_MATCH(&item->node->src, pstSrcChn))
			continue;

		// check if dst already bind to src
		for (i = 0; i < item->node->dsts.u32Num; ++i) {
			if (CHN_MATCH(&item->node->dsts.astMmfChn[i], pstDestChn)) {
				CVI_TRACE_SYS(CVI_DBG_ERR, "Duplicate Dst(%d-%d-%d) to Src(%d-%d-%d)\n",
					pstDestChn->enModId, pstDestChn->s32DevId, pstDestChn->s32ChnId,
					pstSrcChn->enModId, pstSrcChn->s32DevId, pstSrcChn->s32ChnId);
				ret = -1;
				goto BIND_EXIT;
			}
		}
		// check if dsts have enough space for one more bind
		if (item->node->dsts.u32Num >= BIND_DEST_MAXNUM) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "Over max bind Dst number\n");
			ret = -1;
			goto BIND_EXIT;
		}
		item->node->dsts.astMmfChn[item->node->dsts.u32Num++] = *pstDestChn;

		goto BIND_SUCCESS;
	}

	// if src not found
	for (i = 0; i < BIND_NODE_MAXNUM; ++i) {
		if (!bind_nodes[i].bUsed) {
			memset(&bind_nodes[i], 0, sizeof(bind_nodes[i]));
			bind_nodes[i].bUsed = true;
			bind_nodes[i].src = *pstSrcChn;
			bind_nodes[i].dsts.u32Num = 1;
			bind_nodes[i].dsts.astMmfChn[0] = *pstDestChn;
			break;
		}
	}

	if (i == BIND_NODE_MAXNUM) {
		CVI_TRACE_SYS(CVI_DBG_ERR, "No free bind node\n");
		ret = -1;
		goto BIND_EXIT;
	}

	item = calloc(1,sizeof(*item));
	if (item == NULL) {
		memset(&bind_nodes[i], 0, sizeof(bind_nodes[i]));
		ret = CVI_ERR_SYS_NOMEM;
		goto BIND_EXIT;
	}

	item->node = &bind_nodes[i];
	TAILQ_INSERT_TAIL(&binds, item, tailq);

BIND_SUCCESS:
	ret = 0;

	if (pstDestChn->enModId == CVI_ID_VENC)
		venc_vb_ctx[pstDestChn->s32ChnId].enable_bind_mode = CVI_TRUE;
	else if (pstSrcChn->enModId == CVI_ID_VDEC)
		vdec_vb_ctx[pstSrcChn->s32ChnId].enable_bind_mode = CVI_TRUE;

BIND_EXIT:
	pthread_mutex_unlock(&bind_lock);

	return ret;
}

int32_t sys_ctx_unbind(MMF_CHN_S *pstSrcChn, MMF_CHN_S *pstDestChn)
{
	struct bind_t *item, *item_tmp;
	uint32_t i;

	pthread_mutex_lock(&bind_lock);
	TAILQ_FOREACH_SAFE(item, &binds, tailq, item_tmp) {
		if (!CHN_MATCH(&item->node->src, pstSrcChn))
			continue;

		for (i = 0; i < item->node->dsts.u32Num; ++i) {
			if (CHN_MATCH(&item->node->dsts.astMmfChn[i], pstDestChn)) {
				if (--item->node->dsts.u32Num) {
					for (; i < item->node->dsts.u32Num; i++)
						item->node->dsts.astMmfChn[i] = item->node->dsts.astMmfChn[i + 1];
				}

				if (pstDestChn->enModId == CVI_ID_VENC)
					venc_vb_ctx[pstDestChn->s32ChnId].enable_bind_mode = CVI_FALSE;
				else if (pstSrcChn->enModId == CVI_ID_VDEC)
					vdec_vb_ctx[pstSrcChn->s32ChnId].enable_bind_mode = CVI_FALSE;

				pthread_mutex_unlock(&bind_lock);
				return 0;
			}
		}
	}
	pthread_mutex_unlock(&bind_lock);
	return 0;
}

int32_t sys_ctx_get_bindbysrc(MMF_CHN_S *pstSrcChn, MMF_BIND_DEST_S *pstBindDest)
{
	struct bind_t *item, *item_tmp;
	uint32_t i;

	CVI_TRACE_SYS(CVI_DBG_DEBUG, "src(.enModId=%d, .s32DevId=%d, .s32ChnId=%d)\n",
		pstSrcChn->enModId, pstSrcChn->s32DevId, pstSrcChn->s32ChnId);

	pthread_mutex_lock(&bind_lock);
	TAILQ_FOREACH_SAFE(item, &binds, tailq, item_tmp) {
		for (i = 0; i < item->node->dsts.u32Num; ++i) {
			if (CHN_MATCH(&item->node->src, pstSrcChn)) {
				*pstBindDest = item->node->dsts;
				pthread_mutex_unlock(&bind_lock);
				return 0;
			}
		}
	}
	pthread_mutex_unlock(&bind_lock);
	return -1;
}

int32_t sys_ctx_get_bindbydst(MMF_CHN_S *pstDestChn, MMF_CHN_S *pstSrcChn)
{
	struct bind_t *item, *item_tmp;
	uint32_t i;

	CVI_TRACE_SYS(CVI_DBG_DEBUG, "dst(.enModId=%d, .s32DevId=%d, .s32ChnId=%d)\n",
		pstSrcChn->enModId, pstSrcChn->s32DevId, pstSrcChn->s32ChnId);

	pthread_mutex_lock(&bind_lock);
	TAILQ_FOREACH_SAFE(item, &binds, tailq, item_tmp) {
		for (i = 0; i < item->node->dsts.u32Num; ++i) {
			if (CHN_MATCH(&item->node->dsts.astMmfChn[i], pstDestChn)) {
				*pstSrcChn = item->node->src;
				pthread_mutex_unlock(&bind_lock);
				return 0;
			}
		}
	}
	pthread_mutex_unlock(&bind_lock);
	return -1;
}

