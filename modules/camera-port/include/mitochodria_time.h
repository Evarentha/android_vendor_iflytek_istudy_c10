/*
 * Mitochodria Camera Time Compatibility
 *
 * Adapts legacy camera timestamps to the Linux 5.15 time APIs.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MITOCHODRIA_CAMERA_TIME_H
#define MITOCHODRIA_CAMERA_TIME_H
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/time_types.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/compat.h>
#define ioremap_nocache ioremap

/* Preserve the LP64 legacy camera timeval layout at the ioctl boundary. */
#define timeval __kernel_old_timeval
static inline void mitochodria_camera_gettimeofday(struct __kernel_old_timeval *tv)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}
#define do_gettimeofday mitochodria_camera_gettimeofday
#define timespec timespec64
#define ktime_get_ts ktime_get_ts64
#define timespec_sub timespec64_sub
static inline struct __kernel_old_timeval mitochodria_ktime_to_timeval(ktime_t kt)
{
	s64 us = ktime_to_us(kt);
	struct __kernel_old_timeval tv;
	s32 remainder;

	tv.tv_sec = div_s64_rem(us, USEC_PER_SEC, &remainder);
	tv.tv_usec = remainder;
	return tv;
}
#define ktime_to_timeval mitochodria_ktime_to_timeval
#endif
