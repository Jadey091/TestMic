#include "model_import.h"
#include "model_import_mver_internal.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/json.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

#define MVER_METADATA ".bongo-cat-mver.json"

static double number_or(yyjson_val *value, double fallback) {
    return yyjson_is_num(value) ? yyjson_get_num(value) : fallback;
}

static bool add_standard_pointer(yyjson_mut_doc *output, yyjson_mut_val *root,
    yyjson_val *config, const BongoCatImportCandidate *candidate) {
    if (candidate->mode != BONGO_CAT_MODE_STANDARD) return true;
    yyjson_val *decoration = yyjson_obj_get(config, "decoration");
    yyjson_val *standard = yyjson_obj_get(config, "standard");
    yyjson_val *mouse_value = yyjson_obj_get(standard, "mouse");
    bool mouse = yyjson_is_bool(mouse_value) && yyjson_get_bool(mouse_value);
    yyjson_val *live2d_value = yyjson_obj_get(standard, "l2d");
    bool live2d = !yyjson_is_bool(live2d_value) || yyjson_get_bool(live2d_value);
    size_t device_index = mouse ? 0 : 1;
    yyjson_val *offset_x = yyjson_obj_get(decoration, "offsetX");
    yyjson_val *offset_y = yyjson_obj_get(decoration, "offsetY");
    yyjson_val *scale = yyjson_obj_get(decoration, "scalar");
    yyjson_val *hand_offset = yyjson_obj_get(
        live2d ? standard : decoration, "hand_offset");
    yyjson_val *line = yyjson_obj_get(decoration, "armLineColor");
    yyjson_val *left_handed = yyjson_obj_get(decoration, "leftHanded");
    const char *device = mouse ? "resources/mver-pointer/mouse.png" :
        "resources/mver-pointer/tablet.png";
    const char *left = mouse ? "resources/mver-pointer/mouse_left.png" :
        "resources/mver-pointer/tablet_left.png";
    const char *right = mouse ? "resources/mver-pointer/mouse_right.png" :
        "resources/mver-pointer/tablet_right.png";
    const char *side = mouse ? "resources/mver-pointer/mouse_side.png" : "";
    yyjson_mut_val *pointer = yyjson_mut_obj_add_obj(output, root, "standardPointer");
    return pointer &&
        /* Mver draws the pointer layer after the Live2D model in both standard
           and Live2D-standard modes. Keep it enabled so the authored hand,
           device, and button overlays continue to follow the pointer. */
        yyjson_mut_obj_add_bool(output, pointer, "enabled", true) &&
        yyjson_mut_obj_add_bool(output, pointer, "mouse", mouse) &&
        yyjson_mut_obj_add_bool(output, pointer, "leftHanded",
            yyjson_is_bool(left_handed) && yyjson_get_bool(left_handed)) &&
        yyjson_mut_obj_add_str(output, pointer, "arm",
            "resources/mver-pointer/arm.png") &&
        yyjson_mut_obj_add_strcpy(output, pointer, "device", device) &&
        yyjson_mut_obj_add_strcpy(output, pointer, "left", left) &&
        yyjson_mut_obj_add_strcpy(output, pointer, "right", right) &&
        yyjson_mut_obj_add_strcpy(output, pointer, "side", side) &&
        yyjson_mut_obj_add_real(output, pointer, "offsetX",
            number_or(yyjson_arr_get(offset_x, device_index), 0.0)) &&
        yyjson_mut_obj_add_real(output, pointer, "offsetY",
            number_or(yyjson_arr_get(offset_y, device_index), 0.0)) &&
        yyjson_mut_obj_add_real(output, pointer, "scale",
            number_or(yyjson_arr_get(scale, device_index), 1.0)) &&
        yyjson_mut_obj_add_real(output, pointer, "handOffsetX",
            number_or(yyjson_arr_get(hand_offset, 0), 0.0)) &&
        yyjson_mut_obj_add_real(output, pointer, "handOffsetY",
            number_or(yyjson_arr_get(hand_offset, 1), 0.0)) &&
        yyjson_mut_obj_add_int(output, pointer, "lineRed",
            (int)number_or(yyjson_arr_get(line, 0), 0.0)) &&
        yyjson_mut_obj_add_int(output, pointer, "lineGreen",
            (int)number_or(yyjson_arr_get(line, 1), 0.0)) &&
        yyjson_mut_obj_add_int(output, pointer, "lineBlue",
            (int)number_or(yyjson_arr_get(line, 2), 0.0));
}

