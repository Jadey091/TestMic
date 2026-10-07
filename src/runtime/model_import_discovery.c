#include "model_import.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb_image.h>
#include <yyjson.h>

static bool safe_reference(const char *value) {
    if (!value || !value[0] || value[0] == '/' || value[0] == '\\' ||
        strchr(value, ':')) return false;
    const char *part = value;
    while (*part) {
        while (*part == '/' || *part == '\\') part++;
        if (part[0] == '.' && part[1] == '.' &&
            (!part[2] || part[2] == '/' || part[2] == '\\')) return false;
        part = strpbrk(part, "/\\");
        if (!part) break;
    }
    return true;
}

static bool referenced_file(const char *root, const char *relative) {
    char path[BONGO_CAT_PATH_CAP];
    return safe_reference(relative) &&
        bongo_cat_path_join(path, sizeof(path), root, relative) && bongo_cat_path_is_file(path);
}

static bool referenced_texture(const char *root, const char *relative) {
    char path[BONGO_CAT_PATH_CAP];
    if (!safe_reference(relative) ||
        !bongo_cat_path_join(path, sizeof(path), root, relative)) return false;
    FILE *file = bongo_cat_file_open(path, "rb");
    int width = 0, height = 0, channels = 0;
    bool valid = file && stbi_info_from_file(file, &width, &height, &channels) &&
        width > 0 && height > 0 && channels > 0;
    if (file) fclose(file);
    return valid;
}

static bool optional_reference(const char *root, yyjson_val *refs,
    const char *name) {
    yyjson_val *value = yyjson_obj_get(refs, name);
    if (!value) return true;
    const char *relative = yyjson_get_str(value);
    return relative && referenced_file(root, relative);
}

static bool behavior_references(const char *root, yyjson_val *refs) {
    yyjson_val *expressions = yyjson_obj_get(refs, "Expressions");
    if (expressions && !yyjson_is_arr(expressions)) return false;
    size_t index, count; yyjson_val *item;
    yyjson_arr_foreach(expressions, index, count, item)
        if (!referenced_file(root, yyjson_get_str(yyjson_obj_get(item, "File"))))
            return false;
    yyjson_val *motions = yyjson_obj_get(refs, "Motions");
    if (motions && !yyjson_is_obj(motions)) return false;
    size_t group_index, group_count; yyjson_val *key, *group;
    yyjson_obj_foreach(motions, group_index, group_count, key, group) {
        if (!yyjson_is_arr(group)) return false;
        yyjson_arr_foreach(group, index, count, item) {
            if (!referenced_file(root, yyjson_get_str(yyjson_obj_get(item, "File"))))
                return false;
            const char *sound = yyjson_get_str(yyjson_obj_get(item, "Sound"));
            if (sound && !safe_reference(sound)) return false;
        }
    }
    return true;
}

bool bongo_cat_import_manifest_valid(const char *root, const char *setting,
    BongoCatError *error) {
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), root, setting)) return false;
    FILE *file = bongo_cat_file_open(path, "rb");
    yyjson_doc *document = file ? yyjson_read_fp(file, 0, NULL, NULL) : NULL;
    if (file) fclose(file);
    yyjson_val *manifest = document ? yyjson_doc_get_root(document) : NULL;
    yyjson_val *refs = yyjson_is_obj(manifest)
        ? yyjson_obj_get(manifest, "FileReferences") : NULL;
    const char *moc = yyjson_get_str(yyjson_obj_get(refs, "Moc"));
    yyjson_val *textures = yyjson_obj_get(refs, "Textures");
    bool valid = yyjson_get_int(yyjson_obj_get(manifest, "Version")) == 3 &&
        yyjson_is_obj(refs) && referenced_file(root, moc) && yyjson_is_arr(textures) &&
        yyjson_arr_size(textures) > 0;
    size_t index, maximum; yyjson_val *texture;
    yyjson_arr_foreach(textures, index, maximum, texture)
        valid = valid && referenced_texture(root, yyjson_get_str(texture));
    valid = valid && optional_reference(root, refs, "Physics") &&
        optional_reference(root, refs, "Pose") &&
        optional_reference(root, refs, "DisplayInfo") && behavior_references(root, refs);
    yyjson_doc_free(document);
    if (!valid && error) bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
        "Model manifest or referenced assets are invalid: %s", path);
    return valid;
}

