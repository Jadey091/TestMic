#include "model_import_mver_internal.h"
#include "model_storage.h"
#include "preferences_model_glyphs.h"
#include "preferences_state.h"
#include "ui_font_atlas.h"
#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #value); \
    failures++; \
} } while (0)

const char *test_mver_pointer_config(bool live2d);
bool test_mver_pointer_fixture_assets(const char *standard, const char *source);
bool test_mver_pointer_adapter(const char *adapter, bool expected_enabled);

static bool chord(const char *json, bool gamepad, const char *expected) {
    yyjson_doc *document = yyjson_read(json, strlen(json), 0);
    BongoCatImportCandidate candidate = {0};
    candidate.gamepad_buttons = gamepad;
    char output[BONGO_CAT_SHORTCUT_CAP];
    bool ok = document && bongo_cat_mver_chord(&candidate,
        yyjson_doc_get_root(document), output, sizeof(output));
    bool matches = ok && expected && strcmp(output, expected) == 0;
    yyjson_doc_free(document);
    return expected ? matches : !ok;
}

static bool write_text(const char *path, const char *text) {
    FILE *file = bongo_cat_file_open(path, "wb");
    bool ok = file && fwrite(text, 1, strlen(text), file) == strlen(text);
    if (file && fclose(file) != 0) ok = false;
    return ok;
}

static bool child(char *output, size_t capacity, const char *root,
    const char *name, bool directory) {
    return bongo_cat_path_join(output, capacity, root, name) &&
        (!directory || SDL_CreateDirectory(output));
}

