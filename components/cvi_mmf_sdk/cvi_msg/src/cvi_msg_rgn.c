#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "arch_helpers.h"

#include "cvi_type.h"
#include "cvi_mw_base.h"
#include "cvi_region.h"
#include "rgn_ioctl.h"
#include "cvi_ipcmsg.h"
#include "cvi_msg.h"
#include "msg_ctx.h"

static CVI_S32 MSG_RGN_Create(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_ATTR_S *pstRegion = (RGN_ATTR_S *)pstMsg->pBody;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstRegion);
	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_create(fd, Handle, pstRegion);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_Create call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_Create call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_Destroy(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_destroy(fd, Handle);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_Destroy call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_Destroy call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_GetAttr(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	RGN_ATTR_S stRegion;

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_get_attr(fd, Handle, &stRegion);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stRegion, sizeof(stRegion));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_GetAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_GetAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_SetAttr(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	RGN_ATTR_S *pstRegion = (RGN_ATTR_S *)pstMsg->pBody;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstRegion);
	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_set_attr(fd, Handle, pstRegion);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_SetAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_SetAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_SetBitMap(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	BITMAP_S *pstBitmap = (BITMAP_S *)pstMsg->pBody;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstBitmap);
	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_set_bit_map(fd, Handle, pstBitmap);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_SetBitMap call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_SetBitMap call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_AttachToChn(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;
	RGN_CHN_ATTR_S *pstChnAttr = (RGN_CHN_ATTR_S *)(pstMsg->pBody + sizeof(MMF_CHN_S));
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_attach_to_chn(fd, Handle, pstChn, pstChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_AttachToChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_AttachToChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);
	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_DetachFromChn(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);
	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_detach_from_chn(fd, Handle, pstChn);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_DetachFromChn call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_DetachFromChn call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_SetDisplayAttr(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;
	RGN_CHN_ATTR_S *pstChnAttr = (RGN_CHN_ATTR_S *)(pstMsg->pBody + sizeof(MMF_CHN_S));

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_set_display_attr(fd, Handle, pstChn, pstChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_SetDisplayAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_SetDisplayAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_GetDisplayAttr(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;
	RGN_CHN_ATTR_S stChnAttr;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_get_display_attr(fd, Handle, pstChn, &stChnAttr);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stChnAttr, sizeof(stChnAttr));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_GetDisplayAttr call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_GetDisplayAttr call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_GetCanvasInfo(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_CANVAS_INFO_S stCanvasInfo;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_get_canvas_info(fd, Handle, &stCanvasInfo);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &stCanvasInfo, sizeof(stCanvasInfo));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_GetCanvasInfo call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_GetCanvasInfo call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_UpdateCanvas(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	CVI_U32 u32Width = pstMsg->as32PrivData[1];
	CVI_U32 u32Height = pstMsg->as32PrivData[2];
	struct cvi_rgn_ctx *pCtx;

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_get_ctx_pointer(fd, Handle, &pCtx);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "rgn_get_ctx_pointer failed with s32Ret:%x\n", s32Ret);
		goto ERROR;
	}
	if (pCtx->stRegion.unAttr.stOverlay.stCompressInfo.enOSDCompressMode !=
		OSD_COMPRESS_MODE_NONE) {
		inv_dcache_range((uintptr_t)pCtx->stCanvasInfo[pCtx->canvas_idx].u64PhyAddr, 8);
		pCtx->stCanvasInfo[pCtx->canvas_idx].u32CompressedSize =
				*((CVI_U32 *)pCtx->stCanvasInfo[pCtx->canvas_idx].pu8VirtAddr + 1);
		// restore bitstream header to origin format
		*((CVI_U32 *)pCtx->stCanvasInfo[pCtx->canvas_idx].pu8VirtAddr + 1) =
		((((u32Width - 1) >> 1) & 0x7FFF) |
		(((u32Height - 1) << 15) & 0x7FFF8000));
		flush_dcache_range((uintptr_t)pCtx->stCanvasInfo[pCtx->canvas_idx].u64PhyAddr, 8);
	}

	s32Ret = rgn_update_canvas(fd, Handle);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

ERROR:
	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_UpdateCanvas call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_UpdateCanvas call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_Invert_Color(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;
	CVI_U32 *pu32Color = (CVI_U32 *)(pstMsg->pBody + sizeof(MMF_CHN_S));

	// not supported yet
	return CVI_ERR_RGN_NOT_SUPPORT;

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_invert_color(fd, Handle, pstChn, (void *)pu32Color);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
		return CVI_FAILURE;
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_Invert_Color call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_Invert_Color call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_SetChnPalette(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];
	MMF_CHN_S *pstChn = (MMF_CHN_S *)pstMsg->pBody;
	RGN_PALETTE_S *pstPalette = (RGN_PALETTE_S *)(pstMsg->pBody + sizeof(MMF_CHN_S));

	pstPalette->pstPaletteTable = (RGN_RGBQUARD_S *)(pstMsg->pBody + sizeof(MMF_CHN_S) + sizeof(RGN_PALETTE_S));

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_set_chn_palette(fd, Handle, pstChn, pstPalette);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
		return CVI_FAILURE;
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, NULL, 0);
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_SetChnPalette call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_SetChnPalette call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static CVI_S32 MSG_RGN_GetCtx(CVI_S32 s32Id, CVI_IPCMSG_MESSAGE_S *pstMsg)
{
	CVI_S32 fd = -1, s32Ret;
	CVI_IPCMSG_MESSAGE_S *respMsg = CVI_NULL;
	struct cvi_rgn_ctx ctx;
	RGN_HANDLE Handle = pstMsg->as32PrivData[0];

	// Driver control
	fd = get_rgn_fd();
	s32Ret = rgn_get_ctx(fd, Handle, &ctx);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "failed with s32Ret:%x\n", s32Ret);
	}

	respMsg = CVI_IPCMSG_CreateRespMessage(pstMsg, s32Ret, &ctx, sizeof(struct cvi_rgn_ctx));
	if (respMsg == CVI_NULL) {
		CVI_TRACE_MSG("RGN_GetCtx call CVI_IPCMSG_CreateRespMessage fail\n");
	}

	s32Ret = CVI_IPCMSG_SendOnly(s32Id, respMsg);
	if (s32Ret != CVI_SUCCESS) {
		CVI_TRACE_MSG("RGN_GetCtx call CVI_IPCMSG_SendOnly fail,ret:%x\n", s32Ret);
		CVI_IPCMSG_DestroyMessage(respMsg);
		return s32Ret;
	}

	CVI_IPCMSG_DestroyMessage(respMsg);

	return CVI_SUCCESS;
}

static MSG_MODULE_CMD_S g_stRgnCmdTable[] = {
	{ MSG_CMD_RGN_CREATE,              MSG_RGN_Create },
	{ MSG_CMD_RGN_DESTROY,             MSG_RGN_Destroy },
	{ MSG_CMD_RGN_GET_ATTR,            MSG_RGN_GetAttr },
	{ MSG_CMD_RGN_SET_ATTR,            MSG_RGN_SetAttr },
	{ MSG_CMD_RGN_SET_BITMAP,          MSG_RGN_SetBitMap },
	{ MSG_CMD_RGN_ATTACH_TO_CHN,       MSG_RGN_AttachToChn },
	{ MSG_CMD_RGN_DETACH_FROM_CHN,     MSG_RGN_DetachFromChn },
	{ MSG_CMD_RGN_SET_DISP_ATTR,       MSG_RGN_SetDisplayAttr },
	{ MSG_CMD_RGN_GET_DISP_ATTR,       MSG_RGN_GetDisplayAttr },
	{ MSG_CMD_RGN_GET_CANVAS_INFO,     MSG_RGN_GetCanvasInfo },
	{ MSG_CMD_RGN_UPDATE_CANVAS,       MSG_RGN_UpdateCanvas },
	{ MSG_CMD_RGN_INVERT_COLOR,        MSG_RGN_Invert_Color },
	{ MSG_CMD_RGN_SET_CHN_PALETTE,     MSG_RGN_SetChnPalette },
	{ MSG_CMD_RGN_GET_CTX,             MSG_RGN_GetCtx},
};

MSG_SERVER_MODULE_S g_stModuleRgn = {
	CVI_ID_RGN,
	"rgn",
	sizeof(g_stRgnCmdTable) / sizeof(MSG_MODULE_CMD_S),
	&g_stRgnCmdTable[0]
};

MSG_SERVER_MODULE_S *MSG_GetRgnMod(CVI_VOID)
{
	return &g_stModuleRgn;
}