static bool add_render_profile(yyjson_mut_doc *output, yyjson_mut_val *root,
    yyjson_val *config, const BongoCatImportCandidate *candidate) {
    yyjson_val *decoration = yyjson_obj_get(config, "decoration");
    yyjson_val *workarea = yyjson_obj_get(config, "workarea");
    yyjson_val *window = yyjson_obj_get(decoration, "window_size");
    yyjson_val *offset = yyjson_obj_get(decoration, "l2d_offset");
    yyjson_val *top_left = yyjson_obj_get(workarea, "top_left");
    yyjson_val *right_bottom = yyjson_obj_get(workarea, "right_bottom");
    double scale = number_or(yyjson_obj_get(decoration, "l2d_correct"), 1.1);
    int width = (int)number_or(yyjson_arr_get(window, 0), 612.0);
    int height = (int)number_or(yyjson_arr_get(window, 1), 352.0);
    double offset_x = number_or(yyjson_arr_get(offset, 0), 0.0);
    double offset_y = number_or(yyjson_arr_get(offset, 1), 0.0);
    yyjson_val *mirror_value = yyjson_obj_get(decoration, "l2d_horizontal_flip");
    bool mirror = yyjson_is_bool(mirror_value) && yyjson_get_bool(mirror_value);
    yyjson_val *left_handed_value = yyjson_obj_get(decoration, "leftHanded");
    bool left_handed = yyjson_is_bool(left_handed_value) &&
        yyjson_get_bool(left_handed_value);
    yyjson_val *force_value = yyjson_obj_get(decoration, "mouse_force_move");
    bool force = yyjson_is_bool(force_value) && yyjson_get_bool(force_value);
    double mouse_speed = number_or(yyjson_obj_get(decoration, "mouse_speed"), 1.0);
    yyjson_val *custom_value = yyjson_obj_get(workarea, "workarea");
    bool custom = yyjson_is_bool(custom_value) && yyjson_get_bool(custom_value);
    int left = (int)number_or(yyjson_arr_get(top_left, 0), 0.0);
    int top = (int)number_or(yyjson_arr_get(top_left, 1), 0.0);
    int right = (int)number_or(yyjson_arr_get(right_bottom, 0), 0.0);
    int bottom = (int)number_or(yyjson_arr_get(right_bottom, 1), 0.0);
    if (scale <= 0.0 || scale > 100.0) scale = 1.1;
    if (width <= 0 || height <= 0) { width = 612; height = 352; }
    yyjson_mut_val *render = yyjson_mut_obj_add_obj(output, root, "render");
    return render &&
        yyjson_mut_obj_add_str(output, render, "profile", "mver-0.1.6") &&
        yyjson_mut_obj_add_real(output, render, "projectionScale", scale) &&
        yyjson_mut_obj_add_real(output, render, "offsetX", offset_x) &&
        yyjson_mut_obj_add_real(output, render, "offsetY", offset_y) &&
        yyjson_mut_obj_add_int(output, render, "referenceWidth", width) &&
        yyjson_mut_obj_add_int(output, render, "referenceHeight", height) &&
        yyjson_mut_obj_add_bool(output, render, "mirror", mirror) &&
        yyjson_mut_obj_add_bool(output, render, "pointerLeftHanded", left_handed) &&
        yyjson_mut_obj_add_bool(output, render, "mouseForceMove", force) &&
        yyjson_mut_obj_add_real(output, render, "mouseSpeed", mouse_speed) &&
        yyjson_mut_obj_add_bool(output, render, "customPointerBounds", custom) &&
        yyjson_mut_obj_add_int(output, render, "pointerLeft", left) &&
        yyjson_mut_obj_add_int(output, render, "pointerTop", top) &&
        yyjson_mut_obj_add_int(output, render, "pointerRight", right) &&
        yyjson_mut_obj_add_int(output, render, "pointerBottom", bottom) &&
        add_standard_pointer(output, root, config, candidate);
}

static bool sound_source(const BongoCatImportCandidate *candidate, size_t index,
    char *source, size_t capacity, char *relative, size_t relative_capacity) {
    static const char *extensions[] = {"wav", "ogg", "flac"};
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        char name[40], sounds[BONGO_CAT_PATH_CAP];
        snprintf(name, sizeof(name), "%zu.%s", index, extensions[i]);
        if (!bongo_cat_path_join(sounds, sizeof(sounds), candidate->assets, "sounds") ||
            !bongo_cat_path_join(source, capacity, sounds, name) ||
            !bongo_cat_path_is_file(source)) continue;
        snprintf(relative, relative_capacity, "resources/sounds/%s", name);
        return true;
    }
    return false;
}

static bool add_sound_clear(yyjson_mut_doc *output, yyjson_mut_val *items,
    yyjson_val *config, const BongoCatImportCandidate *candidate) {
    yyjson_val *decoration = yyjson_obj_get(config, "decoration");
    yyjson_val *row = yyjson_obj_get(decoration, "soundClear");
    if (!row) return true;
    char shortcut[BONGO_CAT_SHORTCUT_CAP];
    if (!bongo_cat_mver_chord(candidate, row, shortcut, sizeof(shortcut))) return false;
    yyjson_mut_val *item = yyjson_mut_arr_add_obj(output, items);
    return item && yyjson_mut_obj_add_str(output, item, "kind", "sound-clear") &&
        yyjson_mut_obj_add_strcpy(output, item, "shortcut", shortcut);
}

