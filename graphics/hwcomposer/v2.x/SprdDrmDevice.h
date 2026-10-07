/*
 * SPRD DRM Device Selection
 *
 * Selects the SPRD display node by driver identity instead of probe order.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: MIT
 */

#ifndef SPRD_DRM_DEVICE_H
#define SPRD_DRM_DEVICE_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <xf86drm.h>

// Primary DRM minors are allocated in probe order. PVR may own card0;
// display, GSP and DPU ioctls must all go to the SPRD display driver.
static inline int sprdDrmOpenPath(const char *path) {
  int fd = open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0)
    return -1;

  drmVersionPtr version = drmGetVersion(fd);
  bool match = version && version->name && version->name_len == 4 &&
               !memcmp(version->name, "sprd", 4);
  if (version)
    drmFreeVersion(version);
  if (!match) {
    close(fd);
    errno = ENODEV;
    return -1;
  }

  ALOGI("C8DRM selected %s driver=sprd", path);
  return fd;
}

static inline int sprdDrmOpen() {
  char path[PROPERTY_VALUE_MAX];
  if (property_get("vendor.hwc.drm.device", path, "") > 0) {
    int fd = sprdDrmOpenPath(path);
    if (fd < 0)
      ALOGE("C8DRM invalid display override %s: %s", path, strerror(errno));
    return fd;
  }

  // Do not use drmOpen(name, NULL): legacy libdrm may reject a matching
  // primary node once another client has assigned it a bus ID.
  for (int minor = 0; minor < 64; ++minor) {
    snprintf(path, sizeof(path), "/dev/dri/card%d", minor);
    int fd = sprdDrmOpenPath(path);
    if (fd >= 0)
      return fd;
  }
  ALOGE("C8DRM no accessible SPRD primary node");
  errno = ENODEV;
  return -1;
}

#endif
