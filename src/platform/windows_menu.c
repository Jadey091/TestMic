#include "bongo_cat/platform.h"
#include "windows_borderless.h"
#include "windows_popup.h"
#include "../ui/ui_native_theme.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <stdlib.h>
#include <windows.h>

static HWND native_window(BongoCatPlatform *platform) {
    return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

static wchar_t *wide(const char *text) {
    if (!text) return NULL;
    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *value = length > 0 ? calloc((size_t)length, sizeof(*value)) : NULL;
    if (value) MultiByteToWideChar(CP_UTF8, 0, text, -1, value, length);
    return value;
}

static void menu_text(HMENU menu, UINT flags, UINT_PTR id, const char *text) {
    wchar_t *label = wide(text);
    AppendMenuW(menu, flags, id, label ? label : L"");
    free(label);
}

static void destroy_unattached(HMENU menu, HMENU sizes, HMENU opacity,
    HMENU models, HMENU motions, HMENU expressions) {
    if (menu) DestroyMenu(menu);
    if (sizes) DestroyMenu(sizes);
    if (opacity) DestroyMenu(opacity);
    if (models) DestroyMenu(models);
    if (motions) DestroyMenu(motions);
    if (expressions) DestroyMenu(expressions);
}

BongoCatMenuAction bongo_cat_platform_context_menu(BongoCatPlatform *platform,
    const BongoCatMenuLabels *labels) {
    if (!platform || !labels) return BONGO_CAT_MENU_NONE;
    HMENU menu = CreatePopupMenu(), sizes = CreatePopupMenu(), opacity = CreatePopupMenu();
    HMENU models = CreatePopupMenu();
    HMENU motions = labels->motion_count ? CreatePopupMenu() : NULL;
    HMENU expressions = labels->expression_count ? CreatePopupMenu() : NULL;
    if (!menu || !sizes || !opacity || !models ||
        (labels->motion_count && !motions) ||
        (labels->expression_count && !expressions)) {
        destroy_unattached(menu, sizes, opacity, models, motions, expressions);
        return BONGO_CAT_MENU_NONE;
    }
    menu_text(menu, MF_STRING, BONGO_CAT_MENU_PREFERENCES, labels->preferences);
    menu_text(menu, MF_STRING, BONGO_CAT_MENU_HIDE, labels->hide);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    menu_text(menu, MF_STRING | (labels->pass_through_checked ? MF_CHECKED : 0),
        BONGO_CAT_MENU_PASS_THROUGH, labels->pass_through);
    menu_text(menu, MF_STRING | (labels->always_on_top_checked ? MF_CHECKED : 0),
        BONGO_CAT_MENU_ALWAYS_ON_TOP, labels->always_on_top);
    for (int i = 0; i < 16; ++i) {
        int scale = 50 + i * 10;
        wchar_t label[16]; swprintf(label, 16, L"%d%%", scale);
        UINT flags = MF_STRING | (SDL_fabsf(labels->scale_percent - scale) < .5f ? MF_CHECKED : 0);
        AppendMenuW(sizes, flags, BONGO_CAT_MENU_SCALE_50 + i, label);
    }
    menu_text(sizes, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
        labels->wheel_size_hint);
    const int opacities[] = {10,20,30,40,50,60,70,80,90,100};
    for (int i = 0; i < 10; ++i) {
        wchar_t label[16]; swprintf(label, 16, L"%d%%", opacities[i]);
        UINT flags = MF_STRING | (SDL_fabsf(labels->opacity_percent - opacities[i]) < .5f ? MF_CHECKED : 0);
        AppendMenuW(opacity, flags, BONGO_CAT_MENU_OPACITY_10 + i, label);
    }
    menu_text(opacity, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
        labels->wheel_opacity_hint);
    for (size_t i = 0; i < labels->motion_count; ++i)
        menu_text(motions, MF_STRING, BONGO_CAT_MENU_MOTION_FIRST + i,
            labels->motion_names[i]);
    for (size_t i = 0; i < labels->expression_count; ++i)
        menu_text(expressions, MF_STRING | (i == labels->current_expression ? MF_CHECKED : 0),
            BONGO_CAT_MENU_EXPRESSION_FIRST + i, labels->expression_names[i]);
    for (size_t i = 0; i < labels->model_count; ++i)
        menu_text(models, MF_STRING | (i == labels->current_model ? MF_CHECKED : 0),
            BONGO_CAT_MENU_MODEL_FIRST + i, labels->model_names[i]);
    if (labels->model_count) AppendMenuW(models, MF_SEPARATOR, 0, NULL);
    menu_text(models, MF_STRING, BONGO_CAT_MENU_MODEL_ADD, labels->add_model);
    wchar_t *size_label = wide(labels->window_size), *opacity_label = wide(labels->opacity);
    wchar_t *model_label = wide(labels->model), *motion_label = wide(labels->motion);
    wchar_t *expression_label = wide(labels->expression);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)sizes, size_label ? size_label : L"");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)opacity, opacity_label ? opacity_label : L"");
    if (labels->motion_count) AppendMenuW(menu, MF_POPUP, (UINT_PTR)motions,
        motion_label ? motion_label : L"");
    if (labels->expression_count) AppendMenuW(menu, MF_POPUP, (UINT_PTR)expressions,
        expression_label ? expression_label : L"");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)models, model_label ? model_label : L"");
    free(size_label); free(opacity_label); free(model_label); free(motion_label);
    free(expression_label);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    menu_text(menu, MF_STRING, BONGO_CAT_MENU_EXIT, labels->exit);
    POINT point; GetCursorPos(&point);
    HWND window = native_window(platform);
    bongo_cat_ui_native_menu_prepare(platform->window, labels->dark_theme);
    bongo_cat_windows_menu_preview(window, labels->preview,
        labels->preview_tick, labels->preview_userdata);
    UINT command = bongo_cat_windows_popup_track(window, menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y);
    bongo_cat_windows_menu_preview(window, NULL, NULL, NULL);
    if (labels->restore) labels->restore(labels->preview_userdata, (BongoCatMenuAction)command);
    DestroyMenu(menu);
    return (BongoCatMenuAction)command;
}
#endif
