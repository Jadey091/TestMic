#include "runtime.h"

#include <stdio.h>

#define WINDOW_MIN_DIMENSION 64
#define WINDOW_MAX_DIMENSION 8192
#define WINDOW_MIN_SCALE 10.0f
#define WINDOW_MAX_SCALE 500.0f

static int round_dimension(double value) {
    return (int)(value + 0.5);
}

bool bongo_cat_window_scaled_size(int base_width, int base_height, float base_scale,
    float requested_scale, float *actual_scale, int *width, int *height) {
    if (base_width <= 0 || base_height <= 0 || base_scale <= 0.0f ||
        !actual_scale || !width || !height) return false;
    float minimum = SDL_max(WINDOW_MIN_SCALE, SDL_max(
        base_scale * WINDOW_MIN_DIMENSION / base_width,
        base_scale * WINDOW_MIN_DIMENSION / base_height));
    float maximum = SDL_min(WINDOW_MAX_SCALE, SDL_min(
        base_scale * WINDOW_MAX_DIMENSION / base_width,
        base_scale * WINDOW_MAX_DIMENSION / base_height));
    if (minimum > maximum) return false;
    float scale = SDL_clamp(requested_scale, minimum, maximum);
    int next_width = round_dimension((double)base_width * scale / base_scale);
    int next_height = round_dimension((double)base_height * scale / base_scale);
    *actual_scale = scale;
    *width = SDL_clamp(next_width, WINDOW_MIN_DIMENSION, WINDOW_MAX_DIMENSION);
    *height = SDL_clamp(next_height, WINDOW_MIN_DIMENSION, WINDOW_MAX_DIMENSION);
    return true;
}

bool bongo_cat_window_apply_geometry(BongoCatApp *app, int x, int y,
    float scale, int width, int height) {
    if (!app || !app->window || width < WINDOW_MIN_DIMENSION ||
        height < WINDOW_MIN_DIMENSION || width > WINDOW_MAX_DIMENSION ||
        height > WINDOW_MAX_DIMENSION) return false;
    if (!bongo_cat_platform_set_geometry(&app->platform, x, y, width, height))
        return false;
    app->config.window.scale_percent = scale;
    app->config.window.x = x;
    app->config.window.y = y;
    app->config.window.width = width;
    app->config.window.height = height;
    if (SDL_GetWindowSizeInPixels(app->window,
        &app->resize_pixel_width, &app->resize_pixel_height))
        app->resize_pending = true;
    app->dirty = true;
    bongo_cat_window_mark_hit_dirty(app);
    return true;
}

bool bongo_cat_window_set_scale(BongoCatApp *app, float scale) {
    if (!app || !app->window) return false;
    int x, y, width, height;
    if (!SDL_GetWindowPosition(app->window, &x, &y) ||
        !SDL_GetWindowSize(app->window, &width, &height)) return false;
    float actual;
    int next_width, next_height;
    if (!bongo_cat_window_scaled_size(width, height,
        app->config.window.scale_percent, scale,
        &actual, &next_width, &next_height)) return false;
    if (actual == app->config.window.scale_percent &&
        next_width == width && next_height == height) return false;
    return bongo_cat_window_apply_geometry(app, x, y,
        actual, next_width, next_height);
}

void bongo_cat_window_resize_by_pointer(BongoCatApp *app, const SDL_Event *event) {
    bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0 ||
        bongo_cat_input_shift_down(&app->input);
    if (!(event->motion.state & SDL_BUTTON_RMASK) || !shift) return;
    bongo_cat_window_cancel_wheel_animation(app);
    if (!app->resize_gesture) {
        if (!SDL_GetWindowSize(app->window,
            &app->resize_base_width, &app->resize_base_height)) return;
        app->resize_scale_start = app->config.window.scale_percent;
        app->resize_scale_target = app->resize_scale_start;
        app->resize_gesture = true;
    }
    float delta = (event->motion.xrel + event->motion.yrel) * 0.5f;
    app->resize_scale_target = SDL_clamp(app->resize_scale_target + delta,
        WINDOW_MIN_SCALE, WINDOW_MAX_SCALE);
    float actual;
    int width, height, x, y;
    if (!bongo_cat_window_scaled_size(app->resize_base_width,
        app->resize_base_height, app->resize_scale_start,
        app->resize_scale_target, &actual, &width, &height) ||
        !SDL_GetWindowPosition(app->window, &x, &y)) return;
    app->resize_scale_target = actual;
    if (!bongo_cat_window_apply_geometry(app, x, y, actual, width, height))
        app->resize_scale_target = app->config.window.scale_percent;
}

