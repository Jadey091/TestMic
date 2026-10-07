#include "preferences_state.h"
#include "ui_paint.h"
#include "bongo_cat/tray.h"

#include <SDL3/SDL.h>

bool bongo_cat_preferences_reload_language(BongoCatPreferences *value) {
    if (!value || value->font_language == value->app->config.app.language)
        return false;
    BongoCatLanguage previous = value->font_language;
    BongoCatLanguage requested = value->app->config.app.language;
    BongoCatError error = {0};
    if (!value->app->i18n || bongo_cat_i18n_reload(value->app->i18n,
        requested, &error) != BONGO_CAT_OK) {
        value->app->config.app.language = previous;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.message);
        return false;
    }
    value->font_language = requested;
    if (!bongo_cat_preferences_reload_fonts(value)) {
        BongoCatError restore = {0};
        value->app->config.app.language = previous;
        value->font_language = previous;
        bongo_cat_i18n_reload(value->app->i18n, previous, &restore);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Preferences font resources could not be rebuilt");
        return false;
    }
    bongo_cat_ui_paint_destroy(&value->ui);
    if (value->app->tray) bongo_cat_tray_sync(value->app->tray);
    value->render_dirty = true;
    return true;
}
