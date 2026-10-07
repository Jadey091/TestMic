#include "bongo_cat/file.h"
#include "bongo_cat/json.h"
#include "bongo_cat/model.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

#ifdef _WIN32
#include "windows_utf8.h"
#include <stdlib.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

void bongo_cat_models_init(BongoCatModelCatalog *catalog) {
    if (catalog) memset(catalog, 0, sizeof(*catalog));
}

static BongoCatModelMode infer_mode(const char *directory) {
    char path[BONGO_CAT_PATH_CAP], stored[16] = {0};
    bongo_cat_path_join(path, sizeof(path), directory, ".bongo-cat-mode");
    FILE *file = bongo_cat_file_open(path, "rb");
    if (file) {
        size_t length = fread(stored, 1, sizeof(stored) - 1, file);
        fclose(file);
        stored[length] = '\0';
        if (strcmp(stored, "gamepad") == 0) return BONGO_CAT_MODE_GAMEPAD;
        if (strcmp(stored, "keyboard") == 0) return BONGO_CAT_MODE_KEYBOARD;
        if (strcmp(stored, "standard") == 0) return BONGO_CAT_MODE_STANDARD;
    }
    const char *name = bongo_cat_path_name(directory);
    if (strcmp(name, "gamepad") == 0) return BONGO_CAT_MODE_GAMEPAD;
    if (strcmp(name, "keyboard") == 0) return BONGO_CAT_MODE_KEYBOARD;
    if (strcmp(name, "standard") == 0) return BONGO_CAT_MODE_STANDARD;
    bongo_cat_path_join(path, sizeof(path), directory, "resources/right-keys/East.png");
    if (bongo_cat_path_is_file(path)) return BONGO_CAT_MODE_GAMEPAD;
    bongo_cat_path_join(path, sizeof(path), directory, "resources/right-keys");
    return bongo_cat_path_is_dir(path) ? BONGO_CAT_MODE_KEYBOARD : BONGO_CAT_MODE_STANDARD;
}

static bool safe_relative(const char *value) {
    if (!value || !value[0] || value[0] == '/' || value[0] == '\\' || strchr(value, ':'))
        return false;
    const char *cursor = value;
    while (*cursor) {
        while (*cursor == '/' || *cursor == '\\') cursor++;
        if (cursor[0] == '.' && cursor[1] == '.' &&
            (!cursor[2] || cursor[2] == '/' || cursor[2] == '\\')) return false;
        cursor = strpbrk(cursor, "/\\");
        if (!cursor) break;
    }
    return true;
}

static BongoCatModelMode stored_mode(const char *value) {
    if (value && strcmp(value, "keyboard") == 0) return BONGO_CAT_MODE_KEYBOARD;
    if (value && strcmp(value, "gamepad") == 0) return BONGO_CAT_MODE_GAMEPAD;
    return BONGO_CAT_MODE_STANDARD;
}

static bool add_package(BongoCatModelCatalog *catalog, const char *directory,
    bool preset, bool *handled) {
    char descriptor[BONGO_CAT_PATH_CAP];
    *handled = bongo_cat_path_join(descriptor, sizeof(descriptor), directory,
        ".bongo-cat-package.json") && bongo_cat_path_is_file(descriptor);
    if (!*handled) return true;
    yyjson_doc *document = bongo_cat_json_read_file(descriptor, 0, NULL);
    yyjson_val *root = document ? yyjson_doc_get_root(document) : NULL;
    const char *model = yyjson_get_str(yyjson_obj_get(root, "directory"));
    const char *adapter = yyjson_get_str(yyjson_obj_get(root, "adapter"));
    const char *setting = yyjson_get_str(yyjson_obj_get(root, "setting"));
    bool valid = yyjson_is_obj(root) &&
        yyjson_get_int(yyjson_obj_get(root, "schemaVersion")) == 1 &&
        safe_relative(model) && safe_relative(adapter) && safe_relative(setting) &&
        catalog->count < BONGO_CAT_MODEL_CAP;
    BongoCatModelEntry *entry = valid ? &catalog->entries[catalog->count] : NULL;
    if (entry) valid = bongo_cat_path_join(entry->directory,
        sizeof(entry->directory), directory, model) &&
        bongo_cat_path_join(entry->adapter_directory,
            sizeof(entry->adapter_directory), directory, adapter);
    char setting_path[BONGO_CAT_PATH_CAP];
    if (entry) valid = bongo_cat_path_join(setting_path, sizeof(setting_path),
        entry->directory, setting) && bongo_cat_path_is_file(setting_path) &&
        bongo_cat_path_is_dir(entry->adapter_directory);
    if (valid) {
        snprintf(entry->id, sizeof(entry->id), "%s", bongo_cat_path_name(directory));
        snprintf(entry->storage_directory, sizeof(entry->storage_directory), "%s", directory);
        snprintf(entry->setting_file, sizeof(entry->setting_file), "%s", setting);
        entry->mode = stored_mode(yyjson_get_str(yyjson_obj_get(root, "mode")));
        entry->preset = preset;
        catalog->count++;
    }
    yyjson_doc_free(document);
    return valid;
}

