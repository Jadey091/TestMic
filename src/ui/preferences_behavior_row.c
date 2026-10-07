#include "preferences_state.h"
#include "preferences_notice.h"
#include "preferences_overlay.h"
#include "preferences_shortcut_clear.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_icons.h"
#include "ui_paint.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct nk_color alpha(struct nk_color color, float amount) {
    return bongo_cat_preferences_overlay_alpha(color, amount);
}

static void text(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *value, const struct nk_user_font *font, struct nk_color color) {
    nk_draw_text(canvas, bounds, value, nk_strlen(value), font,
        nk_rgba(0, 0, 0, 0), color);
}

static bool hit(struct nk_context *context, struct nk_rect bounds, bool enabled) {
    return enabled && nk_input_is_mouse_hovering_rect(&context->input, bounds) &&
        nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
}

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

static bool shortcut_editor(BongoCatPreferences *value,
    struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *id, BongoCatBehaviorShortcut *shortcut,
    BongoCatUIPalette p, float opacity, bool enabled) {
    bool active = bongo_cat_preferences_shortcut_active(value, id);
    bool hover = enabled && nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (active && p.effects) {
        float phase = (float)(SDL_GetTicksNS() % 1500000000ULL) / 1500000000.0f;
        float pulse = sinf(phase * 6.2831853f);
        bongo_cat_ui_paint_shadow(context, bounds, 10, 0, 0,
            5 + 3 * pulse, 0, alpha(nk_rgba(p.pink.r, p.pink.g,
            p.pink.b, 76), opacity));
        value->render_dirty = true;
    }
    nk_fill_rect(canvas, bounds, 10, alpha(active ? p.hover_pink :
        (hover ? p.hover : p.field), opacity));
    nk_stroke_rect(canvas, bounds, 10, 1, alpha(active ? p.pink :
        (hover ? p.accent : p.border_subtle), opacity));
    const char *shown = active ? bongo_cat_i18n_get(value->app->i18n,
        "components.shortcut.hints.pressRecordShortcut", "Press shortcut") :
        (shortcut->shortcut[0] ? shortcut->shortcut :
        bongo_cat_i18n_get(value->app->i18n,
        "components.shortcut.hints.clickRecordShortcut",
        "Click to record shortcut"));
    bool show_keyboard = active || !shortcut->shortcut[0];
    float text_x = bounds.x + (show_keyboard ? 39.0f : 13.0f);
    if (show_keyboard) bongo_cat_preferences_icon_draw(value, canvas,
        BONGO_CAT_UI_ICON_KEYBOARD, nk_rect(bounds.x + 13,
        bounds.y + 9, 18, 18), alpha(active ? p.pink : p.muted, opacity));
    text(canvas, nk_rect(text_x, bounds.y + 8,
        bounds.x + bounds.w - text_x -
        (shortcut->shortcut[0] && !active ? 31.0f : 9.0f), 21), shown,
        value->ui.caption_font,
        alpha(active ? p.pink : (hover ? p.accent : p.muted), opacity));
    struct nk_rect clear = nk_rect(bounds.x + bounds.w - 25,
        bounds.y + 8, 17, 20);
    if (bongo_cat_pref_shortcut_clear(context, canvas, id, clear, p,
        opacity, enabled && !active && shortcut->shortcut[0])) {
        shortcut->shortcut[0] = '\0'; value->render_dirty = true; return false;
    }
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds,
        BONGO_CAT_UI_CURSOR_POINTER);
    return hit(context, bounds, enabled);
}

static void draw_name(BongoCatPreferences *value, struct nk_context *context,
    struct nk_command_buffer *canvas, struct nk_rect bounds,
    BongoCatBehaviorEntry *entry, BongoCatBehaviorShortcut *binding,
    BongoCatUIPalette p, float opacity, bool enabled) {
    const char *label = display_label(entry, binding);
    bool renaming = !strcmp(value->behavior_rename_id, entry->id);
    bool hover = enabled && nk_input_is_mouse_hovering_rect(
        &context->input, bounds);
    if (renaming || hover) {
        nk_fill_rect(canvas, bounds, 8, alpha(renaming ? p.hover_pink : p.hover,
            opacity));
        nk_stroke_rect(canvas, bounds, 8, 1,
            alpha(renaming ? p.pink : p.accent, opacity));
    }
    const char *shown = renaming ? value->behavior_rename_text : label;
    text(canvas, nk_rect(bounds.x + 8, bounds.y + 9, bounds.w - 16, 21),
        shown, value->ui.caption_font, alpha(p.text, opacity));
    if (renaming) {
        value->behavior_rename_bounds = bounds;
        float caret = value->ui.caption_font->width(
            value->ui.caption_font->userdata, value->ui.caption_font->height,
            shown, nk_strlen(shown));
        caret = NK_MIN(caret, bounds.w - 18);
        nk_stroke_line(canvas, bounds.x + 8 + caret, bounds.y + 8,
            bounds.x + 8 + caret, bounds.y + bounds.h - 8, 1,
            alpha(p.pink, opacity));
    } else if (hover) {
        bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_TEXT);
        if (hit(context, bounds, enabled) && binding)
            bongo_cat_preferences_behavior_rename_begin(value, entry, binding,
                bounds);
    }
}

void bongo_cat_preferences_behavior_row_draw(BongoCatPreferences *value,
    struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect row, BongoCatBehaviorEntry *entry, BongoCatUIPalette p,
    float opacity, bool enabled) {
    struct nk_rect play = nk_rect(row.x + row.w - 52, row.y + 10, 36, 36);
    struct nk_rect shortcut_bounds = nk_rect(play.x - 188, row.y + 10, 180, 36);
    struct nk_rect name = nk_rect(row.x + 8, row.y + 9,
        NK_MAX(48.0f, shortcut_bounds.x - row.x - 16), 38);
    BongoCatBehaviorShortcut *binding = binding_for(&value->app->config,
        entry->id);
    draw_name(value, context, canvas, name, entry, binding, p, opacity, enabled);
    bool play_hover = enabled && nk_input_is_mouse_hovering_rect(
        &context->input, play);
    nk_fill_rect(canvas, play, 10, alpha(play_hover ? p.hover : p.field, opacity));
    nk_stroke_rect(canvas, play, 10, 1, alpha(play_hover ? p.accent :
        p.border_subtle, opacity));
    bongo_cat_preferences_icon_draw(value, canvas, BONGO_CAT_UI_ICON_PLAY,
        nk_rect(play.x + 10, play.y + 10, 16, 16),
        alpha(play_hover ? p.accent : p.text, opacity));
    if (play_hover) bongo_cat_ui_cursor_hover_rect(context, play,
        BONGO_CAT_UI_CURSOR_POINTER);
    if (hit(context, play, enabled)) {
        const char *label = display_label(entry, binding);
        bongo_cat_app_run_behavior(value->app, entry);
        char message[BONGO_CAT_ID_CAP + 5];
        snprintf(message, sizeof(message), "%s \xE2\x9C\x93", label);
        bongo_cat_preferences_notice_show(value->app, message, false);
    }
    if (!binding) return;
    char id[BONGO_CAT_ID_CAP + 16];
    snprintf(id, sizeof(id), "behavior-%.*s", (int)sizeof(id) - 10, entry->id);
    if (shortcut_editor(value, context, canvas, shortcut_bounds, id, binding,
        p, opacity, enabled))
        bongo_cat_preferences_shortcut_begin(value, id, binding->shortcut,
            sizeof(binding->shortcut));
}
