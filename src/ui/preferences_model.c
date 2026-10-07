#include "preferences_state.h"
#include "preferences_model_cover.h"
#include "preferences_widgets.h"
#include "preferences_notice.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_animation.h"
#include "ui_paint.h"
#include "ui_icons.h"
#include "bongo_cat/i18n.h"
#include "bongo_cat/path.h"
#include "bongo_cat/platform.h"
#include "bongo_cat/preferences.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *tr(BongoCatApp *app, const char *key,
    const char *fallback) {
    return bongo_cat_i18n_get(app->i18n, key, fallback);
}

void bongo_cat_preferences_import_complete(BongoCatApp *app,
    BongoCatResult result, const BongoCatError *error, size_t imported_count) {
    if (!app || !app->preferences) return;
    const char *message = result == BONGO_CAT_OK ? tr(app,
        "pages.preference.model.hints.importSuccess", "Model imported") :
        (error && error->message[0] ? error->message : "Model import failed");
    if (app->smoke) {
        if (result != BONGO_CAT_OK) app->exit_code = 1;
    } else bongo_cat_preferences_notice_show(app, message, result != BONGO_CAT_OK);
    if (result == BONGO_CAT_OK) {
        bongo_cat_preferences_reload_fonts(app->preferences);
        SDL_Log("Imported %zu model variant(s)", imported_count);
    }
    bongo_cat_preferences_invalidate(app->preferences);
    bongo_cat_preferences_render(app->preferences);
}

static const char *mode_label(BongoCatApp *app, BongoCatModelMode mode) {
    if (mode == BONGO_CAT_MODE_KEYBOARD)
        return tr(app, "native.modeKeyboard", "Keyboard");
    if (mode == BONGO_CAT_MODE_GAMEPAD)
        return tr(app, "native.modeGamepad", "Gamepad");
    return tr(app, "native.modeStandard", "Standard");
}

static const char *model_name(const BongoCatModelEntry *entry) {
    if (!strcmp(entry->id, "standard")) return "Standard";
    if (!strcmp(entry->id, "keyboard")) return "Keyboard";
    if (!strcmp(entry->id, "gamepad")) return "Gamepad";
    if (entry->display_name[0]) return entry->display_name;
    return entry->id;
}

static void text(struct nk_context *context, struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *value, struct nk_color color,
    const struct nk_user_font *font) {
    if (!font) font = context->style.font;
    nk_draw_text(canvas, bounds, value, nk_strlen(value), font,
        nk_rgba(0, 0, 0, 0), color);
}

static struct nk_rect outline_bounds(struct nk_rect bounds, float thickness) {
    float inset = thickness * .5f;
    return nk_rect(bounds.x + inset, bounds.y + inset,
        NK_MAX(1.0f, bounds.w - thickness),
        NK_MAX(1.0f, bounds.h - thickness));
}

static bool import_card(BongoCatPreferences *value,
    struct nk_context *context) {
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    bounds.y += 5.0f; bounds.h += 1.0f;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    float lift = bongo_cat_ui_animate_eased(context, "model-import-hover",
        hover ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_STANDARD);
    bounds.y -= 2.0f * lift;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (hover && p.effects) bongo_cat_ui_paint_shadow(context, bounds, 14,
        0, 8, 24, 0, nk_rgba(p.pink.r, p.pink.g, p.pink.b, 89));
    nk_fill_rect(canvas, bounds, 14, hover ? p.hover_pink : p.hover);
    bongo_cat_ui_paint_dashed_rounded(context, bounds, 14, 2, 5, 5,
        hover ? p.pink : p.accent);
    float cx = bounds.x + bounds.w * .5f, cy = bounds.y + 94;
    bongo_cat_preferences_icon_draw(value, canvas,
        BONGO_CAT_UI_ICON_UPLOAD, nk_rect(cx - 20, cy - 20, 40, 40), p.pink);
    const char *label = tr(value->app,
        "pages.preference.model.hints.clickOrDragToImport", "Import model directory");
    float width = value->ui.caption_font->width(value->ui.caption_font->userdata,
        value->ui.caption_font->height, label, nk_strlen(label));
    text(context, canvas, nk_rect(cx - width * .5f, cy + 30,
        NK_MIN(width + 1, bounds.w - 20), 24), label, p.accent,
        value->ui.caption_font);
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds,
        BONGO_CAT_UI_CURSOR_POINTER);
    return hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, bounds);
}

static void action_icon(BongoCatPreferences *value,
    struct nk_command_buffer *canvas, struct nk_rect r, int icon,
    struct nk_color color) {
    bongo_cat_preferences_icon_draw(value, canvas, icon,
        nk_rect(r.x + (r.w - 18) * .5f, r.y + (r.h - 18) * .5f, 18, 18), color);
}

