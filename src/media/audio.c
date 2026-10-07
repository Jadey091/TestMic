#include "bongo_cat/audio.h"

#include <miniaudio.h>
#include <stdlib.h>
#include <string.h>

struct BongoCatAudio {
    ma_engine engine;
    ma_sound sound;
    bool initialized;
    bool sound_ready;
    bool enabled;
};

BongoCatAudio *bongo_cat_audio_create(BongoCatError *error) {
    (void)error;
    BongoCatAudio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio->enabled = true;
    return audio;
}

void bongo_cat_audio_stop(BongoCatAudio *audio) {
    if (!audio || !audio->sound_ready) return;
    ma_sound_stop(&audio->sound);
    ma_sound_uninit(&audio->sound);
    audio->sound_ready = false;
}

BongoCatResult bongo_cat_audio_play(BongoCatAudio *audio, const char *path, BongoCatError *error) {
    if (!audio || !path) return BONGO_CAT_ERROR_ARGUMENT;
    if (!audio->enabled) return BONGO_CAT_OK;
    if (!audio->initialized) {
        ma_result initialized = ma_engine_init(NULL, &audio->engine);
        if (initialized != MA_SUCCESS) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "Audio initialization failed: %d", initialized);
            return BONGO_CAT_ERROR_PLATFORM;
        }
        audio->initialized = true;
    }
    bongo_cat_audio_stop(audio);
    ma_result result = ma_sound_init_from_file(&audio->engine, path,
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &audio->sound);
    if (result != MA_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot load motion sound: %s", path);
        return BONGO_CAT_ERROR_IO;
    }
    audio->sound_ready = true;
    result = ma_sound_start(&audio->sound);
    if (result != MA_SUCCESS) {
        bongo_cat_audio_stop(audio);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Cannot start motion sound: %s", path);
        return BONGO_CAT_ERROR_PLATFORM;
    }
    return BONGO_CAT_OK;
}

void bongo_cat_audio_set_enabled(BongoCatAudio *audio, bool enabled) {
    if (!audio) return;
    audio->enabled = enabled;
    if (!enabled) bongo_cat_audio_stop(audio);
}

void bongo_cat_audio_destroy(BongoCatAudio *audio) {
    if (!audio) return;
    bongo_cat_audio_stop(audio);
    if (audio->initialized) ma_engine_uninit(&audio->engine);
    free(audio);
}
