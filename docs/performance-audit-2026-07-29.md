# Performance and package audit — 2026-07-29

## Scope

This pass reduces CPU, memory, GPU texture storage, and package size without
changing pixels, animation timing, frame rate, source resource quality, or
supported Windows/macOS/Linux behavior.

## Implemented changes

- Shadow masks use `GL_R8` with RGB=`GL_ONE`, A=`GL_RED` swizzling instead of
  storing redundant white RGB channels in `GL_RGBA8`.
- Same-color radial masks use the same single-channel path and retain the
  original color through draw tinting.
- Paint texture ownership/upload was separated into `ui_paint_cache` so paint
  algorithms and GPU cache policy remain independently maintainable.
- Preferences CPU buffers changed from 512/128 KiB to 256/96 KiB after 232
  captured frames showed historical peaks of 100,300/36,324 bytes. Full
  interaction peaked at 47,920/16,458 bytes.
- Four unreferenced legacy assets are excluded from Windows, macOS, and Linux
  runtime packages while remaining available in the source tree.
- Visual audit capture now moves the physical pointer outside the window to
  remove non-deterministic hover-state noise.

## Measured results

| Metric | Before | After | Result |
|---|---:|---:|---:|
| Preferences paint cache | 9,430,040 B | 3,367,808 B | -6,062,232 B (-64.3%) |
| Runtime-flow peak working set | 156.051 MiB | 143.973 MiB | -12.078 MiB |
| Runtime-flow peak private memory | 126.188 MiB | 114.707 MiB | -11.481 MiB |
| Preferences soak working set | 136.207 MiB | 131.574 MiB | -4.633 MiB |
| Preferences soak private memory | 112.555 MiB | 107.773 MiB | -4.782 MiB |
| Hidden soak working set | 107.715 MiB | 102.922 MiB | -4.793 MiB |
| Hidden soak private memory | 94.586 MiB | 92.383 MiB | -2.203 MiB |
| Active settings CPU, 3-run mean | 182.292 ms | 156.250 ms | -14.3% |
| Idle settings CPU, 3-run mean | 0.344% | 0% | no idle work |
| Embedded PAK | 2,245,899 B | 2,228,107 B | -17,792 B |
| Windows executable | 5,456,384 B | 5,438,464 B | -17,920 B |

## Visual and behavioral parity

- Fixed-input old/new comparison: 10/10 preferences captures were exactly
  identical across 6,120,000 compared pixels; MAE=0 and RMSE=0.
- Visual matrix: 53/53 views passed.
- Overlay matrix: 11/11 views passed; all 8 input scenarios passed.
- Preferences scenarios: 12/12 passed.
- Navigation: all 5 pages passed; P90 frame time 8.67 ms, max 10.29 ms.
- Focus audit passed, including dragging on the first press after focus loss.
- Cursor audit passed for all 11 cursor targets.
- Context menu audit passed with Settings first and all nested labels valid.
- Runtime flow passed all 17 stages with no handle growth or warm recovery
  memory growth.

## Cross-platform and risk decisions

- Shared rendering remains portable C/OpenGL 3.3; macOS keeps its OpenGL 4.1
  core context and the same R8/swizzle semantics.
- MSVC and MinGW Release builds enforce the 300-line policy.
- Runtime/package exclusions are mirrored in Windows PAK creation, macOS bundle
  staging, and Linux installation.
- UPX and whole-pack runtime compression remain intentionally rejected because
  they increase antivirus, signing, startup, and peak-memory risk.
- PNG and FLAC data were not recompressed: the current assets are already
  compressed, and no verified pixel/PCM-identical optimizer is part of the
  toolchain. Exact duplicate payloads remain deduplicated by the asset packer.

## Evidence

- `build-final/optimization-paint-r8-final`
- `build-final/visual-parity-paint-r8-after-stable`
- `build-final/optimization-visual-final`
- `build-final/optimization-overlay-final`
- `build-final/optimization-preference-scenarios-final`
- `build-final/optimization-navigation-final`
- `build-final/optimization-focus-final`
- `build-final/optimization-cursor-final`
- `build-final/optimization-context-menu-final`
- `build-final/optimization-soak-visible`
- `build-final/optimization-soak-hidden-final`
- `build-final/optimization-soak-preferences`
- `build-final/optimization-runtime-flow`
- `build-final/optimization-startup-performance`