static void select_model(BongoCatPreferences *value,
    const BongoCatModelEntry *entry) {
    BongoCatError error = {0};
    if (bongo_cat_app_select_model_with_error(value->app, entry->id, &error)) {
        bongo_cat_preferences_model_cover_capture(value->app, entry);
        bongo_cat_preferences_reload_fonts(value);
        bongo_cat_preferences_invalidate(value);
        return;
    }
    const char *message = tr(value->app, "native.modelLoadFailed",
        "Unable to display this model");
    bongo_cat_preferences_notice_show(value->app, message, true);
    bongo_cat_preferences_invalidate(value);
}

static void open_model_directory(BongoCatPreferences *value,
    const BongoCatModelEntry *entry) {
    const char *directory = entry->storage_directory[0] ?
        entry->storage_directory : entry->directory;
    if (bongo_cat_path_is_dir(directory) &&
        bongo_cat_platform_open_directory(directory)) return;
    bongo_cat_preferences_notice_show(value->app, tr(value->app,
        "pages.preference.model.hints.openDirectoryFailed",
        "Unable to open model directory"), true);
}

static void model_card(BongoCatPreferences *value, struct nk_context *context,
    BongoCatModelEntry *entry) {
    BongoCatApp *app = value->app;
    bool selected = !strcmp(entry->id, app->config.current_model);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    bounds.y += 5.0f; bounds.h += 1.0f;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    char animation_id[BONGO_CAT_ID_CAP + 24];
    snprintf(animation_id, sizeof(animation_id), "model-hover-%s", entry->id);
    float lift = bongo_cat_ui_animate_eased(context, animation_id,
        hover ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_SWIFT);
    bounds.y -= 3.0f * lift;
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    if (selected && p.effects) bongo_cat_ui_paint_shadow(context, bounds,
        14, 0, 8, 20, 0, nk_rgba(p.pink.r, p.pink.g, p.pink.b, 89));
    else if (hover && p.effects) bongo_cat_ui_paint_shadow(context, bounds,
        14, 0, 10, 28, 0, nk_rgba(p.accent.r, p.accent.g, p.accent.b, 46));
    nk_fill_rect(canvas, bounds, 14, p.surface);
    float preview_height = NK_MIN(128.0f, bounds.w * 354.0f / 612.0f);
    struct nk_rect preview = nk_rect(bounds.x + 1, bounds.y + 1,
        bounds.w - 2, preview_height);
    nk_fill_rect(canvas, preview, 12, p.field);
    float raster_scale = value->ui.raster_scale > 0.0f ?
        value->ui.raster_scale : 1.0f;
    const BongoCatModelCover *cover = bongo_cat_preferences_model_cover(
        app, entry, NK_MAX(1, (int)lroundf(preview.w * raster_scale)),
        NK_MAX(1, (int)lroundf(preview.h * raster_scale)));
    if (cover) {
        float scale = NK_MIN(preview.w / cover->width, preview.h / cover->height);
        struct nk_rect image = nk_rect(preview.x + (preview.w - cover->width * scale) * .5f,
            preview.y + (preview.h - cover->height * scale) * .5f,
            cover->width * scale, cover->height * scale);
        struct nk_image texture = nk_image_id((int)cover->texture);
        nk_draw_image(canvas, image, &texture, nk_rgb(255, 255, 255));
    }
    float info_y = preview.y + preview.h + 8;
    text(context, canvas, nk_rect(bounds.x + 13, info_y, bounds.w - 26, 22),
        model_name(entry), p.text, value->ui.label_font);
    text(context, canvas, nk_rect(bounds.x + 13, info_y + 24, bounds.w - 26, 20),
        mode_label(app, entry->mode), p.muted, value->ui.caption_font);
    struct nk_rect actions = nk_rect(bounds.x + 1, bounds.y + bounds.h - 39,
        bounds.w - 2, 38);
    nk_fill_rect(canvas, actions, 0, p.field);
    bool deletable = !entry->preset && !entry->managed;
    float action_width = actions.w / (deletable ? 3.0f : 2.0f);
    struct nk_rect first = nk_rect(actions.x, actions.y, action_width, actions.h);
    struct nk_rect second = nk_rect(first.x + first.w, actions.y,
        action_width, actions.h);
    struct nk_rect third = nk_rect(second.x + second.w, actions.y,
        action_width, actions.h);
    nk_stroke_line(canvas, second.x, second.y + 10, second.x,
        second.y + second.h - 10, 1, p.border_subtle);
    bool first_hover = nk_input_is_mouse_hovering_rect(&context->input, first);
    bool second_hover = nk_input_is_mouse_hovering_rect(&context->input, second);
    bool third_hover = deletable &&
        nk_input_is_mouse_hovering_rect(&context->input, third);
    if (first_hover) nk_fill_rect(canvas, first, 0, p.hover_pink);
    if (second_hover) nk_fill_rect(canvas, second, 0, p.hover_pink);
    if (third_hover) nk_fill_rect(canvas, third, 0, p.hover_pink);
    action_icon(value, canvas, first, BONGO_CAT_UI_ICON_SMILE,
        first_hover ? p.pink : p.muted);
    action_icon(value, canvas, second, BONGO_CAT_UI_ICON_FOLDER,
        second_hover ? p.pink : p.muted);
    if (deletable) {
        nk_stroke_line(canvas, third.x, third.y + 10, third.x,
            third.y + third.h - 10, 1, p.border_subtle);
        action_icon(value, canvas, third, BONGO_CAT_UI_ICON_TRASH,
            third_hover ? p.danger : p.muted);
    }
    float outline_width = selected ? 2.0f : 1.0f;
    struct nk_rect outline = outline_bounds(bounds, outline_width);
    struct nk_color outline_color = selected ? p.pink :
        bongo_cat_ui_color_mix(p.border_subtle, p.accent, lift);
    nk_stroke_rect(canvas, outline, 14.0f - outline_width * .5f,
        outline_width, outline_color);
    if (first_hover || second_hover || third_hover || hover)
        bongo_cat_ui_cursor_hover_rect(context, bounds,
            BONGO_CAT_UI_CURSOR_POINTER);
    if (first_hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, first)) {
        if (selected)
            bongo_cat_preferences_behavior_dialog_open(value);
        else select_model(value, entry);
    } else if (second_hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, second)) open_model_directory(value, entry);
    else if (third_hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, third))
        bongo_cat_preferences_remove_dialog_open(app, entry->id);
    else if (hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, bounds)) select_model(value, entry);
}

