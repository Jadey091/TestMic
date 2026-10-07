#include "preferences_widgets.h"
#include "preferences_controls.h"
#include "preferences_shortcut_clear.h"
#include "ui_animation.h"
#include "ui_backend.h"
#include "ui_catime.h"
#include "ui_icons.h"
#include "ui_paint.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
typedef struct FormStyle {
    struct nk_style_item background;
    struct nk_color window_color;
    struct nk_color border_color;
    struct nk_vec2 padding;
    struct nk_vec2 spacing;
    float border;
} FormStyle;
static struct nk_context *section_context;
static bool section_first;
static int detail_lines(const struct nk_context *context, const char *text) {
    if (!text || !text[0]) return 0;
    const struct nk_user_font *font = bongo_cat_ui_caption_font(context);
    float width = nk_window_get_content_region(context).w - 310.0f;
    if (width < 220.0f) width = 220.0f;
    float measured = font->width(font->userdata, font->height,
        text, nk_strlen(text));
    int lines = (int)(measured / width) + 1;
    return NK_CLAMP(1, lines, 3);
}
static void restore_style(struct nk_context *context, const FormStyle *saved) {
    context->style.window.fixed_background = saved->background;
    context->style.window.background = saved->window_color;
    context->style.window.group_border_color = saved->border_color;
    context->style.window.group_padding = saved->padding;
    context->style.window.spacing = saved->spacing;
    context->style.window.group_border = saved->border;
}
static bool form_begin(struct nk_context *context, const char *id,
    int lines, FormStyle *saved) {
    saved->background = context->style.window.fixed_background;
    saved->window_color = context->style.window.background;
    saved->border_color = context->style.window.group_border_color;
    saved->padding = context->style.window.group_padding;
    saved->spacing = context->style.window.spacing;
    saved->border = context->style.window.group_border;
    struct nk_color clear = nk_rgba(0, 0, 0, 0);
    context->style.window.fixed_background = nk_style_item_color(clear);
    context->style.window.background = clear;
    context->style.window.group_border_color = clear;
    context->style.window.group_padding = nk_vec2(13, 11);
    context->style.window.spacing = nk_vec2(8, 3);
    context->style.window.group_border = 0;
    float height = lines ? 92.0f : 76.0f;
    if (section_context == context && section_first && !lines) height = 69.0f;
    if (section_context == context) section_first = false;
    nk_layout_row_dynamic(context, height, 1);
    if (!nk_group_begin(context, id, NK_WINDOW_NO_SCROLLBAR)) {
        restore_style(context, saved);
        return false;
    }
    return true;
}
static void form_end(struct nk_context *context, const FormStyle *saved) {
    nk_group_end(context);
    restore_style(context, saved);
}
static float left_width(struct nk_context *context) {
    return NK_MAX(220.0f, nk_window_get_content_region(context).w - 228.0f);
}
static void form_title_sized(struct nk_context *context, const char *title,
    float control_width) {
    float available = nk_window_get_content_region(context).w;
    float left = NK_MAX(220.0f, available - control_width - 8.0f);
    nk_layout_row_begin(context, NK_STATIC, 36, 2);
    nk_layout_row_push(context, left);
    nk_style_push_font(context, bongo_cat_ui_label_font(context));
    struct nk_vec2 text_padding = context->style.text.padding;
    context->style.text.padding.x += 5.0f;
    nk_label(context, title, NK_TEXT_LEFT);
    context->style.text.padding = text_padding;
    nk_style_pop_font(context);
    nk_layout_row_push(context, NK_MAX(control_width, available - left - 8.0f));
}
static void form_title(struct nk_context *context, const char *title) {
    form_title_sized(context, title, 220.0f);
}
static void description(struct nk_context *context, const char *text,
    int lines) {
    if (!lines) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    float left = left_width(context);
    nk_layout_row_begin(context, NK_STATIC, 22.0f * lines, 2);
    nk_layout_row_push(context, left);
    nk_style_push_font(context, bongo_cat_ui_caption_font(context));
    struct nk_vec2 text_padding = context->style.text.padding;
    context->style.text.padding.x += 5.0f;
    nk_label_colored_wrap(context, text, p.muted);
    context->style.text.padding = text_padding;
    nk_style_pop_font(context);
    nk_layout_row_push(context, NK_MAX(1.0f,
        nk_window_get_content_region(context).w - left - 8.0f));
    nk_spacing(context, 1);
    nk_layout_row_end(context);
}
static bool secondary_button(struct nk_context *context, const char *label) {
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_style_button style = context->style.button;
    style.normal = nk_style_item_color(p.field);
    style.hover = nk_style_item_color(p.selection);
    style.active = nk_style_item_color(p.selection);
    style.border_color = p.border_subtle;
    style.border = 1;
    style.rounding = 10;
    style.text_normal = p.text;
    style.text_hover = p.accent;
    style.text_active = p.accent;
    struct nk_rect bounds = nk_widget_bounds(context);
    if (nk_input_is_mouse_hovering_rect(&context->input, bounds))
        bongo_cat_ui_cursor_hover_rect(context, bounds, BONGO_CAT_UI_CURSOR_POINTER);
    return nk_button_label_styled(context, &style, label) != 0;
}
void bongo_cat_pref_section(struct nk_context *context, const char *title) {
    section_context = context; section_first = true;
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, 22, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, nk_rect(bounds.x, bounds.y + 2, 4, 18), 2, p.pink);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(bounds.x + 14,
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 14, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.text);
}

