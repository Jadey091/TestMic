#include "runtime.h"
#include "bongo_cat/microphone.h"

bool bongo_cat_app_step_live2d(BongoCatApp *app, float elapsed_seconds) {
    if (!app || !app->live2d || elapsed_seconds <= 0.0f) return false;
    if (elapsed_seconds > 0.25f) elapsed_seconds = 0.25f;

    /* 口型同步：把麦克风音量映射到 ParamMouthOpenY */
    if (app->config.model.lip_sync_enabled && app->microphone &&
        bongo_cat_microphone_active(app->microphone)) {
        float level = bongo_cat_microphone_level(app->microphone);
        float max_open = app->config.model.lip_sync_max_open;
        if (max_open < 0.0f) max_open = 0.0f;
        if (max_open > 1.0f) max_open = 1.0f;
        float mouth_open = level * max_open;
        /* 标准 Live2D 口型参数。模型没有这个参数时 set_parameter 静默返回 false。 */
        bongo_cat_live2d_set_parameter(app->live2d, "ParamMouthOpenY", mouth_open);
    }

    unsigned steps = 1;
    while (steps < 8 && elapsed_seconds / steps > 1.0f / 30.0f) steps++;
    float step = elapsed_seconds / steps;
    bool changed = false;
    for (unsigned i = 0; i < steps; ++i)
        changed = bongo_cat_live2d_update(app->live2d, step) || changed;
    if (changed) app->dirty = true;
    return changed;
}