static bool path_parent(const char *path, char *parent, size_t capacity) {
    size_t length = path ? strlen(path) : 0;
    while (length && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    while (length && path[length - 1] != '/' && path[length - 1] != '\\') length--;
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) length--;
    if (!length || length >= capacity) return false;
    memcpy(parent, path, length);
    parent[length] = '\0';
    return true;
}

static BongoCatModelMode import_mode(const char *path) {
    const char *cursor = path;
    BongoCatModelMode mode = BONGO_CAT_MODE_STANDARD;
    while (cursor && *cursor) {
        while (*cursor == '/' || *cursor == '\\') cursor++;
        const char *end = strpbrk(cursor, "/\\");
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == 8 && SDL_strncasecmp(cursor, "keyboard", length) == 0)
            mode = BONGO_CAT_MODE_KEYBOARD;
        else if (length == 7 && SDL_strncasecmp(cursor, "gamepad", length) == 0)
            mode = BONGO_CAT_MODE_GAMEPAD;
        else if (length == 8 && SDL_strncasecmp(cursor, "standard", length) == 0)
            mode = BONGO_CAT_MODE_STANDARD;
        if (!end) break;
        cursor = end + 1;
    }
    return mode;
}

static bool has_preview_assets(const char *directory) {
    const char *names[] = {"resources", "cover.png", "cat.png", "bg.png",
        "mousebg.png", "tabletbg.png"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_path_join(path, sizeof(path), directory, names[i])) continue;
        if (bongo_cat_path_is_file(path) || bongo_cat_path_is_dir(path)) return true;
    }
    return false;
}

static bool has_cover_asset(const char *directory) {
    const char *names[] = {"resources/cover.png", "cover.png", "cat.png", "bg.png"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[BONGO_CAT_PATH_CAP];
        if (bongo_cat_path_join(path, sizeof(path), directory, names[i]) &&
            bongo_cat_path_is_file(path)) return true;
    }
    return false;
}

static bool has_background_asset(const char *directory) {
    const char *names[] = {"resources/background.png", "background.png",
        "bg.png", "mousebg.png", "tabletbg.png"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[BONGO_CAT_PATH_CAP];
        if (bongo_cat_path_join(path, sizeof(path), directory, names[i]) &&
            bongo_cat_path_is_file(path)) return true;
    }
    return false;
}

static bool add_candidate(BongoCatImportDiscovery *discovery, const char *directory,
    const char *setting) {
    if (discovery->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP) return false;
    for (size_t i = 0; i < discovery->count; ++i)
        if (strcmp(discovery->candidates[i].directory, directory) == 0) {
            if (strcmp(discovery->candidates[i].setting, setting) == 0) return true;
            discovery->ambiguous = true;
            return false;
        }
    BongoCatImportCandidate *candidate = &discovery->candidates[discovery->count++];
    snprintf(candidate->directory, sizeof(candidate->directory), "%s", directory);
    snprintf(candidate->setting, sizeof(candidate->setting), "%s", setting);
    snprintf(candidate->assets, sizeof(candidate->assets), "%s", directory);
    snprintf(candidate->package_root, sizeof(candidate->package_root), "%s", directory);
    candidate->format = BONGO_CAT_IMPORT_TAURI;
    char parent[BONGO_CAT_PATH_CAP];
    if ((!has_cover_asset(directory) || !has_background_asset(directory)) &&
        path_parent(directory, parent, sizeof(parent)) && has_preview_assets(parent))
        snprintf(candidate->assets, sizeof(candidate->assets), "%s", parent);
    candidate->mode = import_mode(candidate->assets);
    return true;
}

