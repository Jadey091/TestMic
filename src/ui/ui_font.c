#include "ui_font.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static bool readable(const char *path) {
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

const char *bongo_cat_ui_system_font(char *path, size_t capacity, bool multilingual) {
#ifdef _WIN32
    const char *windows = SDL_getenv("WINDIR");
    if (!windows) windows = SDL_getenv("SystemRoot");
    if (windows) {
        const char *multi[] = {"Fonts/msyh.ttc", "Fonts/msyhl.ttc"};
        const char *latin[] = {"Fonts/segoeui.ttf", "Fonts/msyhl.ttc"};
        const char **candidates = multilingual ? multi : latin;
        for (size_t i = 0; i < 2; ++i) {
            bongo_cat_path_join(path, capacity, windows, candidates[i]);
            if (readable(path)) return path;
        }
    }
#elif defined(__APPLE__)
    const char *candidates[] = {multilingual ? "/System/Library/Fonts/PingFang.ttc" :
        "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc"};
#else
    const char *candidates[] = {multilingual ?
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" :
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
#endif
#ifndef _WIN32
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!readable(candidates[i])) continue;
        snprintf(path, capacity, "%s", candidates[i]);
        return path;
    }
#endif
    return NULL;
}

const char *bongo_cat_ui_system_heading_font(char *path, size_t capacity,
    bool multilingual) {
#ifdef _WIN32
    const char *windows = SDL_getenv("WINDIR");
    if (!windows) windows = SDL_getenv("SystemRoot");
    if (windows) {
        const char *multi[] = {"Fonts/msyhbd.ttc", "Fonts/msyhl.ttc"};
        const char *latin[] = {"Fonts/seguisb.ttf", "Fonts/segoeui.ttf"};
        const char **candidates = multilingual ? multi : latin;
        for (size_t i = 0; i < 2; ++i) {
            bongo_cat_path_join(path, capacity, windows, candidates[i]);
            if (readable(path)) return path;
        }
    }
#elif defined(__APPLE__)
    const char *candidates[] = {multilingual ? "/System/Library/Fonts/PingFang.ttc" :
        "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc"};
#else
    const char *candidates[] = {multilingual ?
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc" :
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"};
#endif
#ifndef _WIN32
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!readable(candidates[i])) continue;
        snprintf(path, capacity, "%s", candidates[i]);
        return path;
    }
#endif
    return NULL;
}

const char *bongo_cat_ui_system_korean_font(char *path, size_t capacity) {
#ifdef _WIN32
    const char *windows = SDL_getenv("WINDIR");
    if (!windows) windows = SDL_getenv("SystemRoot");
    if (windows) {
        const char *candidates[] = {"Fonts/malgun.ttf", "Fonts/malgunsl.ttf"};
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
            bongo_cat_path_join(path, capacity, windows, candidates[i]);
            if (readable(path)) return path;
        }
    }
#elif defined(__APPLE__)
    const char *candidates[] = {
        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
        "/System/Library/Fonts/Supplemental/AppleGothic.ttf"};
#else
    const char *candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansKR-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"};
#endif
#ifndef _WIN32
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!readable(candidates[i])) continue;
        snprintf(path, capacity, "%s", candidates[i]);
        return path;
    }
#endif
    return bongo_cat_ui_system_font(path, capacity, true);
}

const char *bongo_cat_ui_system_korean_heading_font(char *path,
    size_t capacity) {
#ifdef _WIN32
    const char *windows = SDL_getenv("WINDIR");
    if (!windows) windows = SDL_getenv("SystemRoot");
    if (windows) {
        const char *candidates[] = {"Fonts/malgunbd.ttf", "Fonts/malgun.ttf"};
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
            bongo_cat_path_join(path, capacity, windows, candidates[i]);
            if (readable(path)) return path;
        }
    }
#elif defined(__APPLE__)
    const char *candidates[] = {
        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
        "/System/Library/Fonts/Supplemental/AppleGothic.ttf"};
#else
    const char *candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansKR-Bold.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"};
#endif
#ifndef _WIN32
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!readable(candidates[i])) continue;
        snprintf(path, capacity, "%s", candidates[i]);
        return path;
    }
#endif
    return bongo_cat_ui_system_heading_font(path, capacity, true);
}