static bool add_model(BongoCatModelCatalog *catalog, const char *directory, bool preset) {
    bool handled = false;
    bool package_ok = add_package(catalog, directory, preset, &handled);
    if (handled) return package_ok;
    if (!package_ok) return false;
    if (!preset) return true;
    if (catalog->count >= BONGO_CAT_MODEL_CAP) return false;
    char setting[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_find_suffix(directory, ".model3.json", setting, sizeof(setting))) return true;
    BongoCatModelEntry *entry = &catalog->entries[catalog->count++];
    snprintf(entry->id, sizeof(entry->id), "%s", bongo_cat_path_name(directory));
    snprintf(entry->directory, sizeof(entry->directory), "%s", directory);
    snprintf(entry->adapter_directory, sizeof(entry->adapter_directory), "%s", directory);
    snprintf(entry->storage_directory, sizeof(entry->storage_directory), "%s", directory);
    snprintf(entry->setting_file, sizeof(entry->setting_file), "%s", setting);
    entry->mode = infer_mode(directory);
    entry->preset = preset;
    return true;
}

#ifdef _WIN32
static BongoCatResult scan_windows(BongoCatModelCatalog *catalog, const char *root, bool preset,
    BongoCatError *error) {
    char pattern[BONGO_CAT_PATH_CAP];
    bongo_cat_path_join(pattern, sizeof(pattern), root, "*");
    wchar_t *wide = bongo_cat_windows_wide(pattern);
    WIN32_FIND_DATAW data = {0};
    HANDLE find = wide ? FindFirstFileW(wide, &data) : INVALID_HANDLE_VALUE;
    free(wide);
    if (find == INVALID_HANDLE_VALUE) return BONGO_CAT_OK;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        char filename[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_windows_utf8(data.cFileName, filename, sizeof(filename))) continue;
        char directory[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(directory, sizeof(directory), root, filename) ||
            !add_model(catalog, directory, preset)) {
            FindClose(find);
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Too many models or a path is too long");
            return BONGO_CAT_ERROR_FORMAT;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return BONGO_CAT_OK;
}
#else

static BongoCatResult scan_posix(BongoCatModelCatalog *catalog, const char *root, bool preset,
    BongoCatError *error) {
    DIR *handle = opendir(root);
    if (!handle) return BONGO_CAT_OK;
    struct dirent *item;
    while ((item = readdir(handle))) {
        if (item->d_name[0] == '.') continue;
        char directory[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(directory, sizeof(directory), root, item->d_name)) continue;
        if (!bongo_cat_path_is_dir(directory)) continue;
        if (!add_model(catalog, directory, preset)) {
            closedir(handle);
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT, "Too many models");
            return BONGO_CAT_ERROR_FORMAT;
        }
    }
    closedir(handle);
    return BONGO_CAT_OK;
}
#endif

BongoCatResult bongo_cat_models_scan(BongoCatModelCatalog *catalog, const char *root, bool preset,
    BongoCatError *error) {
    if (!catalog || !root) return BONGO_CAT_ERROR_ARGUMENT;
#ifdef _WIN32
    return scan_windows(catalog, root, preset, error);
#else
    return scan_posix(catalog, root, preset, error);
#endif
}

const BongoCatModelEntry *bongo_cat_models_find(const BongoCatModelCatalog *catalog, const char *id) {
    if (!catalog || !id) return NULL;
    for (size_t i = 0; i < catalog->count; ++i) {
        if (strcmp(catalog->entries[i].id, id) == 0) return &catalog->entries[i];
    }
    return NULL;
}