static void labels_from_shortcut_rows(void) {
    static const char default_expression[] =
        "\xE9\xBB\x98\xE8\xAE\xA4\xE8\xA1\xA8\xE6\x83\x85";
    static const char keyboard_hidden[] =
        "\xE9\x94\xAE\xE7\x9B\x98\xE6\xB6\x88\xE5\xA4\xB1";
    char *temporary = SDL_GetCurrentDirectory();
    CHECK(temporary != NULL);
    if (!temporary) return;
    char root[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    snprintf(root, sizeof(root), "%s/bongo-cat-labels-%llu", temporary,
        (unsigned long long)SDL_GetTicksNS());
    CHECK(SDL_CreateDirectory(root));
    CHECK(child(path, sizeof(path), root, "config.json", false));
    CHECK(write_text(path,
        "{/* misleading: \\\"standard\\\" */standard:{"
        "l2d_expression:[[18,//\xE9\xBB\x98\xE8\xAE\xA4\xE8\xA1\xA8\xE6\x83\x85\n76],"
        "[18,77]],l2d_motion:[],l2d_motion_lockhand:"
        "[[18,//\xE9\x94\xAE\xE7\x9B\x98\xE6\xB6\x88\xE5\xA4\xB1\n49]],sounds:[]},"
        "keyboard:{l2d_expression:[]}}"));
    BongoCatMverLabels labels = {0};
    CHECK(bongo_cat_mver_labels_load(path, "standard", &labels));
    CHECK(labels.count == 2);
    CHECK(strcmp(bongo_cat_mver_label(&labels, "l2d_expression", 0),
        default_expression) == 0);
    CHECK(bongo_cat_mver_label(&labels, "l2d_expression", 1) == NULL);
    CHECK(strcmp(bongo_cat_mver_label(&labels, "l2d_motion_lockhand", 0),
        keyboard_hidden) == 0);
    CHECK(bongo_cat_model_remove_tree(root, NULL));
    SDL_free(temporary);
}

static void metadata_backfills_labels(void) {
    static const char keyboard_hidden[] =
        "\xE9\x94\xAE\xE7\x9B\x98\xE6\xB6\x88\xE5\xA4\xB1";
    char *temporary = SDL_GetCurrentDirectory();
    CHECK(temporary != NULL);
    if (!temporary) return;
    char root[BONGO_CAT_PATH_CAP], path[BONGO_CAT_PATH_CAP];
    snprintf(root, sizeof(root), "%s/bongo-cat-metadata-%llu", temporary,
        (unsigned long long)SDL_GetTicksNS());
    CHECK(SDL_CreateDirectory(root));
    CHECK(child(path, sizeof(path), root, ".bongo-cat-mver.json", false));
    CHECK(write_text(path, "{\"version\":1,\"bindings\":["
        "{\"kind\":\"motion\",\"group\":\"CAT_motion_lock\",\"index\":0,"
        "\"shortcut\":\"Alt+1\",\"label\":\"\xE9\x94\xAE\xE7\x9B\x98\xE6\xB6\x88\xE5\xA4\xB1\"},"
        "{\"kind\":\"motion\",\"group\":\"CAT_motion_lock\",\"index\":1,"
        "\"shortcut\":\"Alt+2\",\"label\":\"Imported\"},"
        "{\"kind\":\"expression\",\"index\":2,\"shortcut\":\"Alt+3\","
        "\"label\":\"Expression label\"}]}"));
    BongoCatApp *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    if (app) {
        app->config.behavior_shortcut_count = 2;
        snprintf(app->config.behavior_shortcuts[0].id,
            sizeof(app->config.behavior_shortcuts[0].id),
            "model:motion:CAT_motion_lock:0");
        snprintf(app->config.behavior_shortcuts[0].shortcut,
            sizeof(app->config.behavior_shortcuts[0].shortcut), "Control+1");
        snprintf(app->config.behavior_shortcuts[1].id,
            sizeof(app->config.behavior_shortcuts[1].id),
            "model:motion:CAT_motion_lock:1");
        snprintf(app->config.behavior_shortcuts[1].label,
            sizeof(app->config.behavior_shortcuts[1].label), "Custom name");
        bongo_cat_import_apply_metadata(app, "model", root);
        CHECK(app->config.behavior_shortcut_count == 3);
        CHECK(strcmp(app->config.behavior_shortcuts[0].shortcut, "Control+1") == 0);
        CHECK(strcmp(app->config.behavior_shortcuts[0].label, keyboard_hidden) == 0);
        CHECK(strcmp(app->config.behavior_shortcuts[1].label, "Custom name") == 0);
        CHECK(strcmp(app->config.behavior_shortcuts[2].id,
            "model:expression:2") == 0);
        CHECK(strcmp(app->config.behavior_shortcuts[2].label,
            "Expression label") == 0);
        free(app);
    }
    CHECK(bongo_cat_model_remove_tree(root, NULL));
    SDL_free(temporary);
}

static bool range_has(const uint32_t *ranges, uint32_t rune) {
    for (size_t i = 0; ranges[i]; i += 2)
        if (rune >= ranges[i] && rune <= ranges[i + 1]) return true;
    return false;
}

static void behavior_labels_add_font_glyphs(void) {
    static const unsigned char custom_label[] = {
        0xe7, 0x8c, 0xab, 0xe5, 0x92, 0xaa, 0xe5, 0xbd,
        0xa2, 0xe6, 0x80, 0x81, 0
    };
    BongoCatApp *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    if (!app) return;
    snprintf(app->behaviors.entries[0].label,
        sizeof(app->behaviors.entries[0].label),
        "\xE9\x94\xAE\xE7\x9B\x98\xE6\xB6\x88\xE5\xA4\xB1");
    app->behaviors.count = 1;
    memcpy(app->config.behavior_shortcuts[0].label, custom_label,
        sizeof(custom_label));
    app->config.behavior_shortcut_count = 1;
    uint32_t ranges[64] = {0x20, 0x7e, 0};
    bongo_cat_preferences_model_glyphs(app, ranges,
        sizeof(ranges) / sizeof(ranges[0]));
    CHECK(range_has(ranges, 0x952e));
    CHECK(range_has(ranges, 0x76d8));
    CHECK(range_has(ranges, 0x6d88));
    CHECK(range_has(ranges, 0x5931));
    CHECK(range_has(ranges, 0x732b));
    CHECK(range_has(ranges, 0x54aa));
    CHECK(range_has(ranges, 0x5f62));
    CHECK(range_has(ranges, 0x6001));
    free(app);
}

static void font_reload_defers_during_frame(void) {
    BongoCatPreferences value = {0};
    value.ui_initialized = true;
    value.ui.frame_building = true;
    CHECK(bongo_cat_preferences_reload_fonts(&value));
    CHECK(value.font_reload_pending);
    CHECK(value.render_dirty);
    BongoCatUIBackend backend = {0};
    backend.frame_building = true;
    CHECK(!bongo_cat_ui_font_atlas_reload(&backend, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, 1.0f));
}

static bool portable_fixture(const char *root) {
    char image[BONGO_CAT_PATH_CAP], standard[BONGO_CAT_PATH_CAP];
    char hand[BONGO_CAT_PATH_CAP], model[BONGO_CAT_PATH_CAP];
    char path[BONGO_CAT_PATH_CAP], source[BONGO_CAT_PATH_CAP];
    if (!SDL_CreateDirectory(root) || !child(image, sizeof(image), root, "img", true) ||
        !child(standard, sizeof(standard), image, "standard", true) ||
        !child(hand, sizeof(hand), standard, "hand", true) ||
        !child(model, sizeof(model), standard, "cat_model", true)) return false;
    if (!child(path, sizeof(path), root, "config.json", false) ||
        !write_text(path, test_mver_pointer_config(true)))
        return false;
    if (!child(path, sizeof(path), model, "cat.model3.json", false) ||
        !write_text(path, "{\"Version\":3,\"FileReferences\":{\"Moc\":\"cat.moc3\","
            "\"Textures\":[\"texture.png\"]}}") ||
        !child(path, sizeof(path), model, "cat.moc3", false) ||
        !write_text(path, "MOC3")) return false;
    snprintf(source, sizeof(source), "%s/resources/assets/tray.png",
        BONGO_CAT_NATIVE_SOURCE_DIR);
    if (!child(path, sizeof(path), model, "texture.png", false) ||
        !SDL_CopyFile(source, path) || !child(path, sizeof(path), hand, "0.png", false) ||
        !SDL_CopyFile(source, path) || !child(path, sizeof(path), standard, "bg.png", false) ||
        !SDL_CopyFile(source, path)) return false;
    return test_mver_pointer_fixture_assets(standard, source);
}

static void portable_model(void) {
    char root[BONGO_CAT_PATH_CAP], package[BONGO_CAT_PATH_CAP];
    char data[BONGO_CAT_PATH_CAP], mode[BONGO_CAT_PATH_CAP];
    char bundle[BONGO_CAT_PATH_CAP], nested_package[BONGO_CAT_PATH_CAP];
    char launch[BONGO_CAT_PATH_CAP];
    char variant[BONGO_CAT_PATH_CAP], variant_model[BONGO_CAT_PATH_CAP];
    char variant_img[BONGO_CAT_PATH_CAP], variant_standard[BONGO_CAT_PATH_CAP];
    char variant_hand[BONGO_CAT_PATH_CAP], source_hand[BONGO_CAT_PATH_CAP];
    char *temporary = SDL_GetCurrentDirectory();
    CHECK(temporary != NULL);
    if (!temporary) return;
    unsigned long long stamp = (unsigned long long)SDL_GetTicksNS();
    snprintf(root, sizeof(root), "%s/bongo-cat-portable-%llu", temporary,
        stamp);
    snprintf(data, sizeof(data), "%s/bongo-cat-portable-data-%llu", temporary,
        stamp);
    bongo_cat_model_remove_tree(root, NULL);
    bongo_cat_model_remove_tree(data, NULL);
    CHECK(SDL_CreateDirectory(root));
    CHECK(SDL_CreateDirectory(data));
    CHECK(child(package, sizeof(package), root, "app", true));
    CHECK(portable_fixture(package));
    CHECK(child(bundle, sizeof(bundle), root, "bundle", true));
    CHECK(child(launch, sizeof(launch), root, "launch", true));
    CHECK(child(nested_package, sizeof(nested_package), bundle, "model", true));
    CHECK(portable_fixture(nested_package));
    CHECK(child(variant, sizeof(variant), bundle, "variant", true));
    CHECK(child(variant_model, sizeof(variant_model), variant, "model", true));
    CHECK(child(variant_img, sizeof(variant_img), variant_model, "img", true));
    CHECK(child(variant_standard, sizeof(variant_standard), variant_img, "standard", true));
    CHECK(child(variant_hand, sizeof(variant_hand), variant_standard, "hand", true));
    CHECK(child(source_hand, sizeof(source_hand), nested_package,
        "img/standard/hand/0.png", false));
    CHECK(child(mode, sizeof(mode), variant_hand, "0.png", false) &&
        SDL_CopyFile(source_hand, mode));
    BongoCatImportDiscovery discovery = {0};
    BongoCatError error = {0};
    CHECK(bongo_cat_import_mver_discover_exact(package, &discovery, &error) == 1);
    CHECK(discovery.count == 1);
    CHECK(child(mode, sizeof(mode), package, "img/standard", false));
    BongoCatImportDiscovery nested = {0};
    CHECK(bongo_cat_import_mver_discover_exact(mode, &nested, &error) == 0);
    CHECK(bongo_cat_import_mver_discover(mode, &nested, &error) == 1);
    BongoCatImportDiscovery selected_container = {0};
    CHECK(bongo_cat_import_discover(root, &selected_container, &error));
    CHECK(selected_container.count == 3);
    size_t patch_count = 0;
    for (size_t i = 0; i < selected_container.count; ++i)
        if (selected_container.candidates[i].format == BONGO_CAT_IMPORT_MVER_PATCH)
            patch_count++;
    CHECK(patch_count == 1);
    BongoCatApp *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    if (!app) { bongo_cat_model_remove_tree(root, NULL); SDL_free(temporary); return; }
    bongo_cat_config_defaults(&app->config);
    bongo_cat_models_init(&app->models);
    snprintf(app->data_root, sizeof(app->data_root), "%s", data);
    CHECK(bongo_cat_import_portable_mver(app, package, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 1);
    CHECK(app->models.entries[0].managed && !app->models.entries[0].preset);
    CHECK(strcmp(app->models.entries[0].display_name, "app") == 0);
    CHECK(strcmp(app->models.entries[0].storage_directory, package) == 0);
    CHECK(strcmp(app->config.current_model, app->models.entries[0].id) == 0);
    char adapter_file[BONGO_CAT_PATH_CAP];
    CHECK(child(adapter_file, sizeof(adapter_file),
        app->models.entries[0].adapter_directory, "resources/left-keys/KeyA.png", false));
    CHECK(bongo_cat_path_is_file(adapter_file));
    CHECK(test_mver_pointer_adapter(
        app->models.entries[0].adapter_directory, true));
    BongoCatLive2DRenderOptions render = {0};
    CHECK(bongo_cat_import_mver_render_options(
        app->models.entries[0].adapter_directory, &render));
    CHECK(render.mver_projection && render.source_mirror);
    CHECK(render.projection_scale > 1.986f && render.projection_scale < 1.988f);
    CHECK(render.offset_y < -0.004f && render.offset_y > -0.006f);
    CHECK(render.reference_width == 1400 && render.reference_height == 1400);
    CHECK(render.custom_pointer_bounds && render.pointer_left == 100 &&
        render.pointer_top == 200 && render.pointer_right == 2100 &&
        render.pointer_bottom == 1400);
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_portable_mver(app, package, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 1);
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_portable_mver(app, mode, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 1);
    CHECK(strcmp(app->models.entries[0].storage_directory, package) == 0);
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_portable_mver(app, root, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 3);
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_portable_mver_scan(app, root, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 3);
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_nearby_mver(app, launch, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 3);
    for (size_t i = 0; i < app->models.count; ++i) {
        CHECK(app->models.entries[i].display_name[0] != '\0');
        CHECK(app->models.entries[i].managed);
    }
    CHECK(child(mode, sizeof(mode), package, "config.json", false));
    CHECK(write_text(mode, test_mver_pointer_config(false)));
    bongo_cat_models_init(&app->models);
    CHECK(bongo_cat_import_portable_mver(app, package, &error) == BONGO_CAT_OK);
    CHECK(app->models.count == 1);
    CHECK(test_mver_pointer_adapter(
        app->models.entries[0].adapter_directory, true));
    free(app);
    CHECK(bongo_cat_model_remove_tree(root, NULL));
    CHECK(bongo_cat_model_remove_tree(data, NULL));
    SDL_free(temporary);
}

int main(void) {
    CHECK(chord("[17,65]", true, "Control+A"));
    CHECK(chord("[0]", true, "Gamepad:South"));
    CHECK(chord("[15]", true, "Gamepad:Select"));
    CHECK(chord("[8]", false, "Backspace"));
    CHECK(chord("[16,65]", false, "Shift+A"));
    CHECK(chord("[17,18,90]", false, "Control+Alt+Z"));
    CHECK(chord("[0]", false, NULL));
    CHECK(chord("[16]", false, NULL));
    CHECK(chord("[17,65,66]", true, NULL));
    BongoCatMverKeyNames modifier = bongo_cat_mver_device_names(16, 1, 2);
    CHECK(modifier.count == 1 && strcmp(modifier.items[0], "ShiftRight") == 0);
    BongoCatMverKeyNames dpad = bongo_cat_mver_gamepad_names(12);
    CHECK(dpad.count == 1 && strcmp(dpad.items[0], "DPadUp") == 0);
    labels_from_shortcut_rows();
    metadata_backfills_labels();
    behavior_labels_add_font_glyphs();
    font_reload_defers_during_frame();
    portable_model();
    return failures ? 1 : 0;
}
