#include "preferences_state.h"
#include "ui_catime.h"
#include "ui_font.h"
#include "ui_font_atlas.h"
#include "preferences_model_glyphs.h"
#include "ui_paint.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/memory_policy.h"
#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <string.h>

#define PREF_WIDTH 900.0f
#define PREF_HEIGHT 680.0f
#define PREF_MIN_WIDTH 720.0f
#define PREF_MIN_HEIGHT 560.0f
#define PREF_SCREEN_MARGIN 48.0f

typedef struct PreferenceFonts {
    char body_path[BONGO_CAT_PATH_CAP];
    char body_fallback_path[BONGO_CAT_PATH_CAP];
    char body_korean_fallback_path[BONGO_CAT_PATH_CAP];
    char heading_path[BONGO_CAT_PATH_CAP];
    char heading_fallback_path[BONGO_CAT_PATH_CAP];
    char heading_korean_fallback_path[BONGO_CAT_PATH_CAP];
    const char *body;
    const char *body_fallback;
    const char *body_korean_fallback;
    const char *heading;
    const char *heading_fallback;
    const char *heading_korean_fallback;
    const nk_rune *ranges;
} PreferenceFonts;

static SDL_DisplayID window_display(SDL_Window *window) {
    SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : 0;
    return display ? display : SDL_GetPrimaryDisplay();
}

static int scaled(float value, float scale) {
    return SDL_max(1, (int)lroundf(value * scale));
}

static void fit_size(SDL_DisplayID display, float layout_scale,
    float logical_width, float logical_height, int *width, int *height,
    int *minimum_width, int *minimum_height) {
    *width = scaled(logical_width, layout_scale);
    *height = scaled(logical_height, layout_scale);
    *minimum_width = scaled(PREF_MIN_WIDTH, layout_scale);
    *minimum_height = scaled(PREF_MIN_HEIGHT, layout_scale);
    SDL_Rect usable;
    if (!display || !SDL_GetDisplayUsableBounds(display, &usable)) return;
    int margin = scaled(PREF_SCREEN_MARGIN, layout_scale);
    int maximum_width = SDL_max(1, usable.w - margin);
    int maximum_height = SDL_max(1, usable.h - margin);
    *minimum_width = SDL_min(*minimum_width, maximum_width);
    *minimum_height = SDL_min(*minimum_height, maximum_height);
    *width = SDL_clamp(*width, *minimum_width, maximum_width);
    *height = SDL_clamp(*height, *minimum_height, maximum_height);
}

static void fit_position(SDL_Window *window, SDL_DisplayID display,
    int width, int height) {
    SDL_Rect usable;
    if (!display || !SDL_GetDisplayUsableBounds(display, &usable)) return;
    int x = 0, y = 0;
    SDL_GetWindowPosition(window, &x, &y);
    int maximum_x = usable.x + SDL_max(0, usable.w - width);
    int maximum_y = usable.y + SDL_max(0, usable.h - height);
    int next_x = SDL_clamp(x, usable.x, maximum_x);
    int next_y = SDL_clamp(y, usable.y, maximum_y);
    if (next_x != x || next_y != y)
        SDL_SetWindowPosition(window, next_x, next_y);
}

static void font_paths(BongoCatPreferences *value, PreferenceFonts *fonts) {
    memset(fonts, 0, sizeof(*fonts));
    fonts->body = bongo_cat_ui_system_font(fonts->body_path,
        sizeof(fonts->body_path), false);
    fonts->body_fallback = bongo_cat_ui_system_font(fonts->body_fallback_path,
        sizeof(fonts->body_fallback_path), true);
    fonts->body_korean_fallback = bongo_cat_ui_system_korean_font(
        fonts->body_korean_fallback_path,
        sizeof(fonts->body_korean_fallback_path));
    fonts->heading = bongo_cat_ui_system_heading_font(fonts->heading_path,
        sizeof(fonts->heading_path), false);
    fonts->heading_fallback = bongo_cat_ui_system_heading_font(
        fonts->heading_fallback_path,
        sizeof(fonts->heading_fallback_path), true);
    fonts->heading_korean_fallback = bongo_cat_ui_system_korean_heading_font(
        fonts->heading_korean_fallback_path,
        sizeof(fonts->heading_korean_fallback_path));
    if (!fonts->heading) fonts->heading = fonts->body;
    if (!fonts->heading_fallback) fonts->heading_fallback = fonts->body_fallback;
    if (!value->app->i18n) return;
    bongo_cat_i18n_glyph_ranges(value->app->i18n, value->glyph_ranges,
        sizeof(value->glyph_ranges) / sizeof(value->glyph_ranges[0]));
    bongo_cat_preferences_model_glyphs(value->app, value->glyph_ranges,
        sizeof(value->glyph_ranges) / sizeof(value->glyph_ranges[0]));
    fonts->ranges = value->glyph_ranges;
}

