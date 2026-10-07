#include "config_internal.h"
#include "bongo_cat/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

yyjson_doc *bongo_cat_config_read_document(const char *path,
    const char *format, BongoCatError *error) {
    yyjson_read_err json_error = {0};
    FILE *file = bongo_cat_file_open(path, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file, 0, NULL, &json_error) : NULL;
    if (file) fclose(file);
    if (!document) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Invalid configuration JSON: %s",
            json_error.msg ? json_error.msg : "cannot open file");
        return NULL;
    }
    yyjson_val *root = yyjson_doc_get_root(document);
    yyjson_val *format_value = yyjson_obj_get(root, "format");
    yyjson_val *version = yyjson_obj_get(root, "version");
    if (!yyjson_is_obj(root) || !yyjson_is_str(format_value) ||
        strcmp(yyjson_get_str(format_value), format) != 0 ||
        !yyjson_is_int(version) ||
        yyjson_get_sint(version) != BONGO_CAT_CONFIG_VERSION) {
        yyjson_doc_free(document);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Unsupported configuration format; expected %s version %d",
            format, BONGO_CAT_CONFIG_VERSION);
        return NULL;
    }
    return document;
}

static bool sync_file(const char *path) {
#ifdef _WIN32
    wchar_t *wide = bongo_cat_windows_wide(path);
    HANDLE file = wide ? CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;
    free(wide);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool result = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    return result;
#else
    int file = open(path, O_RDONLY);
    if (file < 0) return false;
    bool result = fsync(file) == 0;
    close(file);
    return result;
#endif
}

#ifndef _WIN32
static bool sync_parent(const char *path) {
    char directory[BONGO_CAT_PATH_CAP];
    int length = snprintf(directory, sizeof(directory), "%s", path);
    if (length < 0 || (size_t)length >= sizeof(directory)) return false;
    char *slash = strrchr(directory, '/');
    if (!slash) snprintf(directory, sizeof(directory), ".");
    else if (slash == directory) slash[1] = '\0';
    else *slash = '\0';
    int handle = open(directory, O_RDONLY);
    if (handle < 0) return false;
    bool result = fsync(handle) == 0;
    close(handle);
    return result;
}
#endif

BongoCatResult bongo_cat_config_write_document(const char *path,
    yyjson_mut_doc *document, const char *description, BongoCatError *error) {
    char temporary[BONGO_CAT_PATH_CAP + 8];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return BONGO_CAT_ERROR_ARGUMENT;
    yyjson_write_err json_error = {0};
    FILE *file = bongo_cat_file_open(temporary, "wb");
    bool written = file && yyjson_mut_write_fp(file, document,
        YYJSON_WRITE_PRETTY, NULL, &json_error);
    if (file && fclose(file) != 0) written = false;
    if (!written) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot write %s: %s",
            description, json_error.msg ? json_error.msg : "cannot open file");
        return BONGO_CAT_ERROR_IO;
    }
    if (!sync_file(temporary)) {
        bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot flush %s", description);
        return BONGO_CAT_ERROR_IO;
    }
    if (!bongo_cat_file_replace(temporary, path, true)) {
        bongo_cat_file_remove(temporary);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot replace %s", description);
        return BONGO_CAT_ERROR_IO;
    }
#ifndef _WIN32
    if (!sync_parent(path)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot flush configuration directory");
        return BONGO_CAT_ERROR_IO;
    }
#endif
    return BONGO_CAT_OK;
}
