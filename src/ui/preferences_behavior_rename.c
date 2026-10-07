#include "preferences_state.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static BongoCatBehaviorShortcut *binding_for(BongoCatConfig *config,
    const char *id) {
    for (size_t i = 0; i < config->behavior_shortcut_count; ++i)
        if (!strcmp(config->behavior_shortcuts[i].id, id))
            return &config->behavior_shortcuts[i];
    if (config->behavior_shortcut_count >= BONGO_CAT_BEHAVIOR_CAP) return NULL;
    BongoCatBehaviorShortcut *binding =
        &config->behavior_shortcuts[config->behavior_shortcut_count++];
    memset(binding, 0, sizeof(*binding));
    snprintf(binding->id, sizeof(binding->id), "%s", id);
    return binding;
}

static const char *display_label(const BongoCatBehaviorEntry *entry,
    const BongoCatBehaviorShortcut *binding) {
    return binding && binding->label[0] ? binding->label : entry->label;
}

void bongo_cat_preferences_behavior_rename_finish(
    BongoCatPreferences *value, bool save) {
    if (!value || !value->behavior_rename_id[0]) return;
    bool label_changed = false;
    if (save) {
        size_t start = 0, end = strlen(value->behavior_rename_text);
        while (start < end && (value->behavior_rename_text[start] == ' ' ||
            value->behavior_rename_text[start] == '\t')) start++;
        while (end > start && (value->behavior_rename_text[end - 1] == ' ' ||
            value->behavior_rename_text[end - 1] == '\t')) end--;
        if (start) memmove(value->behavior_rename_text,
            value->behavior_rename_text + start, end - start);
        value->behavior_rename_text[end - start] = '\0';
        BongoCatBehaviorEntry *entry = NULL;
        for (size_t i = 0; i < value->app->behaviors.count; ++i)
            if (!strcmp(value->app->behaviors.entries[i].id,
                value->behavior_rename_id)) {
                entry = &value->app->behaviors.entries[i]; break;
            }
        BongoCatBehaviorShortcut *binding = binding_for(&value->app->config,
            value->behavior_rename_id);
        if (binding) {
            char label[BONGO_CAT_ID_CAP];
            snprintf(label, sizeof(label), "%s",
                entry && !strcmp(value->behavior_rename_text, entry->label) ? "" :
                value->behavior_rename_text);
            label_changed = strcmp(binding->label, label) != 0;
            snprintf(binding->label, sizeof(binding->label), "%s", label);
        }
    }
    value->behavior_rename_id[0] = '\0';
    value->behavior_rename_text[0] = '\0';
    value->behavior_rename_select_all = false;
    SDL_StopTextInput(value->window);
    if (label_changed) bongo_cat_preferences_reload_fonts(value);
    value->render_dirty = true;
}

void bongo_cat_preferences_behavior_rename_begin(BongoCatPreferences *value,
    const BongoCatBehaviorEntry *entry, BongoCatBehaviorShortcut *binding,
    struct nk_rect bounds) {
    bongo_cat_preferences_shortcut_cancel(value);
    snprintf(value->behavior_rename_id, sizeof(value->behavior_rename_id),
        "%s", entry->id);
    snprintf(value->behavior_rename_text, sizeof(value->behavior_rename_text),
        "%s", display_label(entry, binding));
    value->behavior_rename_bounds = bounds;
    value->behavior_rename_select_all = true;
    SDL_StartTextInput(value->window);
    value->render_dirty = true;
}

static void append_text(BongoCatPreferences *value, const char *text) {
    if (!text) return;
    if (value->behavior_rename_select_all) {
        value->behavior_rename_text[0] = '\0';
        value->behavior_rename_select_all = false;
    }
    size_t used = strlen(value->behavior_rename_text);
    for (const char *source = text; *source;) {
        nk_rune rune = 0;
        int decoded = nk_utf_decode(source, &rune, (int)strlen(source));
        if (decoded <= 0) { source++; continue; }
        size_t bytes = (size_t)decoded;
        if (used + bytes >= sizeof(value->behavior_rename_text)) break;
        if (rune < 32) { source += bytes; continue; }
        memcpy(value->behavior_rename_text + used, source, bytes);
        used += bytes; source += bytes;
    }
    value->behavior_rename_text[used] = '\0';
}

static void erase_text(BongoCatPreferences *value, SDL_Keycode key) {
    size_t length = strlen(value->behavior_rename_text);
    if (value->behavior_rename_select_all) length = 0;
    else if (length && key == SDLK_BACKSPACE) {
        do length--; while (length &&
            ((unsigned char)value->behavior_rename_text[length] & 0xc0) == 0x80);
    }
    value->behavior_rename_text[length] = '\0';
    value->behavior_rename_select_all = false;
}

static void clipboard_event(BongoCatPreferences *value, SDL_Keycode key) {
    if (key == SDLK_V) {
        char *clipboard = SDL_GetClipboardText();
        append_text(value, clipboard);
        SDL_free(clipboard);
        return;
    }
    if (value->behavior_rename_select_all)
        SDL_SetClipboardText(value->behavior_rename_text);
    if (key == SDLK_X && value->behavior_rename_select_all)
        value->behavior_rename_text[0] = '\0';
}

bool bongo_cat_preferences_behavior_rename_event(
    BongoCatPreferences *value, const SDL_Event *event) {
    if (!value || !event || !value->behavior_rename_id[0]) return false;
    if (event->type == SDL_EVENT_TEXT_INPUT) {
        append_text(value, event->text.text);
        value->render_dirty = true; return true;
    }
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        bongo_cat_preferences_behavior_rename_finish(value, true); return false;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        float scale = value->ui.layout_scale > 0.0f ?
            value->ui.layout_scale : 1.0f;
        float x = event->button.x / scale, y = event->button.y / scale;
        if (!NK_INBOX(x, y, value->behavior_rename_bounds.x,
            value->behavior_rename_bounds.y, value->behavior_rename_bounds.w,
            value->behavior_rename_bounds.h))
            bongo_cat_preferences_behavior_rename_finish(value, true);
        return false;
    }
    if (event->type != SDL_EVENT_KEY_DOWN &&
        event->type != SDL_EVENT_KEY_UP) return false;
    if (event->type == SDL_EVENT_KEY_UP) return true;
    SDL_Keycode key = event->key.key;
    bool control = (event->key.mod & SDL_KMOD_CTRL) != 0;
    if (key == SDLK_ESCAPE)
        bongo_cat_preferences_behavior_rename_finish(value, false);
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
        bongo_cat_preferences_behavior_rename_finish(value, true);
    else if (control && key == SDLK_A) value->behavior_rename_select_all = true;
    else if (control && (key == SDLK_C || key == SDLK_X || key == SDLK_V))
        clipboard_event(value, key);
    else if (key == SDLK_BACKSPACE || key == SDLK_DELETE) erase_text(value, key);
    else if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_HOME ||
        key == SDLK_END) value->behavior_rename_select_all = false;
    value->render_dirty = true;
    return true;
}