static SDL_HitTestResult SDLCALL preference_hit_test(SDL_Window *window,
    const SDL_Point *point, void *data) {
    BongoCatPreferences *value = data;
    if (!value || !point || bongo_cat_preferences_remove_dialog_active(value->app))
        return SDL_HITTEST_NORMAL;
    float scale = value->ui.layout_scale > 0.0f ? value->ui.layout_scale : 1.0f;
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    int edge = scaled(7.0f, scale);
    bool left = point->x < edge, right = point->x >= width - edge;
    bool top = point->y < edge, bottom = point->y >= height - edge;
    if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (top) return SDL_HITTEST_RESIZE_TOP;
    if (right) return SDL_HITTEST_RESIZE_RIGHT;
    if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
    if (left) return SDL_HITTEST_RESIZE_LEFT;
    // Title dragging is handled from SDL events so the application loop keeps
    // advancing the desktop pet instead of entering a native modal move loop.
    return SDL_HITTEST_NORMAL;
}

static bool attach_gl_context(BongoCatPreferences *value) {
    value->gl_context = value->app->gl_context;
    if (SDL_GL_MakeCurrent(value->window, value->gl_context)) return true;
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    value->gl_context = SDL_GL_CreateContext(value->window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    value->owns_gl_context = value->gl_context != NULL;
    return value->gl_context &&
        SDL_GL_MakeCurrent(value->window, value->gl_context);
}

static void discard_window(BongoCatPreferences *value) {
    if (value->owns_gl_context && value->gl_context)
        SDL_GL_DestroyContext(value->gl_context);
    if (value->window) SDL_DestroyWindow(value->window);
    value->window = NULL;
    value->gl_context = NULL;
    value->owns_gl_context = false;
    value->transparent_window = false;
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
}

static bool create_window(BongoCatPreferences *value, int width, int height,
    SDL_WindowFlags flags, bool transparent) {
    value->window = SDL_CreateWindow(BONGO_CAT_SETTINGS_WINDOW_TITLE,
        width, height,
        flags | (transparent ? SDL_WINDOW_TRANSPARENT : 0));
    value->transparent_window = value->window && transparent &&
        (SDL_GetWindowFlags(value->window) & SDL_WINDOW_TRANSPARENT) != 0;
    return value->window != NULL;
}

bool bongo_cat_preferences_open_window(BongoCatPreferences *value) {
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float layout_scale = bongo_cat_ui_display_layout_scale(display);
    int width, height, minimum_width, minimum_height;
    fit_size(display, layout_scale, PREF_WIDTH, PREF_HEIGHT, &width, &height,
        &minimum_width, &minimum_height);
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE, "0");
#endif
    if (!create_window(value, width, height, flags, true)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Transparent preferences window unavailable: %s", SDL_GetError());
        if (!create_window(value, width, height, flags, false)) return false;
    }
    SDL_SetWindowPosition(value->window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    SDL_SyncWindow(value->window);
    display = window_display(value->window);
    float raster_scale = 1.0f;
    bongo_cat_ui_query_window_scale(value->window, &layout_scale, &raster_scale);
    fit_size(display, layout_scale, PREF_WIDTH, PREF_HEIGHT, &width, &height,
        &minimum_width, &minimum_height);
    SDL_SetWindowMinimumSize(value->window, minimum_width, minimum_height);
    SDL_SetWindowSize(value->window, width, height);
    SDL_SetWindowPosition(value->window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    SDL_SyncWindow(value->window);
    bool context_ready = attach_gl_context(value);
    if (!context_ready && value->transparent_window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Transparent preferences OpenGL context unavailable: %s",
            SDL_GetError());
        discard_window(value);
        if (!create_window(value, width, height, flags, false)) return false;
        SDL_SetWindowMinimumSize(value->window, minimum_width, minimum_height);
        SDL_SetWindowPosition(value->window,
            SDL_WINDOWPOS_CENTERED_DISPLAY(display),
            SDL_WINDOWPOS_CENTERED_DISPLAY(display));
        SDL_SyncWindow(value->window);
        context_ready = attach_gl_context(value);
    }
    if (!context_ready) return false;
    PreferenceFonts fonts;
    font_paths(value, &fonts);
    BongoCatError error = {0};
    if (!bongo_cat_ui_init(&value->ui, value->window, fonts.body,
        fonts.body_fallback, fonts.body_korean_fallback, fonts.heading,
        fonts.heading_fallback, fonts.heading_korean_fallback, fonts.ranges,
        layout_scale, raster_scale, &error)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    value->ui_initialized = true;
    SDL_SetWindowHitTest(value->window, preference_hit_test, value);
    value->native_drag = false;
    bongo_cat_preferences_assets_load(value);
    bongo_cat_memory_policy_ui_loaded();
    value->style_theme = -1;
    value->font_language = value->app->config.app.language;
    int pixel_width = 0, pixel_height = 0;
    SDL_GetWindowSizeInPixels(value->window, &pixel_width, &pixel_height);
    SDL_Log("Preferences GL ready: dedicated=%d transparent=%d pixels=%dx%d layout=%.2f raster=%.2f",
        value->owns_gl_context, value->transparent_window,
        pixel_width, pixel_height,
        layout_scale, raster_scale);
    if (!SDL_GL_SetSwapInterval(0)) SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
        "Preferences VSync disable unavailable: %s", SDL_GetError());
    SDL_StartTextInput(value->window);
    value->render_dirty = true;
    bongo_cat_preferences_live_resize_install(value);
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    return true;
}

bool bongo_cat_preferences_reload_fonts(BongoCatPreferences *value) {
    if (!value || !value->ui_initialized) return false;
    if (value->ui.frame_building) {
        value->font_reload_pending = true;
        value->render_dirty = true;
        return true;
    }
    PreferenceFonts fonts;
    font_paths(value, &fonts);
    bool reloaded = bongo_cat_ui_font_atlas_reload(&value->ui, fonts.body,
        fonts.body_fallback, fonts.body_korean_fallback, fonts.heading,
        fonts.heading_fallback, fonts.heading_korean_fallback, fonts.ranges,
        value->ui.raster_scale);
    if (reloaded) value->font_reload_pending = false;
    return reloaded;
}

bool bongo_cat_preferences_refresh_raster(BongoCatPreferences *value) {
    if (!value || value->pending_raster_scale <= 0.0f) return true;
    uint64_t now = SDL_GetTicksNS();
    if (value->raster_retry_ns > now) return false;
    float raster_scale = value->pending_raster_scale;
    PreferenceFonts fonts;
    font_paths(value, &fonts);
    if (!bongo_cat_ui_font_atlas_reload(&value->ui, fonts.body,
        fonts.body_fallback, fonts.body_korean_fallback, fonts.heading,
        fonts.heading_fallback, fonts.heading_korean_fallback, fonts.ranges,
        raster_scale)) {
        value->raster_retry_ns = now + 1000000000ull;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Preferences font atlas could not be rebuilt for %.2fx", raster_scale);
        return false;
    }
    value->pending_raster_scale = 0.0f;
    value->raster_retry_ns = 0;
    bongo_cat_ui_paint_destroy(&value->ui);
    bongo_cat_preferences_model_cover_cache_clear(value->app);
    bongo_cat_preferences_assets_clear(value);
    bongo_cat_preferences_assets_load(value);
    return true;
}

bool bongo_cat_preferences_scale_event(BongoCatPreferences *value,
    const SDL_Event *event) {
    if (!value || !value->window || !event ||
        (event->type != SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED &&
        event->type != SDL_EVENT_WINDOW_DISPLAY_CHANGED)) return false;
    float old_layout = value->ui.layout_scale > 0.0f ? value->ui.layout_scale : 1.0f;
    float old_raster = value->ui.raster_scale > 0.0f ? value->ui.raster_scale : 1.0f;
    int current_width = 1, current_height = 1;
    SDL_GetWindowSize(value->window, &current_width, &current_height);
    float logical_width = current_width / old_layout;
    float logical_height = current_height / old_layout;
    float layout_scale = 1.0f, raster_scale = 1.0f;
    bongo_cat_ui_query_window_scale(value->window, &layout_scale, &raster_scale);
    SDL_DisplayID display = window_display(value->window);
    int width, height, minimum_width, minimum_height;
    fit_size(display, layout_scale, logical_width, logical_height,
        &width, &height, &minimum_width, &minimum_height);
    bool was_rendering = value->live_resize_rendering;
    value->live_resize_rendering = true;
    bool context_ready = SDL_GL_MakeCurrent(value->window, value->gl_context);
    value->ui.layout_scale = layout_scale;
    if (fabsf(raster_scale - old_raster) > 0.01f) {
        value->pending_raster_scale = raster_scale;
        value->raster_retry_ns = 0;
        if (context_ready) bongo_cat_preferences_refresh_raster(value);
        else SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Preferences GL context unavailable during scale change: %s",
            SDL_GetError());
    } else {
        value->pending_raster_scale = 0.0f;
        value->raster_retry_ns = 0;
        value->ui.raster_scale = raster_scale;
    }
    SDL_SetWindowMinimumSize(value->window, minimum_width, minimum_height);
    SDL_SetWindowSize(value->window, width, height);
    fit_position(value->window, display, width, height);
    SDL_SyncWindow(value->window);
    value->live_resize_rendering = was_rendering;
    SDL_GL_MakeCurrent(value->app->window, value->app->gl_context);
    value->render_dirty = true;
    SDL_Log("Preferences scale changed: layout %.2f->%.2f raster %.2f->%.2f logical=%.0fx%.0f",
        old_layout, layout_scale, old_raster, raster_scale,
        logical_width, logical_height);
    return true;
}