static bool is_builtin(const char *id) {
    return !strcmp(id, "standard") || !strcmp(id, "keyboard") ||
        !strcmp(id, "gamepad");
}

static bool has_nearby(const BongoCatApp *app) {
    for (size_t i = 0; i < app->models.count; ++i)
        if (app->models.entries[i].managed) return true;
    return false;
}

static void smoke_model_behavior(BongoCatPreferences *value) {
    BongoCatApp *app = value->app;
    if (app->smoke_preference_model_select) {
        for (size_t i = 0; i < app->models.count; ++i) {
            BongoCatModelEntry *entry = &app->models.entries[i];
            if (!entry->managed || !strcmp(entry->id,
                app->config.current_model)) continue;
            app->smoke_preference_model_select = false;
            value->smoke_behavior_open_pending = true;
            SDL_Log("Preferences smoke selecting nearby model %s", entry->id);
            select_model(value, entry);
            return;
        }
    }
    if (value->smoke_behavior_open_pending && !value->font_reload_pending) {
        value->smoke_behavior_open_pending = false;
        bongo_cat_preferences_behavior_dialog_open(value);
    }
}

static void draw_named(BongoCatPreferences *value, struct nk_context *context,
    const char *id) {
    for (size_t i = 0; i < value->app->models.count; ++i)
        if (!strcmp(value->app->models.entries[i].id, id)) {
            model_card(value, context, &value->app->models.entries[i]); return;
        }
}

void bongo_cat_preferences_page_model(BongoCatPreferences *value,
    struct nk_context *context) {
    BongoCatApp *app = value->app;
    smoke_model_behavior(value);
    bongo_cat_preferences_model_covers_begin();
    bongo_cat_pref_section(context,
        tr(app, "pages.preference.model.title", "Installed models"));
    float width = nk_window_get_content_region(context).w;
    int columns = width >= 780 ? 4 : width >= 620 ? 3 : width >= 400 ? 2 : 1;
    struct nk_vec2 old_spacing = context->style.window.spacing;
    context->style.window.spacing = nk_vec2(14, 17);
    nk_layout_row_dynamic(context, 222, columns);
    if (import_card(value, context))
        bongo_cat_preferences_request_model_import(app->preferences);
    draw_named(value, context, "standard");
    draw_named(value, context, "keyboard");
    draw_named(value, context, "gamepad");
    for (size_t i = 0; i < app->models.count; ++i)
        if (!is_builtin(app->models.entries[i].id) &&
            !app->models.entries[i].managed)
            model_card(value, context, &app->models.entries[i]);
    context->style.window.spacing = old_spacing;
    if (has_nearby(app)) {
        bongo_cat_pref_section(context, tr(app,
            "pages.preference.model.labels.nearbyModels", "Nearby folders"));
        context->style.window.spacing = nk_vec2(14, 17);
        nk_layout_row_dynamic(context, 222, columns);
        for (size_t i = 0; i < app->models.count; ++i)
            if (app->models.entries[i].managed)
                model_card(value, context, &app->models.entries[i]);
        context->style.window.spacing = old_spacing;
    }
    bongo_cat_preferences_model_covers_prune(app);
}
