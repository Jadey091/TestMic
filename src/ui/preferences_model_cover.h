#ifndef BONGO_CAT_PREFERENCES_MODEL_COVER_H
#define BONGO_CAT_PREFERENCES_MODEL_COVER_H

#include "bongo_cat/model.h"

typedef struct BongoCatApp BongoCatApp;

typedef struct BongoCatModelCover {
    unsigned int texture;
    int width;
    int height;
} BongoCatModelCover;

void bongo_cat_preferences_model_covers_begin(void);
void bongo_cat_preferences_model_cover_cache_clear(BongoCatApp *app);
const BongoCatModelCover *bongo_cat_preferences_model_cover(
    BongoCatApp *app, const BongoCatModelEntry *entry,
    int pixel_width, int pixel_height);
bool bongo_cat_preferences_model_cover_capture(BongoCatApp *app,
    const BongoCatModelEntry *entry);
void bongo_cat_preferences_model_covers_prune(BongoCatApp *app);

#endif
