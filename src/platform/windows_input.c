#include "windows_input.h"
#include "windows_keys.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef struct WindowsInputState {
    BongoCatPlatform *platform;
    SRWLOCK platform_lock;
    HANDLE thread;
    HANDLE stop;
    HHOOK keyboard;
    HHOOK mouse;
    BongoCatWindowsKeyboard keyboard_state;
    UINT test_drop_key_up;
    DWORD test_start_delay_ms;
} WindowsInputState;

static WindowsInputState *global_state;

static void wake_main_thread(WindowsInputState *state) {
    if (!state || !state->platform) return;
    SDL_Event wake = {0};
    wake.type = state->platform->wake_event_type;
    SDL_PushEvent(&wake);
}

static void push_event(BongoCatInputKind kind, const char *name, float value) {
    WindowsInputState *state = global_state;
    if (!state || !name) return;
    AcquireSRWLockShared(&state->platform_lock);
    BongoCatPlatform *platform = state->platform;
    if (!platform) {
        ReleaseSRWLockShared(&state->platform_lock);
        return;
    }
    BongoCatInputEvent event = {0};
    event.kind = kind;
    event.timestamp_ms = GetTickCount64();
    event.value = value;
    snprintf(event.name, sizeof(event.name), "%s", name);
    if (bongo_cat_input_push(platform->input, &event)) wake_main_thread(state);
    ReleaseSRWLockShared(&state->platform_lock);
}

static void emit_key(bool down, const char *name, void *userdata) {
    (void)userdata;
    push_event(down ? BONGO_CAT_INPUT_KEY_DOWN : BONGO_CAT_INPUT_KEY_UP,
        name, down ? 1.0f : 0.0f);
}

static LRESULT CALLBACK keyboard_hook(int code, WPARAM message, LPARAM data) {
    WindowsInputState *state = global_state;
    if (code == HC_ACTION && state) bongo_cat_windows_keyboard_event(
        &state->keyboard_state, (const KBDLLHOOKSTRUCT *)data, message,
        &state->test_drop_key_up, emit_key, NULL);
    return CallNextHookEx(NULL, code, message, data);
}

static const char *mouse_button(WPARAM message, DWORD mouse_data) {
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: return "Left";
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: return "Right";
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: return "Middle";
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return HIWORD(mouse_data) == XBUTTON1 ? "Back" : "Forward";
    default: return NULL;
    }
}

static LRESULT CALLBACK mouse_hook(int code, WPARAM message, LPARAM data) {
    WindowsInputState *state = global_state;
    if (code == HC_ACTION && state) {
        const MSLLHOOKSTRUCT *mouse = (const MSLLHOOKSTRUCT *)data;
        if (message == WM_MOUSEMOVE) {
            AcquireSRWLockShared(&state->platform_lock);
            BongoCatPlatform *platform = state->platform;
            if (platform && bongo_cat_input_mouse(platform->input,
                mouse->pt.x, mouse->pt.y)) wake_main_thread(state);
            ReleaseSRWLockShared(&state->platform_lock);
        } else {
            const char *name = mouse_button(message, mouse->mouseData);
            bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
            if (name) push_event(down ? BONGO_CAT_INPUT_MOUSE_DOWN :
                BONGO_CAT_INPUT_MOUSE_UP, name, down ? 1.0f : 0.0f);
        }
    }
    return CallNextHookEx(NULL, code, message, data);
}

static void dispatch_messages(void) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

static DWORD WINAPI input_thread(void *context) {
    WindowsInputState *state = context;
    if (state->test_start_delay_ms && WaitForSingleObject(state->stop,
        state->test_start_delay_ms) == WAIT_OBJECT_0) return 0;
    if (WaitForSingleObject(state->stop, 0) == WAIT_OBJECT_0) return 0;
    global_state = state;
    state->keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook, NULL, 0);
    state->mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook, NULL, 0);
    if (!state->keyboard || !state->mouse) SDL_LogWarn(
        SDL_LOG_CATEGORY_APPLICATION,
        "Global input hooks are unavailable; global input may be incomplete");
    ULONGLONG last_reconcile_ms = GetTickCount64();
    for (;;) {
        DWORD wait = MsgWaitForMultipleObjects(1, &state->stop, FALSE, 25,
            QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_OBJECT_0 + 1) dispatch_messages();
        else if (wait == WAIT_FAILED) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "Windows input thread wait failed");
            break;
        }
        ULONGLONG now_ms = GetTickCount64();
        if (now_ms - last_reconcile_ms >= 25) {
            bongo_cat_windows_keyboard_reconcile(&state->keyboard_state,
                now_ms, emit_key, NULL);
            last_reconcile_ms = now_ms;
        }
    }
    if (state->keyboard) UnhookWindowsHookEx(state->keyboard);
    if (state->mouse) UnhookWindowsHookEx(state->mouse);
    global_state = NULL;
    return 0;
}

bool bongo_cat_windows_input_start(BongoCatPlatform *platform) {
    if (!platform || SDL_getenv("BONGO_CAT_TEST_HOOK_FAILURE")) return false;
    WindowsInputState *state = calloc(1, sizeof(*state));
    if (!state) return false;
    InitializeSRWLock(&state->platform_lock);
    state->platform = platform;
    const char *drop_key_up = SDL_getenv("BONGO_CAT_TEST_DROP_KEY_UP");
    if (drop_key_up) state->test_drop_key_up =
        (UINT)strtoul(drop_key_up, NULL, 10);
    const char *start_delay = SDL_getenv("BONGO_CAT_TEST_HOOK_DELAY_MS");
    if (start_delay) {
        unsigned long delay = strtoul(start_delay, NULL, 10);
        state->test_start_delay_ms = delay > 10000 ? 10000 : (DWORD)delay;
    }
    state->stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    state->thread = state->stop ?
        CreateThread(NULL, 0, input_thread, state, 0, NULL) : NULL;
    if (!state->stop || !state->thread) {
        if (state->thread) CloseHandle(state->thread);
        if (state->stop) CloseHandle(state->stop);
        free(state);
        return false;
    }
    platform->native = state;
    return true;
}

void bongo_cat_windows_input_stop(BongoCatPlatform *platform) {
    WindowsInputState *state = platform ? platform->native : NULL;
    if (!state) return;
    AcquireSRWLockExclusive(&state->platform_lock);
    state->platform = NULL;
    ReleaseSRWLockExclusive(&state->platform_lock);
    SetEvent(state->stop);
    if (WaitForSingleObject(state->thread, 3000) != WAIT_OBJECT_0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Windows input thread did not stop; its state will remain isolated until exit");
        platform->native = NULL;
        return;
    }
    CloseHandle(state->thread);
    CloseHandle(state->stop);
    free(state);
    platform->native = NULL;
}
#endif
