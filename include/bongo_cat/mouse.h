#ifndef BONGO_CAT_MOUSE_H
#define BONGO_CAT_MOUSE_H

#include <stdbool.h>

typedef struct BongoCatMouseTracking {
    double target_x;
    double target_y;
    double current_x;
    double current_y;
    bool initialized;
    bool settled;
} BongoCatMouseTracking;

void bongo_cat_mouse_target(BongoCatMouseTracking *tracking, double x, double y);
bool bongo_cat_mouse_step(BongoCatMouseTracking *tracking, float delta_seconds,
    double *x, double *y);
float bongo_cat_mouse_parameter_value(float minimum, float maximum,
    float x_ratio, float y_ratio, char axis, bool mirror);

#endif
