#include "bongo_cat/i18n.h"

#include <stdio.h>
#include <string.h>
#include <yyjson.h>

static yyjson_doc *load(const char *root, const char *name) {
    char path[BONGO_CAT_PATH_CAP];
    snprintf(path, sizeof(path), "%s/%s.json", root, name);
    return yyjson_read_file(path, 0, NULL, NULL);
}

static bool same_shape(yyjson_val *reference, yyjson_val *candidate,
    const char *path) {
    if (!reference || !candidate || yyjson_get_type(reference) != yyjson_get_type(candidate)) {
        fprintf(stderr, "Locale mismatch at %s\n", path);
        return false;
    }
    if (yyjson_is_obj(reference) &&
        yyjson_obj_size(reference) != yyjson_obj_size(candidate)) {
        fprintf(stderr, "Locale key count mismatch at %s\n", path);
        return false;
    }
    if (!yyjson_is_obj(reference)) return true;
    size_t index, count; yyjson_val *key, *value;
    yyjson_obj_foreach(reference, index, count, key, value) {
        const char *name = yyjson_get_str(key);
        yyjson_val *next = yyjson_obj_get(candidate, name);
        char child[BONGO_CAT_PATH_CAP];
        snprintf(child, sizeof(child), "%s%s%s", path, path[0] ? "." : "", name);
        if (!same_shape(value, next, child)) return false;
    }
    return true;
}

static bool includes(const uint32_t *ranges, uint32_t point) {
    for (size_t i = 0; ranges[i] && ranges[i + 1]; i += 2)
        if (point >= ranges[i] && point <= ranges[i + 1]) return true;
    return false;
}

static uint32_t next_utf8(const unsigned char **cursor) {
    const unsigned char *value = *cursor;
    uint32_t point = *value++;
    int remaining = 0;
    if (point < 0x80) { *cursor = value; return point; }
    if ((point & 0xe0) == 0xc0) { point &= 0x1f; remaining = 1; }
    else if ((point & 0xf0) == 0xe0) { point &= 0x0f; remaining = 2; }
    else if ((point & 0xf8) == 0xf0) { point &= 0x07; remaining = 3; }
    else { *cursor = value; return 0; }
    while (remaining--) {
        if ((*value & 0xc0) != 0x80) { *cursor = value; return 0; }
        point = (point << 6) | (*value++ & 0x3f);
    }
    *cursor = value;
    return point;
}

static bool covers_value(const uint32_t *ranges, yyjson_val *value) {
    if (yyjson_is_str(value)) {
        const unsigned char *text = (const unsigned char *)yyjson_get_str(value);
        while (*text) {
            uint32_t point = next_utf8(&text);
            if (point >= 0x20 && !includes(ranges, point)) return false;
        }
    } else if (yyjson_is_arr(value)) {
        size_t index, count; yyjson_val *item;
        yyjson_arr_foreach(value, index, count, item)
            if (!covers_value(ranges, item)) return false;
    } else if (yyjson_is_obj(value)) {
        size_t index, count; yyjson_val *key, *item;
        yyjson_obj_foreach(value, index, count, key, item)
            if (!covers_value(ranges, item)) return false;
    }
    return true;
}

int main(void) {
    char root[BONGO_CAT_PATH_CAP];
    snprintf(root, sizeof(root), "%s/resources/assets/locales", BONGO_CAT_NATIVE_SOURCE_DIR);
    yyjson_doc *reference = load(root, "zh-CN");
    if (!reference) return 1;
    const uint32_t expected[] = {'A', 0x4e2d, 0x8a2d, 0x00e7, 0x00fc,
        0x65e5, 0xd55c, 0x00ea, 0x0420, 0x00f1};
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        const char *name = bongo_cat_language_name((BongoCatLanguage)language);
        yyjson_doc *document = load(root, name);
        if (!document || !same_shape(yyjson_doc_get_root(reference),
            yyjson_doc_get_root(document), "")) return 2;
        BongoCatError error = {0};
        BongoCatI18n *i18n = bongo_cat_i18n_create(root, (BongoCatLanguage)language, &error);
        uint32_t ranges[2048];
        if (!i18n || bongo_cat_i18n_glyph_ranges(i18n, ranges, 2048) < 3 ||
            ranges[0] != 0x20 || !includes(ranges, expected[language]) ||
            !covers_value(ranges, yyjson_doc_get_root(document))) {
            fprintf(stderr, "Missing U+%04X for %s\n", expected[language], name);
            return 3;
        }
        bongo_cat_i18n_destroy(i18n);
        yyjson_doc_free(document);
    }
    BongoCatError error = {0};
    BongoCatI18n *all = bongo_cat_i18n_create(root,
        BONGO_CAT_LANG_EN_US, &error);
    uint32_t all_ranges[2048];
    const uint32_t menu_points[] = {0x7b80, 0x9ad4, 0x00e7, 0x00fc, 0x65e5,
        0xd55c, 0x00ea, 0x0420, 0x00f1};
    if (!all || bongo_cat_i18n_all_glyph_ranges(all, all_ranges, 2048) < 3)
        return 4;
    for (size_t i = 0; i < sizeof(menu_points) / sizeof(menu_points[0]); ++i)
        if (!includes(all_ranges, menu_points[i])) return 5;
    for (int language = 0; language < BONGO_CAT_LANG_COUNT; ++language) {
        yyjson_doc *document = load(root,
            bongo_cat_language_name((BongoCatLanguage)language));
        if (!document || !covers_value(all_ranges, yyjson_doc_get_root(document)))
            return 6;
        yyjson_doc_free(document);
    }
    bongo_cat_i18n_destroy(all);
    yyjson_doc_free(reference);
    puts("i18n smoke passed");
    return 0;
}
