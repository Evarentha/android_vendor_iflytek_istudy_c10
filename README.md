<!--
iFLYTEK iStudy C10 Vendor Source Guide

Documents source layout, dependencies and coordinated image builds.

Authors:
worryzu <worryzu@gmail.com> @LinearTeam

Copyright (C) 2026 Evarentha
SPDX-License-Identifier: MIT
-->

# iFLYTEK iStudy C10 Vendor Source Tree

Vendor sources and compatibility components for the **iFLYTEK iStudy C10
(SPRD UD710-AB / ROC1)**, adapted for **Android 9 and Android Kernel 5.15.178**.
The sources support both standard and KernelSU-Next kernel builds.

## Mitochodria Interface Compatibility

The CM4 loader is installed as `/vendor/bin/mitochodria-sensorhub-boot`.
The kernel must provide
`/proc/mitochodria_cm4/ldinfo`, which supplies the addresses and sizes of the
main firmware and calibration library instead of a compiled-in board memory layout.
The matching DTB must contain `sprd,roc1-cm4-loader`.
Use matching boot, DTB, and vendor images when updating these interfaces.

The camera power module is `mitochodria_cam_power.ko`. External modules must
match the target kernel configuration and exported symbol CRCs. Factory HAL
and firmware paths retain their required protocol names.

## Directory Layout

| Directory | Contents |
|---|---|
| `sensorhub/` | Two-segment CM4 firmware loader, including factory TEE unlock and verification calls |
| `camera/` | Camera HAL and sensor port sources; bundled vendor libraries remain prebuilts (see `prebuilts/README.md`) |
| `graphics/` | HWC, GSP, and DPU userspace sources |
| `modules/camera-port/` | External camera, flash, power, and DVFS kernel module sources |
| `modules/npu-port/` | External NPU kernel module sources |
| `modules/wifi-port/` | External Broadcom Wi-Fi module sources |
| `compat/` | Android 9 compatibility headers used by the builds |
| `overlay/` | Init, ueventd, and other configuration overlays, relative to `/vendor` |
| `prebuilts/vendor/` | Matching PowerVR userspace binaries and GPU firmware |
| `image/` | Overlay installation plan, permissions, and SELinux labels |
| `build/` | Build and packaging tools, with ARM32/ARM64 build recipes |
| `provenance.json` | Per-file import sources and SHA256 hashes; historical provenance, not a runtime dependency |

## External Dependencies

Building requires the following external inputs:

- `--toolchain`: Clang 22.1.8 installation containing `bin/` and `lib/`. Required for compilation.
- `--android-source`: Platform headers and NDK r18 Android 28 CRT objects. The current recipes use the JingPad BSP source layout.
- `--factory`: Read-only factory extraction containing `vendor/` and `system/system/`. Supplies factory link libraries and unmodified vendor files.
- `--kernel-source` / `--kernel-output`: Matching Android Kernel 5.15 sources and a fully built `O=` directory containing `.config` and `Module.symvers`.
- `--image-tools`: Directory containing `mke2fs` and `e2fsdroid`. The host also needs `e2fsck`, `debugfs`, Python 3, and make.
- `--host-library-dir`: Optional extra host shared-library directory, repeatable. Use this only if your compiler or image tools need libraries outside the system search path. The existing `LD_LIBRARY_PATH` is preserved.

## Building

Clone this repository anywhere and run from its root, substituting your
dependency paths. No parent BSP workspace layout is required:

```sh
python3 build/build.py all \
  --output ../c8pro-vendor-build \
  --toolchain /path/to/clang-r22.1.8 \
  --kernel-source /path/to/kernel \
  --kernel-output /path/to/kernel-output \
  --android-source /path/to/jingpad \
  --factory /path/to/c8pro-boot-yuan \
  --image-tools /path/to/image-tools
```

Individual targets are available: `check`, `sensorhub`, `camera`, `graphics`,
`modules`, `stage`, and `pack`. All artifacts, module build copies, and logs
are written under the specified output directory; `.o` and `.ko` files are not
generated in the maintained source tree. Staging copies the read-only factory
tree and applies replacements and removals to that copy. Existing staging
trees and `vendor.img` files are not overwritten.

Relative paths are resolved from the directory where you invoke the command.
The output directory must be separate from this repository and all dependency
directories (neither inside them nor an ancestor of them). For example, use a
sibling build directory. Linux Kbuild still requires whitespace-free paths for
external module builds. Dependencies retain the internal layouts listed above;
this change makes their locations configurable, not their contents optional.

The `check` target needs only `--output`. Userspace targets require
`--toolchain`, `--android-source`, and `--factory`; `graphics` also requires
`--kernel-source`. The `modules` target requires `--toolchain` and
`--kernel-output`. Staging requires `--factory`; packing additionally requires
`--image-tools`. The `all` target requires all compilation and packaging inputs.

The resulting `vendor.img` is a 576716800-byte raw ext4 image. Packaging runs
e2fsck and reads back each file in the installation plan. Content hashes are
recorded in `artifact.json` and `stage/manifest.json`. Successful builds do
not replace on-device validation; changing build paths may also change ELF bytes.

## Development

Maintain source code, configuration overlays, and build recipes in this
repository. Keep generated artifacts in a separate output directory.
Kernel-side changes are maintained in the corresponding kernel repository.

## License

The [MIT license](LICENSE) applies only to files explicitly marked
`SPDX-License-Identifier: MIT`. Kernel-module code and third-party components
retain their respective licenses; the repository as a whole is not MIT-licensed.
See individual file headers and [prebuilt dependency notes](prebuilts/README.md).
