#include "bongo_cat/mouse.h"

#include <math.h>

void bongo_cat_mouse_target(BongoCatMouseTracking *tracking, double x, double y) {
    if (!tracking) return;
    tracking->target_x = x;
    tracking->target_y = y;
    if (!tracking->initialized) {
        tracking->current_x = x;
        tracking->current_y = y;
        tracking->initialized = true;
    }
    tracking->settled = false;
}

bool bongo_cat_mouse_step(BongoCatMouseTracking *tracking, float delta_seconds,
    double *x, double *y) {
    if (!tracking || !tracking->initialized || tracking->settled || !x || !y)
        return false;
    if (delta_seconds < 0.0f) delta_seconds = 0.0f;
    double alpha = 1.0 - pow(0.75, (double)delta_seconds * 60.0);
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    tracking->current_x += (tracking->target_x - tracking->current_x) * alpha;
    tracking->current_y += (tracking->target_y - tracking->current_y) * alpha;
    double dx = tracking->target_x - tracking->current_x;
    double dy = tracking->target_y - tracking->current_y;
    if (dx * dx + dy * dy < 0.25) {
        tracking->current_x = tracking->target_x;
        tracking->current_y = tracking->target_y;
        tracking->settled = true;
    }
    *x = tracking->current_x;
    *y = tracking->current_y;
    return true;
}

float bongo_cat_mouse_parameter_value(float minimum, float maximum,
    float x_ratio, float y_ratio, char axis, bool mirror) {
    float value;
    if (axis == 'Z') {
        float x = 1.0f - 2.0f * x_ratio;
        float y = 1.0f - 2.0f * y_ratio;
        value = x * y * minimum;
    } else {
        float ratio = axis == 'Y' ? y_ratio : x_ratio;
        value = maximum - ratio * (maximum - minimum);
    }
    return axis != 'Y' && mirror ? -value : value;
}
