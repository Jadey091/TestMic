#include "preferences_state.h"
#include "preferences_model_glyphs.h"

#include <string.h>

static bool glyph_covered(const uint32_t *ranges, size_t used, uint32_t rune) {
    for (size_t i = 0; i + 1 < used; i += 2)
        if (rune >= ranges[i] && rune <= ranges[i + 1]) return true;
    return false;
}

static void add_text(uint32_t *ranges, size_t capacity,
    size_t *used, const char *text) {
    const char *cursor = text;
    while (cursor && *cursor) {
        nk_rune rune = 0;
        int decoded = nk_utf_decode(cursor, &rune, (int)strlen(cursor));
        if (decoded <= 0) { cursor++; continue; }
        cursor += decoded;
        if (rune <= 0x7e || glyph_covered(ranges, *used, rune)) continue;
        if (*used + 2 >= capacity) return;
        ranges[(*used)++] = rune;
        ranges[(*used)++] = rune;
        ranges[*used] = 0;
    }
}

void bongo_cat_preferences_model_glyphs(const BongoCatApp *app,
    uint32_t *ranges, size_t capacity) {
    if (!app || !ranges) return;
    size_t used = 0;
    while (used < capacity && ranges[used]) used++;
    if (used >= capacity || (used & 1)) return;
    for (size_t i = 0; i < app->models.count; ++i) {
        const BongoCatModelEntry *entry = &app->models.entries[i];
        add_text(ranges, capacity, &used,
            entry->display_name[0] ? entry->display_name : entry->id);
    }
    for (size_t i = 0; i < app->behaviors.count; ++i)
        add_text(ranges, capacity, &used, app->behaviors.entries[i].label);
    for (size_t i = 0;
        i < app->config.behavior_shortcut_count; ++i)
        add_text(ranges, capacity, &used,
            app->config.behavior_shortcuts[i].label);
}
