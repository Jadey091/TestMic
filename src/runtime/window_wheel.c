#include "runtime.h"
#include "bongo_cat/preferences.h"
#include <stdio.h>

#define WHEEL_OPACITY_STEP 5.0f
#define WHEEL_SCALE_STEP 5.0f
#define WHEEL_SCALE_TARGET_LEAD 10.0f
#define WHEEL_OPACITY_RESPONSE_SECONDS 0.05f
#define WHEEL_SCALE_RESPONSE_SECONDS 0.01f
#define WHEEL_OPACITY_SPEED_PER_SECOND 100.0f
#define WHEEL_SCALE_SPEED_PER_SECOND 150.0f
#define WHEEL_MAX_FRAME_SECONDS (1.0f / 60.0f)
#define WHEEL_GESTURE_IDLE_NS 120000000ull

static float wheel_delta(const SDL_MouseWheelEvent *event) {
    float value = event ? event->y : 0.0f;
    return event && event->direction == SDL_MOUSEWHEEL_FLIPPED ? -value : value;
}

static bool initialize_targets(BongoCatApp *app, bool reset) {
    if (!reset) return true;
    int x = 0, y = 0, width = 0, height = 0;
    if (!SDL_GetWindowPosition(app->window, &x, &y) ||
        !SDL_GetWindowSize(app->window, &width, &height) ||
        width <= 0 || height <= 0 || app->config.window.scale_percent <= 0.0f)
        return false;
    app->wheel_opacity_target = app->config.window.opacity_percent;
    app->wheel_scale_target = app->config.window.scale_percent;
    app->wheel_center_x = x + width * 0.5f;
    app->wheel_center_y = y + height * 0.5f;
    app->wheel_geometry_scale = app->config.window.scale_percent;
    app->wheel_base_width = width;
    app->wheel_base_height = height;
    app->wheel_animation_ns = SDL_GetTicksNS();
    app->wheel_input_ns = app->wheel_animation_ns;
    return true;
}

static int round_position(float value) {
    return (int)(value + (value < 0.0f ? -0.5f : 0.5f));
}

static void clamp_position(BongoCatApp *app, int width, int height, int *x, int *y) {
    if (!app->config.window.keep_in_screen) return;
    SDL_DisplayID display = SDL_GetDisplayForWindow(app->window);
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds)) return;
    int max_x = SDL_max(bounds.x, bounds.x + bounds.w - width);
    int max_y = SDL_max(bounds.y, bounds.y + bounds.h - height);
    *x = SDL_clamp(*x, bounds.x, max_x);
    *y = SDL_clamp(*y, bounds.y, max_y);
}

void bongo_cat_window_wheel(BongoCatApp *app, const SDL_MouseWheelEvent *event) {
    if (!app || !app->window || !event) return;
    float delta = wheel_delta(event);
    if (SDL_fabsf(delta) < 0.001f) return;
    delta = SDL_clamp(delta, -1.0f, 1.0f);
    uint64_t event_ns = event->timestamp ? event->timestamp : SDL_GetTicksNS();
    bool continuing = app->wheel_gesture_active && event_ns >= app->wheel_event_ns &&
        event_ns - app->wheel_event_ns < WHEEL_GESTURE_IDLE_NS;
    if (!initialize_targets(app, !continuing)) return;
    app->wheel_event_ns = event_ns;
    app->wheel_gesture_active = true;
    float old_opacity_target = app->wheel_opacity_target;
    float old_scale_target = app->wheel_scale_target;
    bool control = (SDL_GetModState() & SDL_KMOD_CTRL) != 0 ||
        bongo_cat_input_control_down(&app->input);
    if (!control) {
        float minimum = SDL_max(10.0f,
            app->config.window.scale_percent - WHEEL_SCALE_TARGET_LEAD);
        float maximum = SDL_min(500.0f,
            app->config.window.scale_percent + WHEEL_SCALE_TARGET_LEAD);
        app->wheel_scale_target = SDL_clamp(app->wheel_scale_target +
            delta * WHEEL_SCALE_STEP, minimum, maximum);
    } else {
        app->wheel_opacity_target = SDL_clamp(app->wheel_opacity_target +
            delta * WHEEL_OPACITY_STEP, 10.0f, 100.0f);
    }
    uint64_t input_ns = SDL_GetTicksNS();
    if (old_opacity_target == app->wheel_opacity_target &&
        old_scale_target == app->wheel_scale_target) {
        if (app->wheel_animation_active) app->wheel_input_ns = input_ns;
        return;
    }
    if (!app->wheel_animation_active) app->wheel_animation_ns = input_ns;
    app->wheel_input_ns = input_ns;
    app->wheel_animation_active = true;
    app->dirty = true;
}

