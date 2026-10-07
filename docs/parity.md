# Native completion contract

The standalone native application is complete only when each automated check
and the matching manual scenario pass without depending on another frontend or
desktop runtime.

## Model and rendering

- Load all three bundled Cubism `model3` models.
- Import and remove custom model directories without changing their layout.
- Render drawable order, masks, blend modes, opacity, multiply and screen color.
- Run `motion3` motions, `exp3` expressions, fade timing, and motion audio.
- Preserve mouse, keyboard, and gamepad parameter mappings.
- Draw the model background and at most one key overlay per hand.
- Preserve mirror, mouse mirror, model scale, opacity, radius, and FPS controls.

## Input

- Observe global keyboard press and release events.
- Observe global mouse buttons and coalesced pointer movement.
- Preserve Windows automatic release timing for system keys.
- Support SDL game controllers, sticks, triggers, and hot plug.
- Register behavior shortcuts and application shortcuts.
- Request the required macOS input-monitoring permission.

## Window and application

- Transparent, borderless, draggable main window with per-pixel alpha.
- Click-through, always-on-top, hide-on-hover, and keep-in-monitor behavior.
- Shift plus secondary drag resizing from 10 through 500 percent.
- Tray and context menus, capture-friendly taskbar/Dock suppression, and single instance handling.
- Restore window position and size across monitor and DPI changes.
- Autostart, project information, and application exit.

## Preferences and data

- General, cat, model, behavior, and shortcut preference views.
- Simplified Chinese, Traditional Chinese, English, French, German, Japanese,
  Korean, Portuguese, Russian, and Spanish.
- Light, dark, and system themes.
- Persist native settings and custom models without external project paths.
- Build, test, run, and package with only files contained in this repository.
- Persist settings atomically and recover from a truncated write.

## Release gates

- Golden images for idle, key, mouse, controller, motion, and expression states.
- Recorded input traces produce equivalent parameter values in old and new apps.
- Windows 10/11, macOS x64/arm64, and Linux X11 manual matrix passes.
- Wayland reports reduced global-input support instead of silently failing.
- Idle CPU is below 0.3 percent on the reference machine.
- Working set is below 100 MiB with one bundled model active.
- Hidden rendering is suspended and a 24-hour run has no unbounded growth.
- Every hand-written file passes the configured source-size policy, whose
  default is 300 physical lines with reviewed file-specific limits.

## Cubism Viewer animation equivalence

Live2D Cubism Viewer 5.3.03 is the animation and interaction baseline for every
model. Motion curves, expression blending, physics, eye blink, pose, idle
motion, and pointer-driven look updates use the official Cubism Framework
order and behavior. Source format never selects a second animation runtime.
Viewer mouse tracking feeds its configured `ParamAngleX`, `ParamAngleY`,
`ParamEyeBallX`, `ParamEyeBallY`, and, when present, `ParamBodyAngleX` channels
through the Viewer's own `TargetPoint` variant before physics consumes them at
the `physics3` authored FPS. That variant uses a `7.2727275 / 30` maximum target
speed, a 0.15 second acceleration time, a pixel-scale desktop dead zone, and
the Viewer's additional 1.2 input gain. Parameter scaling selects the authored
positive or negative extent from the parameter's value immediately before the
additive update; all five default mappings use weight 1.0. Tracking follows
pointer movement continuously and keeps the Viewer's TargetPoint response
curve without requiring a mouse button. Primary and secondary button state
remains independent for model-authored hand parameters. Viewer-default
automatic Idle playback is enabled and avoids immediately repeating the same
Idle motion when alternatives exist.
Desktop tracking is model-centered by default, so look direction changes only
when the pointer crosses the pet's visible center. Users can disable the
setting to retain the legacy screen/work-area-centered coordinate mapping.
In model-centered mode, the authored hand and device range maps absolutely to
the full display containing the pet; legacy mode retains Mver relative input.
The display is selected from the pet's visible center. Negative-coordinate,
stacked, and mixed-DPI layouts retain desktop coordinates, and display mode,
topology, or scale changes invalidate and immediately resample the mapping.
`ParamAngleZ` and model-specific `ParamMouse*` channels are not synthesized by
the Viewer-equivalent path.

Mver 0.1.6 remains the compatibility baseline for package discovery, imported
files, configuration, shortcuts, window composition, and projection only. A
Tauri or standalone Live2D model is imported through the same adapter and then
runs through the Viewer-equivalent Cubism animation path.

Mver imports retain the source package's `l2d_correct`, `window_size`,
`l2d_offset`, and horizontal-follow values in `.bongo-cat-mver.json`. The native
renderer uses the Mver 0.1.6 projection order for those models. Expressions keep
the authored canvas projection so the Live2D model remains aligned with the
background and input layers. The compatibility matrix also converts the
modern Cubism Core canvas units to Mver 0.1.6 units while retaining authored
Layout translation and `l2d_offset`. Its horizontal option controls following,
not model mirroring. Window resizing follows the imported reference aspect
ratio. Pointer-driven head, body, eye, and physics parameters use Mver's
configured `workarea`, or its DPI-virtualized primary-screen coordinate domain
when no custom work area is enabled.

The Mver adapter translates the horizontal target sign and work-area mapping
before the values reach the shared Viewer-equivalent look stage. It does not
alter Cubism motion curves, expression blending, physics, or look timing.

## Cubism Viewer blind test

The `viewer-sequence` CI scenario captures model-only Native frames and key
parameters at track and release samples `1, 2, 4, 8, 15, 30`. It does not use
desktop screenshots, so window decoration and desktop overlays cannot affect
the Native side. Reference Viewer screenshots are compared with:

```powershell
cmake\RunCubismViewerBlindTest.ps1 `
  -ViewerDirectory build-delivery-final\viewer-studio-audit\viewer `
  -NativeDirectory build-delivery-final\viewer-studio-audit\native-run\cubism-viewer-audit\native `
  -OutputDirectory build-delivery-final\viewer-studio-audit\blind-test-final
```

The tool removes transparent, white, and black backgrounds, rejects detached
desktop overlays, and normalizes each source with its baseline model bounds.
It writes anonymous randomized A/B ballots, a separate answer key, normalized
frames, and metrics for the full model, face, hair, and hands. Dynamic metrics
also report each side's change from `track-000`, preventing a high static image
score from hiding a different interaction response curve.

The blind-test entry point is a PowerShell wrapper around the native C target
`cubism_viewer_blind_test`; build it with:

```powershell
cmake --build build-delivery-final --config Release --target bongo_cat_validation_tools
```

The standalone C metric tools are `mver_blind_metrics` and
`mver_phase_metrics`. The former accepts two image paths and optional mask and
threshold arguments; the latter accepts two frame directories and searches
their phase lag. The Viewer capture helpers (`cubism_viewer_desktop_capture`
and `cubism_viewer_drag_capture`) are also C targets and keep their existing
command-line arguments and trace formats.

## Live2D texture quality

Live2D atlases retain their authored pixel dimensions for both bundled and
imported models. The loader only downsamples an atlas when it exceeds the
active GPU's `GL_MAX_TEXTURE_SIZE`, or retries at 2048 pixels after an explicit
`GL_OUT_OF_MEMORY` upload failure. Model textures use the same linear,
non-mipmapped sampling as Mver so changing the pet window size does not switch
filtering behavior or silently select a lower-resolution source.
