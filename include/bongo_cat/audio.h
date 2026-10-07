#ifndef BONGO_CAT_AUDIO_H
#define BONGO_CAT_AUDIO_H

#include "bongo_cat/common.h"

typedef struct BongoCatAudio BongoCatAudio;

BongoCatAudio *bongo_cat_audio_create(BongoCatError *error);
void bongo_cat_audio_destroy(BongoCatAudio *audio);
BongoCatResult bongo_cat_audio_play(BongoCatAudio *audio, const char *path, BongoCatError *error);
void bongo_cat_audio_stop(BongoCatAudio *audio);
void bongo_cat_audio_set_enabled(BongoCatAudio *audio, bool enabled);

#endif
