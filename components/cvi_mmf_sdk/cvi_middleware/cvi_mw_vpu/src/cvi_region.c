#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/queue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <inttypes.h>

#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>

#include "cvi_base.h"
#include "cvi_mw_base.h"
#include "cvi_sys.h"
#include "cvi_vpss.h"
#include "cvi_vo.h"
#include "cvi_region.h"
#include "rgn_ioctl.h"

/**************************************************************************
 *   Public APIs.
 **************************************************************************/
CVI_S32 CVI_RGN_Create(RGN_HANDLE Handle, const RGN_ATTR_S *pstRegion)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstRegion);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_create(fd, Handle, pstRegion) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Create RGN fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_Destroy(RGN_HANDLE Handle)
{
	CVI_S32 fd = -1;

	// Driver control
	fd = get_rgn_fd();
	if (rgn_destroy(fd, Handle) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Destroy RGN fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_GetAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRegion)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstRegion);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_get_attr(fd, Handle, pstRegion) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Get RGN attributes fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_SetAttr(RGN_HANDLE Handle, const RGN_ATTR_S *pstRegion)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstRegion);
	// Driver control
	fd = get_rgn_fd();
	if (rgn_set_attr(fd, Handle, pstRegion) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Set RGN attributes fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_SetBitMap(RGN_HANDLE Handle, const BITMAP_S *pstBitmap)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstBitmap);
	// Driver control
	fd = get_rgn_fd();
	if (rgn_set_bit_map(fd, Handle, pstBitmap) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Set RGN Bitmap fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_AttachToChn(RGN_HANDLE Handle, const MMF_CHN_S *pstChn, const RGN_CHN_ATTR_S *pstChnAttr)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);
	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChnAttr);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_attach_to_chn(fd, Handle, pstChn, pstChnAttr) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Attach RGN to channel fail.\n");
		return CVI_FAILURE;
	}
	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_DetachFromChn(RGN_HANDLE Handle, const MMF_CHN_S *pstChn)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_detach_from_chn(fd, Handle, pstChn) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Detach RGN from channel fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_SetDisplayAttr(RGN_HANDLE Handle, const MMF_CHN_S *pstChn, const RGN_CHN_ATTR_S *pstChnAttr)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);
	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChnAttr);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_set_display_attr(fd, Handle, pstChn, pstChnAttr) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Set display RGN attributes fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_GetDisplayAttr(RGN_HANDLE Handle, const MMF_CHN_S *pstChn, RGN_CHN_ATTR_S *pstChnAttr)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);
	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChnAttr);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_get_display_attr(fd, Handle, pstChn, pstChnAttr) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Get display RGN attributes fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_GetCanvasInfo(RGN_HANDLE Handle, RGN_CANVAS_INFO_S *pstCanvasInfo)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstCanvasInfo);

	// Driver control
	fd = get_rgn_fd();
	if (rgn_get_canvas_info(fd, Handle, pstCanvasInfo) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Get RGN canvas information fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_UpdateCanvas(RGN_HANDLE Handle)
{
	CVI_S32 fd = -1;

	// Driver control
	fd = get_rgn_fd();
	if (rgn_update_canvas(fd, Handle) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Update RGN canvas fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

/* CVI_RGN_Invert_Color - invert color per luma statistics of video content
 *   Chns' pixel-format should be YUV.
 *   RGN's pixel-format should be ARGB1555.
 *
 * @param Handle: RGN Handle
 * @param pstChn: the chn which rgn attached
 * @param pu32Color: rgn's content
 */
CVI_S32 CVI_RGN_Invert_Color(RGN_HANDLE Handle, MMF_CHN_S *pstChn, CVI_U32 *pu32Color)
{
	CVI_S32 fd = -1;

	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pstChn);
	MOD_CHECK_NULL_PTR(CVI_ID_RGN, pu32Color);

	// not supported yet
	return CVI_ERR_RGN_NOT_SUPPORT;

	// Driver control
	fd = get_rgn_fd();
	if (rgn_invert_color(fd, Handle, pstChn, (void *)pu32Color) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Invert RGN color fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}

CVI_S32 CVI_RGN_SetChnPalette(RGN_HANDLE Handle, const MMF_CHN_S *pstChn, RGN_PALETTE_S *pstPalette)
{
	struct vdev *d;

	// Driver control
	d = get_dev_info(VDEV_TYPE_RGN, 0);
	if (!IS_VDEV_OPEN(d->state)) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "rgn state(%d) incorrect.", d->state);
		return CVI_ERR_RGN_SYS_NOTREADY;
	}

	if (rgn_set_chn_palette(d->fd, Handle, pstChn, pstPalette) != CVI_SUCCESS) {
		CVI_TRACE_RGN(CVI_DBG_ERR, "Invert RGN color fail.\n");
		return CVI_FAILURE;
	}

	return CVI_SUCCESS;
}
