<!--
C8Pro Prebuilt Dependency Guide

Identifies binary inputs and their provenance; this license covers this document.

Authors:
worryzu <worryzu@gmail.com> @LinearTeam

Copyright (C) 2026 Evarentha
SPDX-License-Identifier: MIT
-->

# Prebuilt Dependencies

`vendor/` contains the PowerVR libraries and firmware imported from the matching
vendor image. Complete rebuildable sources for these binaries are not included.
Installation targets and SHA256 hashes are recorded in `../image/plan.json`;
original import locations and per-file hashes are recorded in `../provenance.json`.

The `.so` and `.a` files bundled under `camera/sensor/.../lib/` are also prebuilt
components. They are not part of the current C8Pro HAL link recipes and remain
at their original locations to preserve vendor source references. They should
not be confused with the rebuildable C/C++ sources.

Other unmodified factory HALs, DSP/CM4 firmware, and calibration executable
images are supplied through the explicit `--factory` input. Builds do not modify
the factory backup. Android platform headers and CRT objects come from
`--android-source`, while factory link libraries come from `--factory`.
