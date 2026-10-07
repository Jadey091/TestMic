# Modal menu frame ownership

Native context menus run a nested platform event loop. The SDL application loop
does not advance while one is open, so animation must be pumped explicitly.

The dependency direction is intentionally one way:

```text
native modal timer -> BongoCatModalTick -> runtime/modal_frame -> input + Live2D + render
```

Ownership boundaries:

- `runtime/modal_frame.*` owns elapsed-time continuity and the work performed by
  a modal frame. It is the only modal-related module allowed to coordinate input,
  Live2D, and rendering.
- `runtime/window_menu_preview.c` owns temporary context-menu previews. It
  delegates animation to the modal frame pump.
- `runtime/tray.c` owns tray state and supplies a modal frame callback. It does
  not know how a native menu loop is implemented.
- `platform/windows_borderless.c` owns the desktop menu timer and highlight
  messages. Its callback state is attached to the individual window.
- `platform/windows_tray.c` owns SDL tray-window subclassing and the tray menu
  timer. Its callback state is attached to the individual tray window.
- Native window dragging uses the same pump while the operating system owns the
  nested move loop.
- `platform/macos.m` schedules the same callback in the menu run-loop mode.
  SDL's Linux tray is asynchronous; the custom X11 desktop menu calls the
  callback from its own loop.

Invariants for future changes:

1. Platform menu modules must not include Live2D or application runtime state.
2. Modal ticks must update `app->last_frame_ns`, preventing a large catch-up
   step when the SDL loop resumes.
3. The pump must drain model input without dispatching shortcuts before advancing Live2D.
4. Every native timer and subclass must be removed before its callback userdata
   is destroyed.
5. Desktop and tray menus must both pass a held-open test with continuing frame
   timestamps and different image hashes after pointer movement.
