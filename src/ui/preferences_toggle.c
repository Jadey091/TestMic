#include "preferences_controls.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_paint.h"

#include <stdio.h>

bool bongo_cat_pref_control_toggle(struct nk_context *context,
    const char *id, bool *value) {
    struct nk_rect cell;
    if (nk_widget(&cell, context) == NK_WIDGET_INVALID) return false;
    const float effect_margin = 18.0f;
    struct nk_rect track = nk_rect(cell.x + cell.w - 46 - effect_margin,
        cell.y + (cell.h - 24) * .5f, 46, 24);
    struct nk_rect interaction = nk_rect(track.x, track.y,
        track.w + effect_margin, track.h);
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, interaction);
    bool changed = hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, interaction);
    if (changed) *value = !*value;
    float progress = bongo_cat_ui_animate_eased(context, id,
        *value ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_SPRING);
    char hover_id[80];
    snprintf(hover_id, sizeof(hover_id), "toggle-hover-%s", id);
    float hover_amount = bongo_cat_ui_animate_eased(context, hover_id,
        hover ? 1.0f : 0.0f, 250.0f, BONGO_CAT_UI_EASE_STANDARD);
    float scale = 1.0f + .05f * hover_amount;
    track = nk_rect(track.x - track.w * (scale - 1.0f) * .5f,
        track.y - track.h * (scale - 1.0f) * .5f,
        track.w * scale, track.h * scale);
    BongoCatUIPalette p = bongo_cat_ui_palette(
        bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float state_amount = NK_CLAMP(0.0f, progress, 1.0f);
    if (state_amount > 0.0f && p.effects)
        bongo_cat_ui_paint_shadow(context, track, 12, 0, 2, 8, 0,
            nk_rgba(p.accent.r, p.accent.g, p.accent.b,
            (nk_byte)(89 * state_amount)));
    nk_fill_rect(canvas, track, 12,
        bongo_cat_ui_color_mix(p.field, p.accent, state_amount));
    nk_stroke_rect(canvas, track, 12, 1.0f, bongo_cat_ui_color_mix(
        p.border_subtle, p.accent, state_amount));
    float knob_size = 18.0f * scale;
    float knob_x = track.x + 3.0f * scale +
        (track.w - 6.0f * scale - knob_size) * progress;
    struct nk_rect knob = nk_rect(knob_x,
        track.y + (track.h - knob_size) * .5f, knob_size, knob_size);
    if (p.effects) bongo_cat_ui_paint_shadow(context, knob,
        knob_size * .5f, 0, 2, 5, 0, nk_rgba(0, 0, 0, 51));
    nk_fill_circle(canvas, knob, nk_rgb(255, 255, 255));
    if (hover) bongo_cat_ui_cursor_hover_rect(context, interaction,
        BONGO_CAT_UI_CURSOR_POINTER);
    return changed;
}
