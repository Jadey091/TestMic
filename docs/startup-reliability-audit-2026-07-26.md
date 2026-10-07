# Startup and preferences reliability audit - updated 2026-07-29

## Scope and outcome

This audit covers the standalone native release on the available Windows host,
with emphasis on double-click startup, damaged-data recovery, first-frame
validity, preferences behavior, transparent rendering, resource stability and
portable paths. The audited binary is:

- `build-final/Release/BongoCat.exe`
- Source baseline before this final audit set: `378fd43`
- SHA-256: `F924800D504356AD30034E975A2B9610DE489A109C2A39739E99756BC0B79037`
- Size: 5,456,384 bytes

The MSVC and MinGW Release configurations each pass 10/10 CTest cases. No
finite suite can guarantee startup on every possible machine, so signing,
other Windows versions and real macOS/Linux machines remain explicit gates.

## Startup protections

| Failure class | Implemented protection |
|---|---|
| A second launch appeared to do nothing | Notify the primary process, restore it on-screen and raise the window. |
| Hidden or off-screen saved state | Recover a visible on-screen main window for interactive launch. |
| Invalid or partially written settings | Strict versioned JSON parsing, defaults on corruption and atomic writes. |
| Damaged extracted assets | Verify embedded assets by size and SHA-256 and repair the cache. |
| Damaged custom Live2D model | Validate packages before install/load and fall back to a bundled model. |
| Unavailable global hooks | Continue startup and persist an actionable diagnostic. |
| Unsupported multisampling | Retry without MSAA and prove the resulting sample count is zero. |
| Fatal graphics initialization | Persist startup stage and error logs instead of failing silently. |
| Deep, spaced or Unicode paths | Use bounded UTF-safe path handling and long-path-aware Windows I/O. |
| Large portable model trees | Apply depth, directory and time budgets without recursive stack growth. |
| Interrupted model import | Isolate temporary imports and clean stale residue on startup. |
| Stale first-frame evidence | Wait for a newly completed audit file and retry transient sharing failures. |

## Automated startup evidence

The test set covers core, i18n, UI, app state, model import units and formats,
optional real samples, portable startup soak, the startup reliability matrix
and preferences navigation.

The startup matrix independently verifies:

- fresh extraction and a visible nonblank first frame with `gl_error=0`;
- actual normal MSAA values of `sample_buffers=1`, `sample_count=4`;
- forced fallback values of `sample_buffers=0`, `sample_count=0`;
- same-size embedded-asset corruption and cache repair;
- malformed settings and malformed model recovery;
- hidden and far off-screen window recovery;
- unavailable global hooks;
- bounded portable-directory scanning;
- spaces, Unicode and paths longer than the legacy Windows `MAX_PATH` limit;
- long-path cache reuse, cleanup and model import;
- persisted fatal OpenGL diagnostics; and
- second-instance wake-up from hidden and tray-closed states without hanging.

All 30 PowerShell audit scripts parse successfully, `git diff --check` is clean
and the 300-line handwritten-source policy passes.

## Preferences verification

| Area | Result | Evidence |
|---|---|---|
| Visual matrix | PASS 53/53 | 50 preferences combinations plus 3 models |
| HTML reference parity | PASS | 98.4996% average pixel similarity; 97.9526% minimum page |
| Main interactions | PASS x6 | Toggle, scroll, navigation, combo, locale and shortcut editing |
| Sidebar navigation | PASS x10 | 50/50 clicks; all five pages selected every loop |
| Persistence | PASS | Locale, toggle, stepper and shortcut survive save/restart paths |
| Dialog/state scenarios | PASS 12/12 | Behavior, expression, Escape, outside-close, shortcut, locale, controls and toast |
| Pointer feedback | PASS 11/11 | Tabs, close, toggles, combo, shortcut, import/model cards and cancel |
| Focus recovery | PASS | First inactive-window press drags 56 x 32 px |
| Tray ordering | PASS | Preferences is the first tray entry |
| Localization fonts | PASS | Five locales, both themes, 2x oversampling, `1024x8192` atlas |

Configuration is intentionally split by ownership rather than retaining Tauri
compatibility: user preferences and runtime session state are separately
versioned and atomically persisted. The interaction audit validates both file
formats and debounce behavior.

## Rendering integrity

The model overlay now emits premultiplied RGBA and uses independent color/alpha
blend factors. Preferences retain straight-alpha color blending with an
independent alpha channel. The software compositor uses the complete
source-over equation, and model textures use linear no-mipmap filtering aligned
with the original renderer.

Across 11 model/input captures, each audited frame contains 2,149 to 2,361
semi-transparent antialiased pixels and zero RGB-greater-than-alpha violations.
Standard, keyboard and gamepad models, left/right/both hands, releases, stress
and gamepad-button states all pass.

## Performance and stability

- Cold first valid frame: 1,192 ms.
- Seven warm launches: 511 ms average, 550 ms P95.
- Navigation settles in 235.1 ms, with 9.50 ms average, 15.20 ms P90 and
  16.76 ms maximum audited frame time.
- Active preferences animation uses about 19.5% CPU on this host; idle is 0%.
- Main 30-second soak trends about +0.38 MiB with one additional handle.
- Preferences 30-second soak trends about +0.17 MiB with no handle growth.
- Full runtime close/reopen/recovery flow stays below 170 MiB working set and
  140 MiB private bytes and shows no continuing growth.

The larger font atlas is a deliberate quality/performance tradeoff. Baking
memory is temporary, runtime memory remains within measured budgets, and idle
rendering does not continue consuming CPU.

## Windows release properties

- x64 GUI executable with the static MSVC runtime; imported dependencies are
  Windows system components.
- ASLR, high-entropy VA, DEP/NX and Control Flow Guard are enabled.
- Large application state and startup catalogs live on the heap.
- The executable is not Authenticode-signed. Signing remains required before
  broad public distribution to establish provenance and reduce SmartScreen
  warnings.

## Cross-platform boundary

Portable configuration, rendering and bounded/atomic I/O are shared. Native
window, input, tray and startup behavior remain isolated by platform. CI defines
warnings-as-errors build/test/package jobs for Windows x64, Ubuntu Linux x64 and
macOS. This Windows host cannot honestly certify other systems' compositor,
permissions, signing or GUI behavior; their CI and real-machine smokes must pass
for every release candidate.

## Retained evidence

- `build-final/visual-audit-final-current/audit.csv`
- `build-final/parity-antialias-current/page0.json` through `page4.json`
- `build-final/overlay-audit-final-current-3/overlay-audit.csv`
- `build-final/preference-scenarios-final-current/audit.csv`
- `build-final/preferences-navigation-stable/result.json`
- `build-final/preferences-interaction-robust-6/result.json`
- `build-final/preferences-performance-current-2/result.json`
- `build-final/focus-current/result.json`
- `build-final/cursor-current/result.json`
- `build-final/context-menu-current/result.json`
- `build-final/runtime-flow-current/runtime-flow-summary.csv`

## Repeatable commands

- `cmake --build build-final --config Release --parallel 2`
- `ctest --test-dir build-final -C Release --output-on-failure`
- `ctest --test-dir build-mingw-final -C Release --output-on-failure`
- `cmake -DROOT=. -P cmake/CheckLines.cmake`
