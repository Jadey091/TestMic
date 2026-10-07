#include "config_internal.h"
#include "bongo_cat/path.h"

#include <stdio.h>

#define SESSION_FORMAT "bongo-cat/session"

static bool get_bool(yyjson_val *obj, const char *key, bool fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}
static int get_int(yyjson_val *obj, const char *key, int fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_num(value) ? (int)yyjson_get_sint(value) : fallback;
}
static float get_float(yyjson_val *obj, const char *key, float fallback) {
    yyjson_val *value = yyjson_obj_get(obj, key);
    return yyjson_is_num(value) ? (float)yyjson_get_num(value) : fallback;
}

BongoCatResult bongo_cat_session_load(const char *path,
    BongoCatConfig *config, BongoCatError *error) {
    if (!path || !config) return BONGO_CAT_ERROR_ARGUMENT;
    if (!bongo_cat_path_is_file(path)) return BONGO_CAT_OK;
    yyjson_doc *document = bongo_cat_config_read_document(path, SESSION_FORMAT, error);
    if (!document) return BONGO_CAT_ERROR_FORMAT;
    BongoCatConfig loaded = *config;
    yyjson_val *root = yyjson_doc_get_root(document);
    yyjson_val *window = yyjson_obj_get(root, "window");
    if (yyjson_is_obj(window)) {
        loaded.window.visible = get_bool(window, "visible", loaded.window.visible);
        loaded.window.scale_percent = get_float(window, "scale", loaded.window.scale_percent);
        loaded.window.opacity_percent = get_float(window, "opacity", loaded.window.opacity_percent);
        loaded.window.x = get_int(window, "x", loaded.window.x);
        loaded.window.y = get_int(window, "y", loaded.window.y);
        loaded.window.width = get_int(window, "width", loaded.window.width);
        loaded.window.height = get_int(window, "height", loaded.window.height);
    }
    yyjson_val *model = yyjson_obj_get(root, "currentModel");
    if (yyjson_is_str(model)) snprintf(loaded.current_model,
        sizeof(loaded.current_model), "%s", yyjson_get_str(model));
    yyjson_doc_free(document);
    bongo_cat_config_validate(&loaded);
    *config = loaded;
    return BONGO_CAT_OK;
}

BongoCatResult bongo_cat_session_save(const char *path,
    const BongoCatConfig *config, BongoCatError *error) {
    if (!path || !config) return BONGO_CAT_ERROR_ARGUMENT;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) return BONGO_CAT_ERROR_MEMORY;
    yyjson_mut_val *root = yyjson_mut_obj(doc); yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_strcpy(doc, root, "format", SESSION_FORMAT);
    yyjson_mut_obj_add_int(doc, root, "version", BONGO_CAT_CONFIG_VERSION);
    yyjson_mut_val *window = yyjson_mut_obj_add_obj(doc, root, "window");
    yyjson_mut_obj_add_bool(doc, window, "visible", config->window.visible);
    yyjson_mut_obj_add_real(doc, window, "scale", config->window.scale_percent);
    yyjson_mut_obj_add_real(doc, window, "opacity", config->window.opacity_percent);
    yyjson_mut_obj_add_int(doc, window, "x", config->window.x);
    yyjson_mut_obj_add_int(doc, window, "y", config->window.y);
    yyjson_mut_obj_add_int(doc, window, "width", config->window.width);
    yyjson_mut_obj_add_int(doc, window, "height", config->window.height);
    yyjson_mut_obj_add_strcpy(doc, root, "currentModel", config->current_model);
    BongoCatResult result = bongo_cat_config_write_document(path, doc,
        "session file", error);
    yyjson_mut_doc_free(doc);
    return result;
}
