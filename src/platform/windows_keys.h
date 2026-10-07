#ifndef BONGO_CAT_WINDOWS_KEYS_H
#define BONGO_CAT_WINDOWS_KEYS_H

#ifdef _WIN32
#include "bongo_cat/input.h"
#include <windows.h>

typedef struct BongoCatWindowsKeyboard {
    bool down[BONGO_CAT_INPUT_KEY_STATE_CAP];
    uint64_t changed_ms[BONGO_CAT_INPUT_KEY_STATE_CAP];
    char name[BONGO_CAT_INPUT_KEY_STATE_CAP][BONGO_CAT_ID_CAP];
} BongoCatWindowsKeyboard;

typedef void (*BongoCatWindowsKeyEmit)(bool down, const char *name,
    void *userdata);

const char *bongo_cat_windows_key_name(const KBDLLHOOKSTRUCT *key, char output[16]);
bool bongo_cat_windows_keyboard_event(BongoCatWindowsKeyboard *state,
    const KBDLLHOOKSTRUCT *key, WPARAM message, UINT *drop_key_up,
    BongoCatWindowsKeyEmit emit, void *userdata);
void bongo_cat_windows_keyboard_reconcile(BongoCatWindowsKeyboard *state,
    uint64_t now_ms, BongoCatWindowsKeyEmit emit, void *userdata);
#endif

#endif
