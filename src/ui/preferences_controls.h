#ifndef BONGO_CAT_PREFERENCES_CONTROLS_H
#define BONGO_CAT_PREFERENCES_CONTROLS_H

#include <stdbool.h>
#include "nuklear_config.h"

bool bongo_cat_pref_control_float(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step,
    float default_value);
bool bongo_cat_pref_control_int(struct nk_context *context, const char *id,
    int minimum, int *value, int maximum, int step, int default_value);
bool bongo_cat_pref_control_slider(struct nk_context *context, const char *id,
    float minimum, float *value, float maximum, float step,
    float default_value);
bool bongo_cat_pref_control_toggle(struct nk_context *context,
    const char *id, bool *value);
int bongo_cat_pref_control_combo(struct nk_context *context, const char *id,
    const char *const *items, int count, int selected);
bool bongo_cat_pref_controls_animating(struct nk_context *context);
void bongo_cat_pref_controls_reset(struct nk_context *context);

#endif