static float approach(float current, float target, float elapsed_seconds,
    float response_seconds, float speed_per_second, float snap_distance) {
    float difference = target - current;
    if (SDL_fabsf(difference) < snap_distance) return target;
    float alpha = elapsed_seconds / (response_seconds + elapsed_seconds);
    float step = difference * alpha;
    float maximum = speed_per_second * elapsed_seconds;
    step = SDL_clamp(step, -maximum, maximum);
    return SDL_fabsf(step) >= SDL_fabsf(difference) ? target : current + step;
}

static void apply_scale(BongoCatApp *app, float scale) {
    float old = app->config.window.scale_percent;
    if (old <= 0.0f || SDL_fabsf(scale - old) < 0.001f) return;
    float actual;
    int next_width, next_height;
    if (!bongo_cat_window_scaled_size(app->wheel_base_width,
        app->wheel_base_height, app->wheel_geometry_scale,
        scale, &actual, &next_width, &next_height)) return;
    if (actual != scale) app->wheel_scale_target = actual;
    if (next_width == app->config.window.width &&
        next_height == app->config.window.height) {
        app->config.window.scale_percent = actual;
        return;
    }
    int next_x = round_position(app->wheel_center_x - next_width * 0.5f);
    int next_y = round_position(app->wheel_center_y - next_height * 0.5f);
    clamp_position(app, next_width, next_height, &next_x, &next_y);
    if (!bongo_cat_window_apply_geometry(app, next_x, next_y,
        actual, next_width, next_height)) {
        app->wheel_scale_target = old;
        app->wheel_animation_active = false;
        app->wheel_gesture_active = false;
    }
}

void bongo_cat_window_update_wheel_animation(BongoCatApp *app, uint64_t now) {
    if (!app || !app->wheel_animation_active) return;
    uint64_t elapsed_ns = now >= app->wheel_animation_ns
        ? now - app->wheel_animation_ns : 0;
    app->wheel_animation_ns = now;
    float elapsed_seconds = SDL_min((float)elapsed_ns / 1000000000.0f,
        WHEEL_MAX_FRAME_SECONDS);
    float opacity = approach(app->config.window.opacity_percent,
        app->wheel_opacity_target, elapsed_seconds,
        WHEEL_OPACITY_RESPONSE_SECONDS, WHEEL_OPACITY_SPEED_PER_SECOND, 0.01f);
    float scale = approach(app->config.window.scale_percent,
        app->wheel_scale_target, elapsed_seconds,
        WHEEL_SCALE_RESPONSE_SECONDS, WHEEL_SCALE_SPEED_PER_SECOND, 0.5f);
    bool changed = SDL_fabsf(opacity - app->config.window.opacity_percent) > 0.001f ||
        SDL_fabsf(scale - app->config.window.scale_percent) > 0.001f;
    if (SDL_fabsf(opacity - app->config.window.opacity_percent) > 0.001f) {
        app->config.window.opacity_percent = opacity;
        if (!app->hover_hidden) bongo_cat_platform_set_opacity(
            &app->platform, opacity / 100.0f);
    }
    apply_scale(app, scale);
    bool reached =
        SDL_fabsf(app->config.window.opacity_percent - app->wheel_opacity_target) < 0.01f &&
        SDL_fabsf(app->config.window.scale_percent - app->wheel_scale_target) < 0.01f;
    bool recent_input = now >= app->wheel_input_ns &&
        now - app->wheel_input_ns < WHEEL_GESTURE_IDLE_NS;
    app->wheel_animation_active = !reached || recent_input;
    if (changed || !app->wheel_animation_active) app->dirty = true;
    if (changed) bongo_cat_preferences_invalidate(app->preferences);
}

void bongo_cat_window_cancel_wheel_animation(BongoCatApp *app) {
    if (!app) return;
    app->wheel_animation_active = false;
    app->wheel_gesture_active = false;
}

