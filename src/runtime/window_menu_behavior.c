#include "window_menu.h"

#include <string.h>

static const BongoCatBehaviorEntry *nth_behavior(BongoCatApp *app,
    BongoCatBehaviorKind kind, size_t position) {
    if (!app) return NULL;
    for (size_t i = 0; i < app->behaviors.count; ++i) {
        const BongoCatBehaviorEntry *entry = &app->behaviors.entries[i];
        if (entry->kind != kind) continue;
        if (!position) return entry;
        position--;
    }
    return NULL;
}

static const char *behavior_shortcut(const BongoCatApp *app, const char *id) {
    if (!app || !id) return NULL;
    for (size_t i = 0; i < app->config.behavior_shortcut_count; ++i) {
        const BongoCatBehaviorShortcut *binding = &app->config.behavior_shortcuts[i];
        if (strcmp(binding->id, id) == 0) return binding->shortcut;
    }
    return NULL;
}

static const char *behavior_label(const BongoCatApp *app,
    const BongoCatBehaviorEntry *entry) {
    if (!app || !entry) return "";
    for (size_t i = 0; i < app->config.behavior_shortcut_count; ++i) {
        const BongoCatBehaviorShortcut *binding = &app->config.behavior_shortcuts[i];
        if (strcmp(binding->id, entry->id) == 0 && binding->label[0])
            return binding->label;
    }
    return entry->label;
}

static bool run_binding(BongoCatApp *app, const BongoCatBehaviorEntry *selected) {
    const char *shortcut = behavior_shortcut(app, selected->id);
    bool handled = bongo_cat_app_run_behavior(app, selected);
    if (!shortcut || !shortcut[0]) return handled;
    for (size_t i = 0; i < app->behaviors.count; ++i) {
        const BongoCatBehaviorEntry *entry = &app->behaviors.entries[i];
        const char *bound = behavior_shortcut(app, entry->id);
        if (entry != selected && bound && strcmp(bound, shortcut) == 0)
            handled = bongo_cat_app_run_behavior(app, entry) || handled;
    }
    return handled;
}

void bongo_cat_window_behavior_labels(BongoCatApp *app,
    const char **motions, size_t *motion_count,
    const char **expressions, size_t *expression_count,
    size_t *current_expression) {
    if (!motion_count || !expression_count) return;
    *motion_count = 0; *expression_count = 0;
    if (current_expression) *current_expression = BONGO_CAT_BEHAVIOR_CAP;
    if (!app) return;
    int active_expression = bongo_cat_live2d_expression(app->live2d);
    for (size_t i = 0; i < app->behaviors.count; ++i) {
        const BongoCatBehaviorEntry *entry = &app->behaviors.entries[i];
        if (entry->kind == BONGO_CAT_BEHAVIOR_MOTION && motions)
            motions[(*motion_count)++] = behavior_label(app, entry);
        else if (entry->kind == BONGO_CAT_BEHAVIOR_EXPRESSION && expressions) {
            if (current_expression && entry->index == active_expression)
                *current_expression = *expression_count;
            expressions[(*expression_count)++] = behavior_label(app, entry);
        }
    }
}

bool bongo_cat_window_behavior_action(BongoCatApp *app,
    BongoCatMenuAction action) {
    BongoCatBehaviorKind kind;
    size_t position;
    if (action >= BONGO_CAT_MENU_MOTION_FIRST &&
        action < BONGO_CAT_MENU_MOTION_FIRST + BONGO_CAT_BEHAVIOR_CAP) {
        kind = BONGO_CAT_BEHAVIOR_MOTION;
        position = (size_t)(action - BONGO_CAT_MENU_MOTION_FIRST);
    } else if (action >= BONGO_CAT_MENU_EXPRESSION_FIRST &&
        action < BONGO_CAT_MENU_EXPRESSION_FIRST + BONGO_CAT_BEHAVIOR_CAP) {
        kind = BONGO_CAT_BEHAVIOR_EXPRESSION;
        position = (size_t)(action - BONGO_CAT_MENU_EXPRESSION_FIRST);
    } else return false;
    const BongoCatBehaviorEntry *entry = nth_behavior(app, kind, position);
    return entry && run_binding(app, entry);
}

bool bongo_cat_window_behavior_menu_action(BongoCatMenuAction action) {
    return (action >= BONGO_CAT_MENU_MOTION_FIRST &&
        action < BONGO_CAT_MENU_MOTION_FIRST + BONGO_CAT_BEHAVIOR_CAP) ||
        (action >= BONGO_CAT_MENU_EXPRESSION_FIRST &&
            action < BONGO_CAT_MENU_EXPRESSION_FIRST + BONGO_CAT_BEHAVIOR_CAP);
}

static bool expression_self_test(BongoCatApp *app) {
    size_t count = 0;
    while (nth_behavior(app, BONGO_CAT_BEHAVIOR_EXPRESSION, count)) count++;
    if (!count) return true;
    int original = bongo_cat_live2d_expression(app->live2d);
    size_t selected_position = count > 2 ? 2 : count - 1;
    const BongoCatBehaviorEntry *selected = nth_behavior(app,
        BONGO_CAT_BEHAVIOR_EXPRESSION, selected_position);
    bool passed = selected && bongo_cat_window_behavior_action(app,
        BONGO_CAT_MENU_EXPRESSION_FIRST + selected_position) &&
        bongo_cat_live2d_expression(app->live2d) == selected->index;

    const char *motions[BONGO_CAT_BEHAVIOR_CAP];
    const char *expressions[BONGO_CAT_BEHAVIOR_CAP];
    size_t motion_count, expression_count, current_expression;
    bongo_cat_window_behavior_labels(app, motions, &motion_count,
        expressions, &expression_count, &current_expression);
    passed = passed && expression_count == count &&
        current_expression == selected_position;

    if (passed && count > 1) {
        size_t alternate_position = selected_position ? 0 : 1;
        const BongoCatBehaviorEntry *alternate = nth_behavior(app,
            BONGO_CAT_BEHAVIOR_EXPRESSION, alternate_position);
        BongoCatWindowMenuPreview preview;
        bongo_cat_window_menu_preview_init(&preview, app);
        bongo_cat_window_menu_preview(&preview,
            BONGO_CAT_MENU_EXPRESSION_FIRST + alternate_position);
        passed = alternate && bongo_cat_live2d_expression(app->live2d) ==
            alternate->index;
        bongo_cat_window_menu_restore(&preview, BONGO_CAT_MENU_NONE);
        passed = passed && bongo_cat_live2d_expression(app->live2d) ==
            selected->index;

        bongo_cat_window_menu_preview_init(&preview, app);
        bongo_cat_window_menu_preview(&preview,
            BONGO_CAT_MENU_EXPRESSION_FIRST + alternate_position);
        bongo_cat_window_menu_restore(&preview,
            BONGO_CAT_MENU_EXPRESSION_FIRST + alternate_position);
        passed = passed && bongo_cat_live2d_expression(app->live2d) ==
            alternate->index;
    }
    return bongo_cat_live2d_set_expression(app->live2d, original) && passed;
}

bool bongo_cat_window_behavior_self_test(BongoCatApp *app) {
    if (!app) return false;
    bool passed = true;
    if (nth_behavior(app, BONGO_CAT_BEHAVIOR_MOTION, 0))
        passed = bongo_cat_window_behavior_action(app,
            BONGO_CAT_MENU_MOTION_FIRST) && passed;
    passed = expression_self_test(app) && passed;
    return passed;
}