static bool add_sounds(yyjson_mut_doc *output, yyjson_mut_val *items,
    yyjson_val *config, yyjson_val *rows,
    const BongoCatImportCandidate *candidate, const BongoCatMverLabels *labels,
    const char *target) {
    if (!rows) return true;
    if (!yyjson_is_arr(rows)) return false;
    char target_resources[BONGO_CAT_PATH_CAP], target_sounds[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(target_resources, sizeof(target_resources), target, "resources") ||
        !bongo_cat_path_join(target_sounds, sizeof(target_sounds), target_resources, "sounds") ||
        !bongo_cat_path_create_directory(target_sounds)) return false;
    yyjson_val *decoration = yyjson_obj_get(config, "decoration");
    yyjson_val *keep_value = yyjson_obj_get(decoration, "soundKeep");
    bool keep = !keep_value || yyjson_get_bool(keep_value);
    size_t emitted = 0, index, count; yyjson_val *row;
    yyjson_arr_foreach(rows, index, count, row) {
        char shortcut[BONGO_CAT_SHORTCUT_CAP], source[BONGO_CAT_PATH_CAP];
        char relative[BONGO_CAT_PATH_CAP], destination[BONGO_CAT_PATH_CAP];
        if (!bongo_cat_mver_chord(candidate, row, shortcut, sizeof(shortcut))) return false;
        if (!sound_source(candidate, index, source, sizeof(source), relative,
            sizeof(relative))) continue;
        const char *name = bongo_cat_path_name(source);
        if (!bongo_cat_path_join(destination, sizeof(destination), target_sounds, name) ||
            !bongo_cat_path_copy_file(source, destination)) return false;
        yyjson_mut_val *item = yyjson_mut_arr_add_obj(output, items);
        const char *label = bongo_cat_mver_label(labels, "sounds", index);
        if (!item || !yyjson_mut_obj_add_str(output, item, "kind", "sound") ||
            !yyjson_mut_obj_add_strcpy(output, item, "shortcut", shortcut) ||
            !yyjson_mut_obj_add_strcpy(output, item, "sound", relative) ||
            (label && !yyjson_mut_obj_add_strcpy(output, item, "label", label)) ||
            (!keep && !yyjson_mut_obj_add_bool(output, item, "momentary", true))) return false;
        emitted++;
    }
    return !emitted || !keep || add_sound_clear(output, items, config, candidate);
}

bool bongo_cat_import_mver_metadata(const BongoCatImportCandidate *candidate,
    const char *target, BongoCatError *error) {
    if (candidate->format != BONGO_CAT_IMPORT_MVER &&
        candidate->format != BONGO_CAT_IMPORT_MVER_PATCH) return true;
    yyjson_doc *source = bongo_cat_json_read_file(candidate->config,
        YYJSON_READ_JSON5 | YYJSON_READ_ALLOW_INVALID_UNICODE, NULL);
    yyjson_val *config = source ? yyjson_doc_get_root(source) : NULL;
    yyjson_val *mode = yyjson_obj_get(config, bongo_cat_mode_name(candidate->mode));
    BongoCatMverLabels labels = {0};
    bongo_cat_mver_labels_load(candidate->config,
        bongo_cat_mode_name(candidate->mode), &labels);
    yyjson_mut_doc *output = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = output ? yyjson_mut_obj(output) : NULL;
    yyjson_mut_val *items = root ? yyjson_mut_obj_add_arr(output, root, "bindings") : NULL;
    if (output) yyjson_mut_doc_set_root(output, root);
    bool ok = yyjson_is_obj(mode) && items &&
        yyjson_mut_obj_add_int(output, root, "version", 1) &&
        add_render_profile(output, root, config, candidate) &&
        bongo_cat_mver_add_behaviors(output, items, mode, candidate, &labels, error) &&
        add_sounds(output, items, config, yyjson_obj_get(mode, "sounds"),
            candidate, &labels, target) &&
        bongo_cat_mver_effects(output, items, config, mode, candidate, target);
    char path[BONGO_CAT_PATH_CAP];
    if (ok) ok = bongo_cat_path_join(path, sizeof(path), target, MVER_METADATA) &&
        bongo_cat_json_write_file(path, output, YYJSON_WRITE_PRETTY, NULL);
    yyjson_mut_doc_free(output);
    yyjson_doc_free(source);
    if (!ok && error && !error->message[0])
        bongo_cat_error_set(error, BONGO_CAT_ERROR_FORMAT,
            "Cannot convert Mver behavior metadata: %s", candidate->config);
    return ok;
}
