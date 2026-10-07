#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static void audit_mouse(BongoCatApp *app, double x, double y) {
    if (!app->smoke_input_audit) return;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), app->data_root, "input-audit.txt")) return;
    FILE *file = bongo_cat_file_open(path, "ab");
    if (!file) return;
    fprintf(file, "mouse x=%.2f y=%.2f\n", x, y);
    fclose(file);
}

void bongo_cat_app_track_hover(BongoCatApp *app, double x, double y) {
    int window_x, window_y, width, height;
    SDL_GetWindowPosition(app->window, &window_x, &window_y);
    SDL_GetWindowSize(app->window, &width, &height);
    bool inside = x >= window_x && x <= window_x + width &&
        y >= window_y && y <= window_y + height;
    app->pointer_known = true;
    app->pointer_x = x;
    app->pointer_y = y;
    bongo_cat_window_schedule_pointer_hit(app);
    if (inside == app->hover_inside) return;
    app->hover_inside = inside;
    app->hover_deadline_ns = inside ? SDL_GetTicksNS() +
        (uint64_t)(app->config.window.hide_delay_seconds * 1000000000.0) : 0;
    if (!inside && app->hover_hidden) {
        bongo_cat_platform_set_opacity(&app->platform,
            app->config.window.opacity_percent / 100.0f);
        app->hover_hidden = false;
        bongo_cat_window_sync_click_through(app);
    }
}

void bongo_cat_app_update_hover(BongoCatApp *app, uint64_t now) {
    if (!app->config.window.hide_on_hover || !app->hover_inside || app->hover_hidden ||
        !app->hover_deadline_ns || now < app->hover_deadline_ns) return;
    bongo_cat_platform_set_opacity(&app->platform, 0.0f);
    app->hover_hidden = true;
    bongo_cat_window_sync_click_through(app);
}

static bool reconcile_button(BongoCatApp *app, bool *current, bool pressed,
    const char *parameter) {
    if (*current == pressed) return false;
    *current = pressed;
    bongo_cat_live2d_set_parameter(app->live2d, parameter,
        pressed ? 1.0f : 0.0f);
    if (!pressed) bongo_cat_window_mark_hit_dirty(app);
    app->dirty = true;
    return true;
}

void bongo_cat_app_apply_mouse(BongoCatApp *app) {
    if (!app) return;
    double target_x, target_y;
    bool received = bongo_cat_input_take_mouse(&app->input, &target_x, &target_y);
    float global_x = 0.0f, global_y = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetGlobalMouseState(&global_x, &global_y);
    bool button_changed = reconcile_button(app, &app->left_mouse_down,
        (buttons & SDL_BUTTON_LMASK) != 0, "ParamMouseLeftDown");
    button_changed = reconcile_button(app, &app->right_mouse_down,
        (buttons & SDL_BUTTON_RMASK) != 0, "ParamMouseRightDown") || button_changed;
    bool side_down = (buttons & (SDL_BUTTON_X1MASK | SDL_BUTTON_X2MASK)) != 0;
    if (app->side_mouse_down != side_down) {
        app->side_mouse_down = side_down;
        button_changed = true;
        app->dirty = true;
    }
    if (!received || target_x != global_x || target_y != global_y) {
        target_x = global_x;
        target_y = global_y;
    }
    bool moved = !app->pointer_known || app->pointer_x != target_x ||
        app->pointer_y != target_y;
    if (moved) {
        audit_mouse(app, target_x, target_y);
        bongo_cat_app_track_hover(app, target_x, target_y);
        // Refresh the hit pixel before the next button press. Waiting for the
        // scheduled frame can leave a stale transparent state for fast clicks.
        if (app->click_through_applied && !app->left_mouse_down &&
            !app->right_mouse_down)
            bongo_cat_window_capture_pointer_hit(app);
    }
    bongo_cat_window_sync_click_through(app);
    uint64_t now = SDL_GetTicksNS();
    app->mouse_last_ns = now;
    double model_x = target_x, model_y = target_y;
    bool model_moved = moved;
    if (app->model_render_options.mver_projection &&
        !app->config.model.mouse_centered &&
        !bongo_cat_app_map_mver_pointer(app, target_x, target_y,
            &model_x, &model_y, &model_moved)) return;
    if (app->config.model.ignore_mouse || (!model_moved && !button_changed)) return;
    bongo_cat_app_apply_mouse_coordinates(app, model_x, model_y,
        target_x, target_y);
}