static bool suffix(const char *name, const char *ending) {
    size_t a = name ? strlen(name) : 0, b = ending ? strlen(ending) : 0;
    return a >= b && strcmp(name + a - b, ending) == 0;
}

static BongoCatPathVisit discover_item(void *userdata,
    const char *dirname, const char *name) {
    BongoCatImportDiscovery *discovery = userdata;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), dirname, name))
        return BONGO_CAT_PATH_FAILURE;
    if (bongo_cat_path_is_file(path) && suffix(name, ".model3.json")) {
        if (bongo_cat_import_manifest_valid(dirname, name, NULL) &&
            !add_candidate(discovery, dirname, name)) return BONGO_CAT_PATH_FAILURE;
        return BONGO_CAT_PATH_CONTINUE;
    }
    if (!bongo_cat_path_is_dir(path) || discovery->depth >= 8 || name[0] == '.')
        return BONGO_CAT_PATH_CONTINUE;
    discovery->depth++;
    bool ok = bongo_cat_path_enumerate(path, discover_item, discovery);
    discovery->depth--;
    return ok ? BONGO_CAT_PATH_CONTINUE : BONGO_CAT_PATH_FAILURE;
}

static int rank(const BongoCatImportCandidate *candidate) {
    return candidate->mode == BONGO_CAT_MODE_STANDARD ? 0 :
        candidate->mode == BONGO_CAT_MODE_KEYBOARD ? 1 : 2;
}

static int compare_candidates(const void *left, const void *right) {
    const BongoCatImportCandidate *a = left, *b = right;
    int difference = rank(a) - rank(b);
    return difference ? difference : strcmp(a->directory, b->directory);
}

typedef struct ContainerDiscovery {
    BongoCatImportDiscovery *output;
} ContainerDiscovery;

static BongoCatResult collect_container(void *userdata, const char *source,
    BongoCatImportDiscovery *found, BongoCatError *error) {
    (void)source;
    ContainerDiscovery *container = userdata;
    for (size_t i = 0; i < found->count; ++i) {
        if (container->output->count >= BONGO_CAT_IMPORT_CANDIDATE_CAP) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
                "Selected folder contains too many model variants");
            return BONGO_CAT_ERROR_FORMAT;
        }
        container->output->candidates[container->output->count++] =
            found->candidates[i];
    }
    return BONGO_CAT_OK;
}

bool bongo_cat_import_discover(const char *source, BongoCatImportDiscovery *discovery,
    BongoCatError *error) {
    memset(discovery, 0, sizeof(*discovery));
    int mver = bongo_cat_import_mver_discover(source, discovery, error);
    if (mver > 0) return true;
    if (mver < 0) return false;
    BongoCatError patch_error = {0};
    int patch = bongo_cat_import_mver_patch_discover(source, discovery, &patch_error);
    if (patch > 0) return true;
    memset(discovery, 0, sizeof(*discovery));
    ContainerDiscovery container = {discovery};
    BongoCatResult container_result = bongo_cat_import_portable_scan(source,
        collect_container, &container, error);
    if (container_result != BONGO_CAT_OK) return false;
    if (discovery->count) {
        qsort(discovery->candidates, discovery->count,
            sizeof(discovery->candidates[0]), compare_candidates);
        return true;
    }
    if (patch < 0) {
        if (error) *error = patch_error;
        return false;
    }
    if (error) *error = (BongoCatError){0};
    if (!bongo_cat_path_enumerate(source, discover_item, discovery)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            discovery->ambiguous ? "A model directory contains multiple model3 manifests" :
            "Cannot scan model directory or it contains too many models");
        return false;
    }
    if (!discovery->count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Selected directory contains no valid Live2D model3 JSON");
        return false;
    }
    for (size_t i = 0; i < discovery->count; ++i)
        if (discovery->candidates[i].format == BONGO_CAT_IMPORT_TAURI)
            snprintf(discovery->candidates[i].package_root,
                sizeof(discovery->candidates[i].package_root), "%s", source);
    qsort(discovery->candidates, discovery->count,
        sizeof(discovery->candidates[0]), compare_candidates);
    return true;
}
