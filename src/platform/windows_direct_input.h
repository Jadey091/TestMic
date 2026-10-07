#ifndef BONGO_CAT_WINDOWS_DIRECT_INPUT_H
#define BONGO_CAT_WINDOWS_DIRECT_INPUT_H

#include "bongo_cat/platform.h"

bool bongo_cat_windows_direct_input_create(BongoCatPlatform *platform,
    void *window);
void bongo_cat_windows_direct_input_destroy(BongoCatPlatform *platform);
void bongo_cat_windows_direct_input_reset(BongoCatPlatform *platform);

#endif
