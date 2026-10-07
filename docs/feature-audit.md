# Native feature audit

This is the completion ledger for the independent native application. `PASS`
requires direct automated or visual evidence. `PENDING` means the implementation
exists but the required hardware or operating system has not been exercised.
A diagnostic drawing never counts as a Live2D visual pass.

## Current evidence

- Windows Cubism Release build: `build-delivery-final/Release/BongoCat.exe` - PASS.
- Native tests: `ctest --test-dir build-delivery-final -C Release` - PASS, 5/5.
- Source policy: `check-lines` - PASS; the default is 300 lines and reviewed
  file-specific limits are declared in `cmake/CheckLines.cmake`.
- Preferences matrix: `build-cubism/visual-audit-preferences/audit.csv` - PASS, 50/50.
- Real Live2D matrix: `build-cubism/visual-audit-cubism/audit.csv` - PASS, 14/14.
- Pointer/mouse-mirror matrix: `build-cubism/visual-audit-mouse/audit.csv` - PASS, 5/5.
- Visible Cubism memory baseline: PENDING remeasurement on the current Release
  build; working set and private committed memory are reported separately.
- Standalone smoke: 22 embedded shaders, `renderer=cubism-native`, GL error 0.
- Release ZIP and SHA-256 sidecar generation - PASS.
- Cubism Native SDK under `vendor/CubismSdkForNative` - PASS, SDK 5 r.5.
- Tauri-parity pointer mapping and frame-rate-independent damping - PASS.

## Models and rendering

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Three bundled model packages | PASS | Model tests validate MOC3, textures, settings, motions and expressions. |
| Background and key overlays | PASS | Standard, keyboard and gamepad idle/input screenshots match their intended composition. |
| Custom model import/remove | PASS | Valid import/remove smoke passes; malformed and unsafe manifests are rejected. |
| Model selection | PASS | Model catalog tests plus three real-model startup screenshots. |
| Real Cubism model rendering | PASS | All models report `renderer=cubism-native`; screenshots show the expected Tauri-parity composition. |
| Masks, blending and drawable order | PASS | Cubism r.5 renders alpha, masks, face, paws and overlays without OpenGL errors. |
| Motion playback and fades | PASS | Both bundled motions are accepted and differ from idle by 85.63% and 2.11%. |
| Expression playback | PASS | Expression 0 is accepted and differs from the same-run idle baseline. |
| Motion audio | PENDING | Audio loading is implemented; a manual audible check remains. |
| Mirror visual result | PASS | Real-model mirror differs from idle by 41.95%. |

## Input and behavior

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Keyboard left/right hands | PASS | Individual and simultaneous real frames differ from idle. |
| Mouse left/right buttons | PASS | Runtime parameters pass and real frames differ from idle. |
| Gamepad buttons and four axes | PASS | Runtime parameter assertions and real-frame differences pass. |
| Global Windows keyboard input | PASS | Injected events reach the native hook and alter overlays. |
| Global Windows mouse input | PASS | Injected down/up events reach the native input queue. |
| Global Windows pointer movement | PASS | Low-level hook coordinates, smoothing and parameter mapping are covered. |
| Pointer-driven Live2D tracking | PASS | Frames differ by 8.74%/9.18%; seven parameters and mirrored X/Z directions pass. |
| Windows automatic key release | PASS | Core input tests cover scheduled releases. |
| Gamepad hot plug | PENDING | SDL implementation exists; physical-device testing remains. |
| Behavior shortcuts | PENDING | Parsing and dispatch exist; a configured global shortcut smoke remains. |
| macOS input permission | PENDING | Native implementation exists; macOS manual verification remains. |

## Window and application

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Transparent borderless main window | PASS | Win32 caption, frame, menu and caption buttons are removed. |
| Preferences window | PASS | 50 screenshots contain no blank frames. |
| Click-through and always-on-top | PASS | Native style flags pass the window smoke. |
| Hide on hover | PASS | Enter, hide, pass-through, leave and restore are asserted. |
| Keep inside monitor | PASS | Off-screen placement is clamped in the geometry smoke. |
| Menu scale and opacity | PASS | Native size and opacity values are asserted. |
| Shift plus right-drag resize | PASS | Synthetic gesture verifies scale, size and release. |
| Capture window presence | PENDING | The pet is hidden from the taskbar/Dock while remaining eligible for OBS window capture; Windows is implemented and macOS/Linux need platform exercise. |
| System tray | PASS | Native SDL tray handle and state callbacks are asserted. |
| Right-click context menu | PASS | Production Win32 popup opens Preferences. |
| Single instance | PASS | A second process exits while the first remains alive. |
| Autostart | PENDING | Platform implementations exist; OS login testing remains. |
| Hidden rendering suspension | PASS | Main loop skips model updates while hidden. |

## Preferences, localization and data

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Cat, general, model, shortcuts and about pages | PASS | Current 50-view matrix. |
| Model card grid and direct-drawn icons | PASS | Responsive cards are present in both themes. |
| Simplified/Traditional Chinese, English, French, German, Japanese, Korean, Portuguese, Russian and Spanish | PASS | All ten locales pass shape, glyph, persistence and settings-window smoke checks. |
| Light and dark themes | PASS | Both themes are covered for every page and language. |
| System theme | PASS | Auto mode resolves through the native SDL system theme. |
| Click and drag model import | PASS | Both paths use the validated shared importer. |
| Atomic configuration persistence | PASS | Tests include truncated-write recovery. |
| No Electron or Tauri dependency | PASS | Native source/config scan is clean. |

## Performance and platform gates

| Requirement | Status | Evidence or remaining proof |
|---|---|---|
| Working set below 100 MiB | PENDING | Repeat the current Release measurement with working set and private committed memory reported separately. |
| Diagnostic idle CPU | PASS | Visible average 0.0324%; hidden average 0.0182%. |
| Cubism visible resource use | PASS | At the parity 60 FPS default: 0.94% total CPU on the current 4-CPU environment. |
| Five-minute bounded-memory runs | PASS | Visible, preferences and hidden modes remain bounded. |
| 24-hour bounded-memory run | PENDING | The harness exists; an uninterrupted result remains. |
| Windows 10 and 11 | PENDING | Current Windows passes; a second supported version remains. |
| macOS x64 and arm64 | PENDING | Requires macOS runners and signed application builds. |
| Linux X11 | PENDING | Requires a Linux runner with XInput2 and Xfixes. |
| Linux Wayland reduced-mode reporting | PENDING | Requires a Wayland session. |

The Windows implementation has no remaining Cubism or visual blocker. The
remaining `PENDING` rows require a current memory rebaseline, external
hardware, long-duration execution, or operating systems that are not present
on the current machine.