bool bongo_cat_window_geometry_self_test(BongoCatApp *app) {
    if (!app || !app->window) return false;
    SDL_SyncWindow(app->window);
    BongoCatWindowOptions backup = app->config.window;
    int original_x, original_y, original_width, original_height;
    SDL_GetWindowPosition(app->window, &original_x, &original_y);
    SDL_GetWindowSize(app->window, &original_width, &original_height);
    SDL_DisplayID display = SDL_GetDisplayForWindow(app->window);
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds)) return false;
    app->config.window.keep_in_screen = true;
    app->model_pointer_anchor_ready = true;
    bongo_cat_window_apply_geometry(app, bounds.x - 2000, bounds.y - 2000,
        100.0f, 320, 240);
    SDL_SyncWindow(app->window);
    bongo_cat_window_apply_pending_resize(app);
    bongo_cat_window_clamp_to_display(app);
    SDL_SyncWindow(app->window);
    int x, y, width, height;
    SDL_GetWindowPosition(app->window, &x, &y);
    bool clamped = x >= bounds.x && y >= bounds.y;
    bool anchor_reset = !app->model_pointer_anchor_ready;
    bool scaled = bongo_cat_window_set_scale(app, 125.0f);
    SDL_SyncWindow(app->window);
    SDL_GetWindowSize(app->window, &width, &height);
    int scaled_width = width, scaled_height = height;
    scaled = scaled && width == 400 && height == 300;
    bongo_cat_window_menu_action(app, BONGO_CAT_MENU_OPACITY_50);
    bool opacity = SDL_fabsf(bongo_cat_platform_get_opacity(
        &app->platform) - 0.5f) < 0.02f;
    app->config.window.hide_on_hover = true;
    app->config.window.hide_delay_seconds = 0.0f;
    app->config.window.pass_through = false;
    app->config.window.opacity_percent = 100.0f;
    bongo_cat_app_track_hover(app, x + 10, y + 10);
    bongo_cat_app_update_hover(app, SDL_GetTicksNS() + 1);
    bool hidden = app->hover_hidden &&
        bongo_cat_platform_get_opacity(&app->platform) < 0.02f;
    bongo_cat_app_track_hover(app, bounds.x - 10, bounds.y - 10);
    bool restored = !app->hover_hidden &&
        SDL_fabsf(bongo_cat_platform_get_opacity(&app->platform) - 1.0f) < 0.02f;
    float safe_scale;
    int safe_width, safe_height;
    bool bounded = bongo_cat_window_scaled_size(8000, 4000, 100.0f, 500.0f,
        &safe_scale, &safe_width, &safe_height) && safe_width == 8192 &&
        safe_height == 4096 && safe_scale < 103.0f;
    BongoCatInputEvent shift = {.kind = BONGO_CAT_INPUT_KEY_DOWN};
    snprintf(shift.name, sizeof(shift.name), "ShiftLeft");
    bongo_cat_input_push(&app->input, &shift);
    BongoCatInputEvent discarded;
    bongo_cat_input_pop(&app->input, &discarded);
    SDL_Keymod old_modifiers = SDL_GetModState();
    SDL_SetModState(old_modifiers & ~SDL_KMOD_SHIFT);
    app->resize_gesture = false;
    bongo_cat_window_apply_geometry(app, x, y, 100.0f, 320, 240);
    SDL_Event motion = {.type = SDL_EVENT_MOUSE_MOTION};
    motion.motion.state = SDL_BUTTON_RMASK;
    motion.motion.xrel = 20.0f;
    motion.motion.yrel = 20.0f;
    bongo_cat_window_resize_by_pointer(app, &motion);
    SDL_SyncWindow(app->window);
    SDL_GetWindowSize(app->window, &width, &height);
    int gesture_width = width, gesture_height = height;
    bool gesture = app->resize_gesture &&
        app->config.window.scale_percent == 120.0f &&
        width == 384 && height == 288;
    SDL_Event released = {.type = SDL_EVENT_MOUSE_BUTTON_UP};
    released.button.button = SDL_BUTTON_RIGHT;
    bongo_cat_window_event(app, &released);
    gesture = gesture && !app->resize_gesture;
    app->pointer_known = true; app->model_pointer_anchor_ready = true;
    app->mver_pointer.initialized = true;
    SDL_Event display_scale = {.type = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED};
    display_scale.window.windowID = SDL_GetWindowID(app->window);
    bongo_cat_window_event(app, &display_scale);
    bool display_reset = !app->pointer_known &&
        !app->model_pointer_anchor_ready && !app->mver_pointer.initialized;
    shift.kind = BONGO_CAT_INPUT_KEY_UP;
    bongo_cat_input_push(&app->input, &shift);
    bongo_cat_input_pop(&app->input, &discarded);
    app->resize_gesture = false;
    SDL_SetModState(old_modifiers);
    bongo_cat_window_apply_geometry(app, original_x, original_y,
        backup.scale_percent, original_width, original_height);
    app->config.window = backup;
    bongo_cat_platform_set_opacity(&app->platform,
        backup.opacity_percent / 100.0f);
    bongo_cat_window_sync_click_through(app);
    SDL_SyncWindow(app->window);
    bool passed = clamped && anchor_reset && scaled && opacity && hidden && restored &&
        bounded && gesture && display_reset;
    if (!passed) fprintf(stderr, "geometry self-test: clamped=%d scaled=%d(%dx%d) "
        "anchor=%d opacity=%d hidden=%d restored=%d bounded=%d gesture=%d(%dx%d) display=%d\n",
        clamped, scaled, scaled_width, scaled_height, anchor_reset, opacity, hidden, restored,
        bounded, gesture, gesture_width, gesture_height, display_reset);
    return passed;
}
