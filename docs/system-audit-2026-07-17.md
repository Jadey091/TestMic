# Systematic release audit - updated 2026-07-29

## Scope and result

This audit covers the native cross-platform application on the available
Windows host: startup and recovery, Cubism rendering, transparent-window
composition, preferences parity and interaction, tray/context menus, focus and
dragging, configuration persistence, resource use, source policy and standalone
delivery. The audited release executable is:

- `build-final/Release/BongoCat.exe`
- Size: 5,456,384 bytes
- SHA-256: `F924800D504356AD30034E975A2B9610DE489A109C2A39739E99756BC0B79037`

MSVC Release and MinGW Release both build successfully and pass 10/10 CTest
cases. No failed automated check is being waived in this report.

## Defects corrected

| Area | Correction | Verification |
|---|---|---|
| Overlay edges could develop dark/bright halos | Emit premultiplied RGBA and use independent RGB/alpha blending | 11/11 overlay captures, zero premultiply violations |
| Preferences alpha accumulated incorrectly | Keep straight-alpha color blending while composing destination alpha independently | Visual, interaction and scenario matrices |
| Software layer composition ignored destination alpha | Implement the complete source-over equation | Overlay release/input captures |
| Model textures rendered black or with inconsistent filtering | Match Cubism/Pixi linear no-mipmap filtering and retain bounded anisotropy | Three bundled model captures, `gl_error=0` |
| Font edges were visibly coarse | Bake horizontal and vertical 2x oversampling | Multilingual `1024x8192` atlas and 53/53 captures |
| Requested MSAA was assumed rather than measured | Record actual SDL sample buffers/count and assert both normal and fallback paths | Normal `1/4`, fallback `0/0` |
| Sidebar and shortcut audits depended on fragile desktop injection | Drive SDL-compatible window/input paths and add an internal shortcut smoke event | Ten navigation loops and six full interaction loops |
| Heavy-load capture could sample a stale/black frame | Retry completed-frame reads and validate capture color/black ratios | Stable visual and interaction runs |
| Tray preferences entry was below window toggles | Put Preferences first and assert the first SDL tray entry | Tray self-test plus context-menu capture |
| Inactive-window dragging needed an activation click | Accept the first press after focus loss | First press moved the window 56 x 32 px |
| Resource budgets no longer reflected the high-quality font atlas | Re-measure and set runtime-flow ceilings to 170/140 MiB | Runtime reopen/recovery flow |

## Automated evidence

| Area | Result |
|---|---|
| MSVC Release tests | PASS - 10/10 |
| MinGW Release tests | PASS - 10/10 |
| Source policy | PASS - checked handwritten files are at most 300 lines |
| PowerShell audit syntax | PASS - 30/30 scripts parse |
| Preferences visual matrix | PASS - 50/50: 5 pages x 5 locales x 2 themes |
| Main model visual matrix | PASS - standard, keyboard and gamepad |
| Current HTML pixel comparison | PASS - 98.4996% average similarity; minimum 97.9526% |
| Preferences scenarios | PASS - 12/12 |
| Preferences interaction | PASS - toggle, scroll, navigation, combo, locale, shortcut, debounce, formats and persistence |
| Sidebar stress | PASS - 10 loops, 50/50 selections |
| Cursor hit regions | PASS - 11/11 |
| Overlay/input states | PASS - 11/11 |
| Premultiplied alpha | PASS - 2,149 to 2,361 antialiased pixels per audited frame, zero violations |
| Tray/context menu | PASS - Preferences is the first item |
| Focus recovery drag | PASS - movement occurs on the first press |
| Startup recovery matrix | PASS - corruption, paths, fallback, diagnostics and second instance |

The pixel comparison uses the current native screenshots and the retained HTML
reference at 900 x 680. It is a measured visual-parity check, not a claim that
different operating-system text rasterizers will produce byte-identical pixels.

## Performance and stability

- Cold first valid frame: 1,192 ms.
- Warm first valid frame: 511 ms average, 550 ms P95.
- Preferences transition: 235.1 ms settled; 9.50 ms average frame, 15.20 ms P90.
- Preferences animation CPU: about 19.5%; settled idle CPU: 0%.
- Main 30-second soak: about +0.38 MiB trend, handles +1.
- Preferences 30-second soak: about +0.17 MiB trend, handles unchanged.
- Runtime flow, including settings close/reopen and model recovery, shows no
  continuing growth; measured peak was 156.188 MiB working set and 126.125 MiB
  private bytes, below the 170/140 MiB audit budgets.

## Cross-platform boundary

Shared behavior stays in portable SDL/OpenGL/C code. Windows-only focus,
startup and shell integration remain behind platform source boundaries; macOS
and Linux have their own platform units. CI defines warnings-as-errors builds,
tests and packages for Windows x64, Ubuntu Linux x64 and macOS.

The available host proves the MSVC and MinGW Windows paths. Real macOS/Linux GUI
behavior, compositor differences, permissions, signing and packaging remain
release gates and must not be represented as locally executed tests.

## Repeatable commands

- `cmake --build build-final --config Release --parallel 2`
- `ctest --test-dir build-final -C Release --output-on-failure`
- `ctest --test-dir build-mingw-final -C Release --output-on-failure`
- `cmake -DROOT=. -P cmake/CheckLines.cmake`

## External release gates

- Authenticode/notarization and platform-specific signing.
- Real Windows version coverage beyond this host.
- Real macOS and Linux GUI/compositor smoke tests.
- Physical gamepad hot-plug, audible output and an uninterrupted long soak.
