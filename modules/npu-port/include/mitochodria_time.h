/*
 * Mitochodria NPU Time Compatibility
 *
 * Maps internal legacy NPU timestamps to the Linux 5.15 time APIs.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef MITOCHODRIA_NPU_TIME_H
#define MITOCHODRIA_NPU_TIME_H
#include <linux/ktime.h>
#include <linux/timekeeping.h>
/* These timestamps are internal to the driver, never copied as an ioctl. */
#define timespec timespec64
#define timespec_valid timespec64_valid
#define timespec_compare timespec64_compare
#define getnstimeofday ktime_get_real_ts64
#endif