bool bongo_cat_pref_toggle(struct nk_context *context, const char *id,
    const char *title, const char *detail, bool *value) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title_sized(context, title, 80.0f);
    bool changed = bongo_cat_pref_control_toggle(context, id, value);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_float(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_float(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_int(struct nk_context *context, const char *id,
    const char *title, const char *detail, int minimum, int *value,
    int maximum, int step, int default_value) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_int(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}
bool bongo_cat_pref_slider(struct nk_context *context, const char *id,
    const char *title, const char *detail, float minimum, float *value,
    float maximum, float step, float default_value) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title);
    bool changed = bongo_cat_pref_control_slider(context, id,
        minimum, value, maximum, step, default_value);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return changed;
}

int bongo_cat_pref_combo(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *const *items,
    int count, int selected) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return selected;
    form_title_sized(context, title, 176.0f);
    selected = bongo_cat_pref_control_combo(context, id,
        items, count, selected);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return selected;
}

int bongo_cat_pref_edit(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *value,
    bool recording, const char *idle_hint, const char *record_hint) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    const char *shown = recording ? record_hint : (value && value[0] ? value : idle_hint);
    const struct nk_user_font *font = bongo_cat_ui_body_font(context);
    float width = font->width(font->userdata, font->height, shown, nk_strlen(shown));
    float control_width = NK_CLAMP(180.0f, width + 64.0f, 260.0f);
    form_title_sized(context, title, control_width);
    struct nk_rect bounds;
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) {
        nk_layout_row_end(context); form_end(context, &saved); return false;
    }
    const float effect_margin = 6.0f;
    bounds = nk_rect(bounds.x + bounds.w - control_width, bounds.y,
        control_width - effect_margin, bounds.h);
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    bool hover = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    char hover_id[80]; snprintf(hover_id, sizeof(hover_id), "shortcut-hover-%s", id);
    float hover_amount = bongo_cat_ui_animate_eased(context, hover_id,
        hover ? 1.0f : 0.0f, 200, BONGO_CAT_UI_EASE_STANDARD);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 10, recording ? p.hover_pink :
        bongo_cat_ui_color_mix(p.field, p.hover, hover_amount));
    nk_stroke_rect(canvas, bounds, 10, recording ? 2.0f : 1.0f,
        recording ? p.pink : bongo_cat_ui_color_mix(
        p.border_subtle, p.accent, hover_amount));
    if (recording) {
        float phase = (float)(SDL_GetTicksNS() % 1500000000ULL) /
            1500000000.0f;
        float pulse = .5f - .5f * cosf(phase * 6.2831853f);
        struct nk_rect glow = nk_rect(bounds.x - pulse * 5,
            bounds.y - pulse * 5, bounds.w + pulse * 10,
            bounds.h + pulse * 10);
        nk_stroke_rect(canvas, glow, 12, 2,
            nk_rgba(p.pink.r, p.pink.g, p.pink.b,
            (nk_byte)(150 * (1.0f - pulse))));
    }
    bool has_shortcut = value && value[0];
    bool can_clear = !recording && has_shortcut;
    bool show_keyboard = recording || !has_shortcut;
    float icon_width = show_keyboard ? 26.0f : 0.0f;
    float text_width = NK_MIN(width, bounds.w - icon_width -
        (can_clear ? 34.0f : 16.0f));
    float group_width = icon_width + text_width + (can_clear ? 20.0f : 0.0f);
    float group_x = bounds.x + NK_MAX(14.0f, (bounds.w - group_width) * .5f);
    float text_x = group_x + icon_width;
    struct nk_color icon_color = recording ? p.pink :
        bongo_cat_ui_color_mix(p.muted, p.accent, hover_amount);
    if (show_keyboard) {
        struct nk_rect keyboard = nk_rect(group_x, bounds.y + 10, 18, 18);
        if (!bongo_cat_ui_draw_icon(canvas, BONGO_CAT_UI_ICON_KEYBOARD,
            keyboard, icon_color)) {
            nk_stroke_rect(canvas, keyboard, 3, 1.5f, icon_color);
            for (int i = 0; i < 3; ++i)
                nk_stroke_line(canvas, keyboard.x + 3 + i * 3, keyboard.y + 4,
                    keyboard.x + 4 + i * 3, keyboard.y + 4, 1, icon_color);
            nk_stroke_line(canvas, keyboard.x + 3, keyboard.y + 8,
                keyboard.x + 10, keyboard.y + 8, 1, icon_color);
        }
    }
    struct nk_rect text = nk_rect(text_x, bounds.y + (bounds.h - font->height) * .5f,
        NK_MIN(width + 1, bounds.x + bounds.w - text_x -
        (can_clear ? 24.0f : 8.0f)), font->height);
    nk_draw_text(canvas, text, shown, nk_strlen(shown), font, nk_rgba(0, 0, 0, 0),
        recording ? p.pink : (has_shortcut ? p.text :
        bongo_cat_ui_color_mix(p.muted, p.accent, hover_amount)));
    struct nk_rect clear_bounds = nk_rect(bounds.x + bounds.w - 25,
        bounds.y + 8, 17, 20);
    bool clear = bongo_cat_pref_shortcut_clear(context, canvas, id,
        clear_bounds, p, 1.0f, can_clear);
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds,
        BONGO_CAT_UI_CURSOR_POINTER);
    bool clicked = !clear && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, bounds) != 0;
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved);
    return clear ? -1 : clicked;
}

bool bongo_cat_pref_button(struct nk_context *context, const char *id,
    const char *title, const char *detail, const char *button) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return false;
    form_title(context, title); bool result = secondary_button(context, button);
    nk_layout_row_end(context); description(context, detail, lines);
    form_end(context, &saved); return result;
}

void bongo_cat_pref_status(struct nk_context *context, const char *id,
    const char *title, const char *detail) {
    int lines = detail_lines(context, detail); FormStyle saved;
    if (!form_begin(context, id, lines, &saved)) return;
    BongoCatUIPalette p = bongo_cat_ui_palette(bongo_cat_ui_dark(context));
    nk_layout_row_dynamic(context, 44, 1);
    struct nk_rect row; nk_widget(&row, context);
    nk_fill_circle(nk_window_get_canvas(context),
        nk_rect(row.x, row.y + (row.h - 10) * .5f, 10, 10), p.accent);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(row.x + 20,
        row.y + (row.h - font->height) * .5f, row.w - 20, font->height);
    nk_draw_text(nk_window_get_canvas(context), text, title, nk_strlen(title),
        font, nk_rgba(0, 0, 0, 0), p.text);
    description(context, detail, lines); form_end(context, &saved);
}
