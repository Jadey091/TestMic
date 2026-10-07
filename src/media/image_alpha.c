#include "bongo_cat/image.h"

#include <string.h>

void bongo_cat_image_make_alpha_mask(const BongoCatImage *image,
    BongoCatImageAlphaMask *mask) {
    if (!mask) return;
    memset(mask, 0, sizeof(*mask));
    if (!image || !image->pixels || image->width <= 0 || image->height <= 0) return;
    mask->width = image->width < BONGO_CAT_ALPHA_MASK_SIZE ?
        image->width : BONGO_CAT_ALPHA_MASK_SIZE;
    mask->height = image->height < BONGO_CAT_ALPHA_MASK_SIZE ?
        image->height : BONGO_CAT_ALPHA_MASK_SIZE;
    for (int y = 0; y < image->height; ++y) {
        int target_y = y * mask->height / image->height;
        for (int x = 0; x < image->width; ++x) {
            int target_x = x * mask->width / image->width;
            unsigned char alpha = image->pixels[
                ((size_t)y * image->width + x) * 4 + 3];
            unsigned char *target = &mask->pixels[
                (size_t)target_y * mask->width + target_x];
            if (alpha > *target) *target = alpha;
        }
    }
}
