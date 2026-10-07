#ifndef BONGO_CAT_PREFERENCES_STATE_H
#define BONGO_CAT_PREFERENCES_STATE_H

#include "preferences_internal.h"
#include "ui_backend.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/preferences.h"
#include "../runtime/modal_frame.h"

typedef struct BongoCatPreferenceNotice {
    char message[384];
    uint64_t started_ns;
    uint64_t until_ns;
    bool error;
} BongoCatPreferenceNotice;

struct BongoCatPreferences {
    BongoCatApp *app;
    SDL_Window *window;
    SDL_GLContext gl_context;
    bool owns_gl_context;
    bool transparent_window;
    bool ui_initialized;
    BongoCatUIBackend ui;
    unsigned int logo_texture;
    unsigned int icon_texture;
    unsigned int icon_texture_hidpi;
    int logo_width;
    int logo_height;
    unsigned int catime_texture;
    unsigned int vlaina_texture;
    int catime_width, catime_height;
    int vlaina_width, vlaina_height;
    bool support_assets_loaded;
    int page;
    int style_theme;
    BongoCatLanguage font_language;
    nk_rune glyph_ranges[2048];
    bool input_active;
    bool import_requested;
    BongoCatImportDialog *import_dialog;
    bool frame_checked;
    bool render_dirty;
    bool font_reload_pending;
    bool smoke_behavior_open_pending;
    uint64_t last_render_ns;
    float pending_raster_scale;
    uint64_t raster_retry_ns;
    uint64_t render_retry_ns;
    float scroll_current[5];
    float scroll_target[5];
    bool scroll_ready[5];
    bool page_seen;
    int last_page;
    uint64_t page_transition_ns;
    BongoCatPreferenceNotice notices[4];
    bool behavior_dialog;
    bool behavior_dialog_input_armed;
    int behavior_tab;
    uint64_t behavior_dialog_opened_ns;
    uint64_t behavior_dialog_closing_ns;
    uint64_t behavior_tab_transition_ns;
    float behavior_scroll[2];
    char behavior_rename_id[BONGO_CAT_PATH_CAP];
    char behavior_rename_text[BONGO_CAT_ID_CAP];
    struct nk_rect behavior_rename_bounds;
    bool behavior_rename_select_all;
    bool native_drag;
    bool chrome_dragging;
    bool live_resize_active;
    bool live_resize_rendering;
    bool live_resize_pending;
    bool live_resize_timer;
    bool live_resize_modal_ready;
    BongoCatModalFrame live_resize_modal_frame;
    unsigned live_resize_layout_frames;
    int drag_window_x;
    int drag_window_y;
    float drag_pointer_x;
    float drag_pointer_y;
    char shortcut_id[BONGO_CAT_ID_CAP + 16];
    char *shortcut_target;
    int shortcut_capacity;
    char shortcut_original[BONGO_CAT_SHORTCUT_CAP];
    SDL_Keycode shortcut_key;
    bool shortcut_recording;
    uint64_t shortcut_suppress_until_ns;
};

int bongo_cat_preferences_resolved_theme(const BongoCatPreferences *value);
void bongo_cat_preferences_apply_theme(BongoCatPreferences *value);
bool bongo_cat_preferences_open_window(BongoCatPreferences *value);
bool bongo_cat_preferences_scale_event(BongoCatPreferences *value,
    const SDL_Event *event);
bool bongo_cat_preferences_refresh_raster(BongoCatPreferences *value);
bool bongo_cat_preferences_reload_fonts(BongoCatPreferences *value);
bool bongo_cat_preferences_reload_language(BongoCatPreferences *value);
void bongo_cat_preferences_drag_tick(BongoCatPreferences *value);
void bongo_cat_preferences_live_resize_install(BongoCatPreferences *value);
void bongo_cat_preferences_live_resize_uninstall(BongoCatPreferences *value);
void bongo_cat_preferences_record_frame(BongoCatPreferences *value);
void bongo_cat_preferences_assets_load(BongoCatPreferences *value);
void bongo_cat_preferences_support_assets_load(BongoCatPreferences *value);
void bongo_cat_preferences_assets_clear(BongoCatPreferences *value);
void bongo_cat_preferences_model_cover_cache_clear(BongoCatApp *app);
void bongo_cat_preferences_smoke_frame(BongoCatPreferences *value);
void bongo_cat_preferences_icon_draw(const BongoCatPreferences *value,
    struct nk_command_buffer *canvas, int icon, struct nk_rect bounds,
    struct nk_color color);

#endif