bool bongo_cat_window_wheel_self_test(BongoCatApp *app) {
    if (!app || !app->window) return false;
    BongoCatWindowOptions backup = app->config.window;
    int original_x, original_y, original_width, original_height;
    SDL_GetWindowPosition(app->window, &original_x, &original_y);
    SDL_GetWindowSize(app->window, &original_width, &original_height);
    SDL_Keymod modifiers = SDL_GetModState();
    SDL_SetModState(modifiers | SDL_KMOD_CTRL);
    app->config.window.opacity_percent = 80.0f;
    app->config.window.scale_percent = 100.0f;
    bongo_cat_window_cancel_wheel_animation(app);
    SDL_MouseWheelEvent wheel = {.y = -1.0f};
    bongo_cat_window_wheel(app, &wheel);
    uint64_t started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        bongo_cat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool opacity = SDL_fabsf(app->config.window.opacity_percent - 75.0f) < 0.1f;
    SDL_SetModState(modifiers & ~SDL_KMOD_CTRL);
    wheel.y = 1.0f; bongo_cat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 8; ++i)
        bongo_cat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool responsive = SDL_fabsf(app->config.window.scale_percent - 105.0f) < 0.1f;
    bongo_cat_window_update_wheel_animation(app,
        app->wheel_input_ns + WHEEL_GESTURE_IDLE_NS + 1);
    bool scale = responsive && !app->wheel_animation_active;
    bongo_cat_window_cancel_wheel_animation(app);
    bongo_cat_window_apply_geometry(app, original_x, original_y, 100.0f,
        original_width, original_height);
    wheel.y = 1000.0f;
    for (int i = 0; i < 2; ++i) bongo_cat_window_wheel(app, &wheel);
    bool burst = app->wheel_scale_target >= 109.5f &&
        app->wheel_scale_target <= 110.1f;
    bongo_cat_window_cancel_wheel_animation(app);
    bongo_cat_window_apply_geometry(app, original_x, original_y, 100.0f,
        original_width, original_height);
    wheel.y = 3.0f;
    bongo_cat_window_wheel(app, &wheel);
    bool aggregated = app->wheel_scale_target >= 104.5f &&
        app->wheel_scale_target <= 105.1f;
    bongo_cat_window_cancel_wheel_animation(app);
    bongo_cat_window_apply_geometry(app, original_x, original_y, 100.0f,
        original_width, original_height);
    wheel.y = 1.0f;
    bongo_cat_window_wheel(app, &wheel);
    wheel.y = -1.0f;
    bongo_cat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        bongo_cat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool reversal = !app->wheel_animation_active &&
        SDL_fabsf(app->config.window.scale_percent - 100.0f) < 0.1f;
    wheel.y = 1.0f;
    wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    bongo_cat_window_wheel(app, &wheel);
    started = app->wheel_animation_ns;
    for (int i = 1; i <= 30; ++i)
        bongo_cat_window_update_wheel_animation(app, started + i * 16666667ull);
    bool flipped = SDL_fabsf(app->config.window.scale_percent - 95.0f) < 0.1f;
    wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    bongo_cat_window_cancel_wheel_animation(app);
    app->config.window.scale_percent = 500.0f;
    bongo_cat_window_wheel(app, &wheel);
    bool maximum = !app->wheel_animation_active;
    bongo_cat_window_cancel_wheel_animation(app);
    app->config.window.scale_percent = 10.0f;
    wheel.y = -1.0f;
    bongo_cat_window_wheel(app, &wheel);
    bool minimum = !app->wheel_animation_active;
    bongo_cat_window_cancel_wheel_animation(app);
    SDL_SetModState(modifiers | SDL_KMOD_CTRL);
    app->config.window.opacity_percent = 100.0f;
    wheel.y = 1.0f;
    bongo_cat_window_wheel(app, &wheel);
    bool opacity_maximum = !app->wheel_animation_active;
    bongo_cat_window_cancel_wheel_animation(app);
    app->config.window.opacity_percent = 10.0f;
    wheel.y = -1.0f;
    bongo_cat_window_wheel(app, &wheel);
    bool opacity_minimum = !app->wheel_animation_active;
    bool rounding = round_position(-10.6f) == -11 && round_position(10.6f) == 11;
    SDL_SetModState(modifiers);
    app->config.window = backup;
    bongo_cat_window_cancel_wheel_animation(app);
    bongo_cat_platform_set_opacity(&app->platform,
        backup.opacity_percent / 100.0f);
    bongo_cat_platform_set_geometry(&app->platform, original_x, original_y,
        original_width, original_height);
    bool passed = opacity && scale && burst && aggregated && reversal && flipped &&
        maximum && minimum && opacity_maximum && opacity_minimum && rounding;
    if (!passed) fprintf(stderr, "wheel self-test: opacity=%d scale=%d burst=%d "
        "aggregated=%d reversal=%d flipped=%d maximum=%d minimum=%d opacity_max=%d "
        "opacity_min=%d rounding=%d\n", opacity, scale, burst, aggregated, reversal,
        flipped, maximum, minimum, opacity_maximum, opacity_minimum, rounding);
    return passed;
}
