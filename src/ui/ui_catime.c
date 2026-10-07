#include "ui_catime.h"
#include "ui_backend.h"
#include "ui_animation.h"
#include "ui_icons.h"
#include "ui_paint.h"

#include <math.h>
static BongoCatUIIconDraw external_icon_draw;
static void *external_icon_userdata;

void bongo_cat_ui_set_icons(BongoCatUIIconDraw draw_icon,
    void *icon_userdata) {
    external_icon_draw = draw_icon;
    external_icon_userdata = icon_userdata;
}

bool bongo_cat_ui_draw_icon(struct nk_command_buffer *canvas, int icon,
    struct nk_rect bounds, struct nk_color color) {
    if (!external_icon_draw) return false;
    external_icon_draw(external_icon_userdata, canvas, icon, bounds, color);
    return true;
}

static void centered_span(struct nk_command_buffer *canvas,
    struct nk_rect bounds, const char *text, int length,
    const struct nk_user_font *font, struct nk_color color) {
    float width = font->width(font->userdata, font->height, text, length);
    float target_width = NK_MIN(bounds.w, width + 1.0f);
    struct nk_rect target = nk_rect(bounds.x + (bounds.w - target_width) * .5f,
        bounds.y + (bounds.h - font->height) * .5f,
        target_width, font->height);
    nk_draw_text(canvas, target, text, length, font,
        nk_rgba(0, 0, 0, 0), color);
}

static void centered(struct nk_command_buffer *canvas, struct nk_rect bounds,
    const char *text, const struct nk_user_font *font, struct nk_color color) {
    centered_span(canvas, bounds, text, nk_strlen(text), font, color);
}

float bongo_cat_ui_sidebar_width(float window_width) {
    return window_width <= 780.0f ? BONGO_CAT_UI_SIDEBAR_NARROW :
        BONGO_CAT_UI_SIDEBAR_WIDTH;
}

void bongo_cat_ui_shell_draw(struct nk_context *context, float width,
    float height, bool dark, bool native_frame) {
    BongoCatUIPalette p = bongo_cat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float side = bongo_cat_ui_sidebar_width(width);
    float rounding = native_frame ? 0.0f : 24.0f;
    float sidebar_right = BONGO_CAT_UI_MARGIN + side;
    struct nk_rect surface = nk_rect(0, 0, width, height);
    BongoCatUIBackend *backend = bongo_cat_ui_backend_for_context(context);
    bool fast = backend && backend->live_resize_fast;
    if (native_frame) {
        nk_fill_rect(canvas, surface, 0, p.surface_glass);
        nk_fill_rect(canvas, nk_rect(0, 0, sidebar_right, height), 0,
            p.surface);
    } else if (fast) {
        nk_fill_rect(canvas, surface, rounding, p.surface_glass);
        nk_push_scissor(canvas, nk_rect(0, 0, sidebar_right, height));
        nk_fill_rect(canvas, surface, rounding, p.surface);
        nk_push_scissor(canvas, surface);
    } else {
        bongo_cat_ui_paint_rounded_surface(context, surface, rounding,
            p.surface_glass);
        nk_push_scissor(canvas, nk_rect(0, 0, sidebar_right, height));
        bongo_cat_ui_paint_rounded_surface(context, surface, rounding,
            p.surface);
        nk_push_scissor(canvas, surface);
    }
    if (p.effects && !fast)
        bongo_cat_ui_paint_sidebar_glow(context, surface, sidebar_right,
            rounding,
            nk_rgba(p.accent.r, p.accent.g, p.accent.b, 56));
    nk_stroke_line(canvas, sidebar_right, BONGO_CAT_UI_MARGIN, sidebar_right,
        height - BONGO_CAT_UI_MARGIN, 1,
        p.border_subtle);
}

static void signature(struct nk_command_buffer *canvas, struct nk_rect bounds,
    BongoCatUIPalette p) {
    float x = bounds.x + (bounds.w - 90) * .5f, y = bounds.y + 124;
    struct nk_color glow = p.effects ?
        nk_rgba(p.pink.r, p.pink.g, p.pink.b, 89) : p.pink;
    nk_stroke_curve(canvas, x + 3, y + 8, x + 25, y + 12,
        x + 55, y + 10, x + 87, y + 6, 6, glow);
    float previous_x = x + 3, previous_y = y + 8;
    for (int i = 1; i <= 24; ++i) {
        float t = i / 24.0f, u = 1.0f - t;
        float next_x = x + u * u * u * 3 + 3 * u * u * t * 25 +
            3 * u * t * t * 55 + t * t * t * 87;
        float next_y = y + u * u * u * 8 + 3 * u * u * t * 12 +
            3 * u * t * t * 10 + t * t * t * 6;
        nk_stroke_line(canvas, previous_x, previous_y, next_x, next_y, 3,
            bongo_cat_ui_color_mix(p.accent, p.pink, t));
        previous_x = next_x; previous_y = next_y;
    }
}

