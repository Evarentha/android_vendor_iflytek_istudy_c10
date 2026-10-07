/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/compat.h>
#include <linux/fs.h>
#include <sprd_mm.h>
#include <sprd_isp_hw.h>
#include "sprd_img.h"
#include "compat_cam_drv.h"

long compat_sprd_img_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op->unlocked_ioctl)
		return -ENOTTY;
	if (cmd == COMPAT_SPRD_ISP_IO_CFG_PARAM)
		cmd = SPRD_ISP_IO_CFG_PARAM;
	/* The native handler copies the compat envelope into kernel storage. */
	return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
