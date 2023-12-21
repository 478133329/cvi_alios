#include <stdio.h>
#include <string.h>
#include <time.h>
#include <cvi_base.h>
#include <cvi_mw_base.h>
#include "cvi_buffer.h"
#include <cvi_errno.h>
#include <vi_uapi.h>
#include <vo_uapi.h>
#include <vpss_uapi.h>
#include <rgn_uapi.h>
#include "cvi_debug.h"

struct vdev dev_isp, dev_vpss, dev_disp, dev_rgn;

/*
 * @param type: 0(isp) 1(img) 2(sc) 3(disp)
 * @param dev_id: dev id
 */
struct vdev *get_dev_info(CVI_U8 type, CVI_U8 dev_id)
{
	UNUSED(dev_id);

	if (type == VDEV_TYPE_ISP)
		return &dev_isp;
	else if (type == VDEV_TYPE_DISP)
		return &dev_disp;
	else if (type == VDEV_TYPE_RGN)
		return &dev_rgn;

	return NULL;
}

CVI_S32 get_vi_fd(CVI_VOID)
{
	if (dev_isp.fd <= 0)
		vi_dev_open();

	return dev_isp.fd;
}

CVI_S32 vi_dev_open(CVI_VOID)
{
	if (dev_isp.state != VDEV_STATE_OPEN) {
		// isp
		strncpy(dev_isp.name, "VI", sizeof(dev_isp.name));
		dev_isp.state = VDEV_STATE_CLOSED;
		vi_open();
		dev_isp.state = VDEV_STATE_OPEN;
	}

	return CVI_SUCCESS;
}

CVI_S32 vi_dev_close(CVI_VOID)
{
	if (dev_isp.state != VDEV_STATE_CLOSED) {
		vi_close();
		dev_isp.state = VDEV_STATE_CLOSED;
	}

	return CVI_SUCCESS;
}

CVI_S32 vpss_dev_open(CVI_VOID)
{
	CVI_S32 s32Ret = CVI_SUCCESS;

	if (dev_vpss.state != VDEV_STATE_OPEN) {
		snprintf(dev_vpss.name, sizeof(dev_vpss.name) - 1,
			 "/dev/cvi-vpss");

		vpss_open();
		dev_vpss.state = VDEV_STATE_OPEN;
	}

	return s32Ret;
}

CVI_S32 vpss_dev_close(CVI_VOID)
{
	CVI_S32 s32Ret = CVI_SUCCESS;

	if (dev_vpss.state != VDEV_STATE_CLOSED) {
		s32Ret = vpss_close();
		if (s32Ret != CVI_SUCCESS) {
			CVI_TRACE_SYS(CVI_DBG_ERR, "VPSS close failed\n");
			s32Ret = CVI_FAILURE;
			return s32Ret;
		}
		dev_vpss.state = VDEV_STATE_CLOSED;
	}

	return CVI_SUCCESS;
}


CVI_S32 get_rgn_fd(CVI_VOID)
{
	if (dev_rgn.fd <= 0)
		rgn_dev_open();

	return dev_rgn.fd;
}

CVI_S32 rgn_dev_open(CVI_VOID)
{
	if (dev_rgn.state != VDEV_STATE_OPEN) {
		// rgn
		strncpy(dev_rgn.name, "RGN", sizeof(dev_rgn.name));
		rgn_open();
		dev_rgn.state = VDEV_STATE_OPEN;
	}

	return CVI_SUCCESS;
}

CVI_S32 rgn_dev_close(CVI_VOID)
{
	if (dev_rgn.state != VDEV_STATE_CLOSED) {
		rgn_close();
		dev_rgn.state = VDEV_STATE_CLOSED;
	}

	return CVI_SUCCESS;
}

CVI_S32 get_vo_fd(CVI_VOID)
{
	if (dev_disp.fd <= 0)
		vo_dev_open();

	return dev_disp.fd;
}
CVI_S32 vo_dev_open(CVI_VOID)
{
	if (dev_disp.state != VDEV_STATE_OPEN) {
		// disp
		strncpy(dev_disp.name, "VO", sizeof(dev_disp.name));
		vo_open();
		dev_disp.state = VDEV_STATE_OPEN;
	}

	return CVI_SUCCESS;
}

CVI_S32 vo_dev_close(CVI_VOID)
{
	if (dev_disp.state != VDEV_STATE_CLOSED) {
		vo_close();
		dev_disp.state = VDEV_STATE_CLOSED;
	}

	return CVI_SUCCESS;
}

long _get_diff_in_us(struct timespec t1, struct timespec t2)
{
	struct timespec diff;

	if (t2.tv_nsec-t1.tv_nsec < 0) {
		diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
		diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
	} else {
		diff.tv_sec  = t2.tv_sec - t1.tv_sec;
		diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
	}
	return (diff.tv_sec * 1000000.0 + diff.tv_nsec / 1000.0);
}

