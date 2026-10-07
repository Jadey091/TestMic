#ifndef BONGO_CAT_PREFERENCES_MODEL_GLYPHS_H
#define BONGO_CAT_PREFERENCES_MODEL_GLYPHS_H

#include <stddef.h>
#include <stdint.h>

typedef struct BongoCatApp BongoCatApp;

void bongo_cat_preferences_model_glyphs(const BongoCatApp *app,
    uint32_t *ranges, size_t capacity);

#endif