bool bongo_cat_ui_header(struct nk_context *context, const char *title,
    const struct nk_user_font *font, unsigned int logo_texture,
    bool *title_clicked, bool interactive, bool dark) {
    struct nk_rect bounds;
    float height = nk_window_get_content_region(context).w < 100 ? 118.0f : 148.0f;
    nk_layout_row_dynamic(context, height, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    float size = bounds.w < 100 ? 54.0f : 72.0f;
    bool hover = interactive && nk_input_is_mouse_hovering_rect(&context->input,
        nk_rect(bounds.x, bounds.y, bounds.w, bounds.h));
    float hover_amount = bongo_cat_ui_animate_eased(context,
        "brand-logo-hover", hover ? 1.0f : 0.0f, 250.0f,
        BONGO_CAT_UI_EASE_SPRING);
    size *= 1.0f + .08f * hover_amount;
    struct nk_rect frame = nk_rect(bounds.x + (bounds.w - size) * .5f,
        bounds.y + (bounds.w < 100 ? 10.0f : 13.0f) -
        (size - (bounds.w < 100 ? 54.0f : 72.0f)) * .5f, size, size);
    float rounding = bounds.w < 100 ? 14.0f : 18.0f;
    if (p.effects) bongo_cat_ui_paint_shadow(context, frame, rounding,
        0, 5, 16, 0, nk_rgba(p.pink.r, p.pink.g, p.pink.b, 89));
    if (p.effects) bongo_cat_ui_paint_gradient(context, frame, rounding,
        p.accent, hover ? p.pink_hover : p.pink);
    else nk_fill_rect(canvas, frame, rounding, p.accent);
    struct nk_rect inner = nk_rect(frame.x + 3, frame.y + 3,
        frame.w - 6, frame.h - 6);
    nk_fill_rect(canvas, inner, bounds.w < 100 ? 11.0f : 15.0f, p.surface);
    if (logo_texture) {
        struct nk_image image = nk_image_id((int)logo_texture);
        float image_size = (bounds.w < 100 ? 42.0f : 56.0f) *
            (1.0f + .08f * hover_amount);
        struct nk_rect image_bounds = nk_rect(frame.x + (size - image_size) * .5f,
            frame.y + (size - image_size) * .5f, image_size, image_size);
        nk_draw_image(canvas, image_bounds, &image, nk_rgb(255, 255, 255));
    }
    if (bounds.w >= 100) {
        if (!font) font = bongo_cat_ui_caption_font(context);
        centered(canvas, nk_rect(bounds.x, bounds.y + 88, bounds.w, 28), title,
            font, p.pink);
        signature(canvas, bounds, p);
    }
    if (hover) bongo_cat_ui_cursor_hover_rect(context, bounds,
        BONGO_CAT_UI_CURSOR_POINTER);
    if (title_clicked) *title_clicked = hover &&
        nk_input_is_mouse_click_in_rect(&context->input, NK_BUTTON_LEFT, bounds);
    return false;
}
bool bongo_cat_ui_content_header(struct nk_context *context,
    const char *title, int icon, bool interactive, bool dark) {
    struct nk_rect bounds;
    nk_layout_row_dynamic(context, BONGO_CAT_UI_HEADER_HEIGHT, 1);
    if (nk_widget(&bounds, context) == NK_WIDGET_INVALID) return false;
    BongoCatUIPalette p = bongo_cat_ui_palette(dark);
    struct nk_command_buffer *canvas = nk_window_get_canvas(context);
    nk_fill_rect(canvas, bounds, 0,
        bongo_cat_ui_color_mix(p.surface_glass, p.surface, .2f));
    struct nk_rect icon_bounds = nk_rect(bounds.x + 20, bounds.y + 16, 22, 22);
    if (external_icon_draw) external_icon_draw(external_icon_userdata, canvas,
        icon, icon_bounds, p.accent);
    else bongo_cat_ui_fallback_icon(canvas, icon, icon_bounds, p.accent);
    const struct nk_user_font *font = bongo_cat_ui_label_font(context);
    struct nk_rect text = nk_rect(bounds.x + 52,
        bounds.y + (bounds.h - font->height) * .5f, bounds.w - 110, font->height);
    nk_draw_text(canvas, text, title, nk_strlen(title), font,
        nk_rgba(0, 0, 0, 0), p.accent);
    struct nk_rect close = nk_rect(bounds.x + bounds.w - 48, bounds.y + 11, 34, 34);
    bool hover = interactive && nk_input_is_mouse_hovering_rect(&context->input, close);
    struct nk_color color = hover ? p.accent : p.muted;
    if (!bongo_cat_ui_draw_icon(canvas, BONGO_CAT_UI_ICON_CLOSE,
        nk_rect(close.x + 7, close.y + 7, 20, 20), color)) {
        nk_stroke_line(canvas, close.x + 10, close.y + 10,
            close.x + 24, close.y + 24, 2, color);
        nk_stroke_line(canvas, close.x + 24, close.y + 10,
            close.x + 10, close.y + 24, 2, color);
    }
    if (hover) bongo_cat_ui_cursor_hover_rect(context, close,
        BONGO_CAT_UI_CURSOR_POINTER);
    return hover && nk_input_is_mouse_click_in_rect(&context->input,
        NK_BUTTON_LEFT, close);
}

bool bongo_cat_ui_close_hit(float x, float y, float width) {
    return x >= width - 58.0f && x <= width - 12.0f && y >= 14.0f && y <= 62.0f;
}

bool bongo_cat_ui_title_link_hit(float x, float y, float width) {
    return x >= 8.0f && x <= 8.0f + bongo_cat_ui_sidebar_width(width) &&
        y >= 8.0f && y <= 156.0f;
}

bool bongo_cat_ui_title_drag_hit(float x, float y, float width) {
    float side = bongo_cat_ui_sidebar_width(width);
    return x >= 8.0f + side && x <= width - 8.0f &&
        y >= 8.0f && y <= 8.0f + BONGO_CAT_UI_HEADER_HEIGHT &&
        !bongo_cat_ui_close_hit(x, y, width);
}
