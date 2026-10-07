#include "bongo_cat/file.h"

#ifdef _WIN32
#include "windows_utf8.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static bool wide_mode(const char *mode, wchar_t output[16]) {
    if (!mode || strlen(mode) >= 16) return false;
    size_t index = 0;
    for (; mode[index]; ++index) output[index] = (wchar_t)(unsigned char)mode[index];
    output[index] = L'\0'; return true;
}

FILE *bongo_cat_file_open(const char *path, const char *mode) {
    wchar_t mode_wide[16];
    wchar_t *path_wide = bongo_cat_windows_wide(path);
    FILE *file = path_wide && wide_mode(mode, mode_wide)
        ? _wfopen(path_wide, mode_wide) : NULL;
    free(path_wide); return file;
}

bool bongo_cat_file_remove(const char *path) {
    wchar_t *wide = bongo_cat_windows_wide(path);
    bool removed = wide && _wremove(wide) == 0;
    free(wide); return removed;
}

bool bongo_cat_file_replace(const char *source, const char *target, bool durable) {
    wchar_t *source_wide = bongo_cat_windows_wide(source);
    wchar_t *target_wide = bongo_cat_windows_wide(target);
    DWORD flags = MOVEFILE_REPLACE_EXISTING | (durable ? MOVEFILE_WRITE_THROUGH : 0);
    bool replaced = source_wide && target_wide &&
        MoveFileExW(source_wide, target_wide, flags) != FALSE;
    free(source_wide); free(target_wide); return replaced;
}
#else
#include <stdio.h>

FILE *bongo_cat_file_open(const char *path, const char *mode) { return fopen(path, mode); }
bool bongo_cat_file_remove(const char *path) { return remove(path) == 0; }
bool bongo_cat_file_replace(const char *source, const char *target, bool durable) {
    (void)durable; return rename(source, target) == 0;
}
#endif
