/*
 * Factory Camera Sensor ABI Checks
 *
 * Checks ARM32 and ARM64 sensor layouts against the factory library ABI.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: MIT
 */

#include "sensor_drv_u.h"
#include <stddef.h>

static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, module_id) == 8, "module ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, face_type) == 0x10, "facing ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, angle) == 0x14, "angle ABI");
#if __SIZEOF_POINTER__ == 4
static_assert(sizeof(PHYSICAL_SENSOR_INFO_T) == 124, "factory ARM32 stride");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, focus_eb) == 0x38, "focus ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, fov_info) == 0x40, "FOV ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, source_width_max) == 0x4c, "width ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, sensor_type) == 0x50, "type ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, sensor_name) == 0x56, "name ABI");
#else
static_assert(sizeof(PHYSICAL_SENSOR_INFO_T) == 144, "factory ARM64 stride");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, focus_eb) == 0x4c, "focus ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, fov_info) == 0x54, "FOV ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, source_width_max) == 0x60, "width ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, sensor_type) == 0x64, "type ABI");
static_assert(offsetof(PHYSICAL_SENSOR_INFO_T, sensor_name) == 0x6a, "name ABI");
#endif
