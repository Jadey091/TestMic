#ifndef BONGO_CAT_COMMON_H
#define BONGO_CAT_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BONGO_CAT_NAME "BongoCat"
#define BONGO_CAT_PET_WINDOW_TITLE "BongoCat - Pet"
#define BONGO_CAT_SETTINGS_WINDOW_TITLE "BongoCat - Settings"
#define BONGO_CAT_SLUG "bongo-cat"
#define BONGO_CAT_EXECUTABLE "BongoCat"
#define BONGO_CAT_APP_ID "com.bongocat.desktop"
#define BONGO_CAT_NAME_W L"BongoCat"
#define BONGO_CAT_PET_WINDOW_TITLE_W L"BongoCat - Pet"
#define BONGO_CAT_VERSION "0.1.0"
#define BONGO_CAT_PATH_CAP 1024
#define BONGO_CAT_ID_CAP 128
#define BONGO_CAT_SHORTCUT_CAP 128
#define BONGO_CAT_MODEL_CAP 128
#define BONGO_CAT_BEHAVIOR_CAP 128
#define BONGO_CAT_AUTO_RELEASE_CAP 64

typedef enum BongoCatResult {
    BONGO_CAT_OK = 0,
    BONGO_CAT_ERROR_ARGUMENT,
    BONGO_CAT_ERROR_IO,
    BONGO_CAT_ERROR_FORMAT,
    BONGO_CAT_ERROR_MEMORY,
    BONGO_CAT_ERROR_PLATFORM,
    BONGO_CAT_ERROR_CUBISM
} BongoCatResult;

typedef struct BongoCatError {
    BongoCatResult code;
    char message[256];
} BongoCatError;

#ifdef __cplusplus
extern "C" {
#endif

void bongo_cat_error_set(BongoCatError *error, BongoCatResult code, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
