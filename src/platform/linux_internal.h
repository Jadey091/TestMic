#ifndef BONGO_CAT_LINUX_INTERNAL_H
#define BONGO_CAT_LINUX_INTERNAL_H

#include "bongo_cat/platform.h"

bool bongo_cat_linux_x11_start(BongoCatPlatform *platform, BongoCatError *error);
void bongo_cat_linux_x11_stop(BongoCatPlatform *platform);
bool bongo_cat_linux_x11_supported(const BongoCatPlatform *platform);
void bongo_cat_linux_x11_click_through(BongoCatPlatform *platform, bool enabled);
void bongo_cat_linux_x11_configure_capture_window(BongoCatPlatform *platform);
void bongo_cat_linux_x11_begin_drag(BongoCatPlatform *platform);
BongoCatMenuAction bongo_cat_linux_context_menu(BongoCatPlatform *platform,
    const BongoCatMenuLabels *labels);

#endif
