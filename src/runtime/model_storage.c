#include "model_storage.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <string.h>

#define MODEL_REMOVE_DEPTH_CAP 32

typedef struct RemoveContext { BongoCatError *error; unsigned depth; } RemoveContext;
static bool remove_tree(const char *path, unsigned depth, BongoCatError *error);

static BongoCatPathVisit remove_item(void *userdata,
    const char *dirname, const char *name) {
    RemoveContext *context = userdata;
    char path[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(path, sizeof(path), dirname, name) &&
        remove_tree(path, context->depth + 1, context->error)
        ? BONGO_CAT_PATH_CONTINUE : BONGO_CAT_PATH_FAILURE;
}

static bool remove_tree(const char *path, unsigned depth, BongoCatError *error) {
    if (!path || !path[0]) return true;
    bool directory = bongo_cat_path_is_dir(path);
    if (!directory && !bongo_cat_path_is_file(path)) return true;
    if (depth > MODEL_REMOVE_DEPTH_CAP) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Model directory nesting exceeds %u levels", MODEL_REMOVE_DEPTH_CAP);
        return false;
    }
    RemoveContext context = {error, depth};
    if (directory &&
        !bongo_cat_path_enumerate(path, remove_item, &context)) return false;
    if (bongo_cat_path_remove(path)) return true;
    if (error) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "Cannot remove %s: %s", path, SDL_GetError());
    return false;
}

bool bongo_cat_model_remove_tree(const char *path, BongoCatError *error) {
    return remove_tree(path, 0, error);
}

static bool temporary_name(const char *name) {
    static const char prefix[] = ".import-", suffix[] = ".tmp";
    if (!name || strncmp(name, prefix, sizeof(prefix) - 1) != 0) return false;
    const char *cursor = name + sizeof(prefix) - 1;
    const char *dash = strchr(cursor, '-');
    if (!dash || dash == cursor) return false;
    for (const char *value = cursor; value < dash; ++value)
        if (!isxdigit((unsigned char)*value)) return false;
    cursor = dash + 1;
    const char *end = name + strlen(name) - (sizeof(suffix) - 1);
    if (end <= cursor || strcmp(end, suffix) != 0) return false;
    for (const char *value = cursor; value < end; ++value)
        if (!isdigit((unsigned char)*value)) return false;
    return true;
}

static BongoCatPathVisit cleanup_item(void *userdata,
    const char *dirname, const char *name) {
    BongoCatError *error = userdata;
    if (!temporary_name(name)) return BONGO_CAT_PATH_CONTINUE;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), dirname, name) ||
        !bongo_cat_path_is_dir(path)) return BONGO_CAT_PATH_CONTINUE;
    return bongo_cat_model_remove_tree(path, error)
        ? BONGO_CAT_PATH_CONTINUE : BONGO_CAT_PATH_FAILURE;
}

bool bongo_cat_model_cleanup_imports(const char *root, BongoCatError *error) {
    return root && bongo_cat_path_is_dir(root) &&
        bongo_cat_path_enumerate(root, cleanup_item, error);
}
