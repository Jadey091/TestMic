#include "model_import.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

static bool child_path(char *output, size_t capacity, const char *root,
    const char *first, const char *second) {
    char intermediate[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(intermediate, sizeof(intermediate), root, first) &&
        bongo_cat_path_join(output, capacity, intermediate, second);
}

static bool numbered_asset(const char *directory, const char *group) {
    char path[BONGO_CAT_PATH_CAP];
    return child_path(path, sizeof(path), directory, group, "0.png") &&
        bongo_cat_path_is_file(path);
}

static bool mver_shape(const char *image_root) {
    char mode[BONGO_CAT_PATH_CAP];
    if (bongo_cat_path_join(mode, sizeof(mode), image_root, "standard") &&
        numbered_asset(mode, "hand")) return true;
    const char *names[] = {"keyboard", "gamepad"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (bongo_cat_path_join(mode, sizeof(mode), image_root, names[i]) &&
            numbered_asset(mode, "lefthand") && numbered_asset(mode, "righthand")) return true;
    return false;
}

static bool parent_path(const char *path, char *parent, size_t capacity) {
    size_t length = path ? strlen(path) : 0;
    while (length && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    while (length && path[length - 1] != '/' && path[length - 1] != '\\') length--;
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    if (!length || length >= capacity) return false;
    memcpy(parent, path, length); parent[length] = '\0';
    return true;
}

static bool package_at(const char *source, char *config, char *image_root) {
    return bongo_cat_path_join(config, BONGO_CAT_PATH_CAP, source,
            "config.json") &&
        bongo_cat_path_join(image_root, BONGO_CAT_PATH_CAP, source, "img") &&
        bongo_cat_path_is_file(config) && bongo_cat_path_is_dir(image_root) &&
        mver_shape(image_root);
}

static bool find_package(const char *source, char *package, size_t capacity,
    char *config, char *image_root) {
    snprintf(package, capacity, "%s", source);
    for (int depth = 0; depth < 4; ++depth) {
        if (package_at(package, config, image_root)) return true;
        char parent[BONGO_CAT_PATH_CAP];
        if (!parent_path(package, parent, sizeof(parent))) break;
        snprintf(package, capacity, "%s", parent);
    }
    return false;
}

static bool mode_config_valid(yyjson_val *mode, BongoCatModelMode value,
    const char *directory) {
    if (!yyjson_is_obj(mode)) return false;
    if (value == BONGO_CAT_MODE_STANDARD)
        return yyjson_is_arr(yyjson_obj_get(mode, "hand")) &&
            numbered_asset(directory, "hand");
    return yyjson_is_arr(yyjson_obj_get(mode, "lefthand")) &&
        yyjson_is_arr(yyjson_obj_get(mode, "righthand")) &&
        numbered_asset(directory, "lefthand") && numbered_asset(directory, "righthand");
}

static bool model_at(const char *directory, char *setting, size_t capacity) {
    return bongo_cat_path_find_unique_suffix(directory, ".model3.json",
        setting, capacity) == 1 &&
        bongo_cat_import_manifest_valid(directory, setting, NULL);
}

static bool find_mode_model(const char *package, const char *mode_root, char *directory,
    size_t directory_capacity, char *setting, size_t setting_capacity) {
    if (bongo_cat_path_join(directory, directory_capacity, mode_root, "cat_model") &&
        bongo_cat_path_is_dir(directory) && model_at(directory, setting, setting_capacity))
        return true;
    snprintf(directory, directory_capacity, "%s", mode_root);
    if (model_at(directory, setting, setting_capacity)) return true;
    char resources[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(resources, sizeof(resources), package, "Resources") &&
        bongo_cat_path_join(directory, directory_capacity, resources, "cat") &&
        bongo_cat_path_is_dir(directory) && model_at(directory, setting, setting_capacity);
}

static bool add_mode(BongoCatImportDiscovery *discovery, const char *source,
    const char *config, const char *image_root, yyjson_val *root,
    BongoCatModelMode mode, BongoCatError *error) {
    const char *name = bongo_cat_mode_name(mode);
    char mode_root[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(mode_root, sizeof(mode_root), image_root, name)) return false;
    yyjson_val *mode_config = yyjson_obj_get(root, name);
    if (!bongo_cat_path_is_dir(mode_root) || !yyjson_is_obj(mode_config)) return true;
    if (!mode_config_valid(mode_config, mode, mode_root)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Mver mode has invalid input mappings or numbered assets: %s", mode_root);
        return false;
    }
    if (discovery->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP) return false;
    BongoCatImportCandidate *candidate = &discovery->candidates[discovery->count];
    if (!find_mode_model(source, mode_root, candidate->directory, sizeof(candidate->directory),
        candidate->setting, sizeof(candidate->setting))) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Mver mode contains no valid Live2D model: %s", mode_root);
        return false;
    }
    snprintf(candidate->assets, sizeof(candidate->assets), "%s", mode_root);
    snprintf(candidate->package_root, sizeof(candidate->package_root), "%s", source);
    snprintf(candidate->config, sizeof(candidate->config), "%s", config);
    candidate->mode = mode;
    candidate->format = BONGO_CAT_IMPORT_MVER;
    yyjson_val *input_mode = yyjson_obj_get(mode_config, "input_mode");
    candidate->gamepad_buttons = mode == BONGO_CAT_MODE_GAMEPAD &&
        ((!yyjson_is_int(input_mode) && !yyjson_is_uint(input_mode)) ||
        yyjson_get_int(input_mode) != 0);
    discovery->count++;
    return true;
}

static int discover_package(const char *package, const char *config,
    const char *image_root, BongoCatImportDiscovery *discovery,
    BongoCatError *error) {
    FILE *file = bongo_cat_file_open(config, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file,
        YYJSON_READ_JSON5 | YYJSON_READ_ALLOW_INVALID_UNICODE, NULL, NULL) : NULL;
    if (file) fclose(file);
    yyjson_val *root = document ? yyjson_doc_get_root(document) : NULL;
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(document);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Cannot parse Mver configuration: %s", config);
        return -1;
    }
    bool ok = add_mode(discovery, package, config, image_root, root,
        BONGO_CAT_MODE_STANDARD, error) &&
        add_mode(discovery, package, config, image_root, root,
            BONGO_CAT_MODE_KEYBOARD, error) &&
        add_mode(discovery, package, config, image_root, root,
            BONGO_CAT_MODE_GAMEPAD, error);
    yyjson_doc_free(document);
    if (!ok) return -1;
    if (!discovery->count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Mver package contains no supported model modes: %s", package);
        return -1;
    }
    return 1;
}

int bongo_cat_import_mver_discover_exact(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error) {
    char config[BONGO_CAT_PATH_CAP], image_root[BONGO_CAT_PATH_CAP];
    if (!package_at(source, config, image_root)) return 0;
    return discover_package(source, config, image_root, discovery, error);
}

int bongo_cat_import_mver_discover(const char *source,
    BongoCatImportDiscovery *discovery, BongoCatError *error) {
    char package[BONGO_CAT_PATH_CAP], config[BONGO_CAT_PATH_CAP];
    char image_root[BONGO_CAT_PATH_CAP];
    if (!find_package(source, package, sizeof(package), config, image_root)) return 0;
    return discover_package(package, config, image_root, discovery, error);
}
