#include "windows_capture.h"
#include "windows_direct_input.h"
#include "windows_input.h"
#include "windows_popup.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static int failures;

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #value); \
    failures++; \
} } while (0)

static void test_popup_completion(void) {
    HWND window = CreateWindowExW(0, L"STATIC", L"BongoCat popup test",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    CHECK(window != NULL);
    if (!window) return;
    bongo_cat_windows_popup_complete(window);
    MSG message = {0};
    bool found = false;
    while (PeekMessageW(&message, window, 0, 0, PM_REMOVE))
        if (message.message == WM_NULL) found = true;
    CHECK(found);
    DestroyWindow(window);
}

static void test_input_startup(void) {
    SDL_unsetenv_unsafe("BONGO_CAT_TEST_HOOK_FAILURE");
    SDL_setenv_unsafe("BONGO_CAT_TEST_HOOK_DELAY_MS", "2000", 1);
    BongoCatInputState input;
    bongo_cat_input_init(&input);
    BongoCatPlatform platform = {.input = &input};
    ULONGLONG started = GetTickCount64();
    CHECK(bongo_cat_windows_input_start(&platform));
    CHECK(GetTickCount64() - started < 500);
    bongo_cat_windows_input_stop(&platform);
    CHECK(platform.native == NULL);
    SDL_unsetenv_unsafe("BONGO_CAT_TEST_HOOK_DELAY_MS");
    SDL_setenv_unsafe("BONGO_CAT_TEST_HOOK_FAILURE", "1", 1);
    CHECK(!bongo_cat_windows_input_start(&platform));
    CHECK(platform.native == NULL);
    SDL_unsetenv_unsafe("BONGO_CAT_TEST_HOOK_FAILURE");
}

static void test_direct_input_rebase(void) {
    HWND window = CreateWindowExW(0, L"STATIC", L"BongoCat input test",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    CHECK(window != NULL);
    if (!window) return;
    BongoCatPlatform platform = {0};
    if (bongo_cat_windows_direct_input_create(&platform, window)) {
        double x = 0.0, y = 0.0;
        CHECK(!bongo_cat_platform_relative_pointer(&platform, &x, &y));
        bongo_cat_windows_direct_input_reset(&platform);
        CHECK(!bongo_cat_platform_relative_pointer(&platform, &x, &y));
        bongo_cat_windows_direct_input_destroy(&platform);
        CHECK(platform.relative_pointer == NULL);
    }
    DestroyWindow(window);
}

int main(void) {
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC",
        L"BongoCat capture test", WS_POPUP, 0, 0, 32, 32,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    CHECK(window != NULL);
    if (window) {
        CHECK(bongo_cat_windows_capture_configure(window));
        LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
        CHECK((style & WS_EX_APPWINDOW) != 0);
        CHECK((style & WS_EX_TOOLWINDOW) == 0);
        CHECK(!bongo_cat_windows_capture_handle_message(window, WM_NULL, 0));
        CHECK(!bongo_cat_windows_capture_handle_message(window, WM_TIMER, 1));
        DestroyWindow(window);
    }
    test_popup_completion();
    test_input_startup();
    test_direct_input_rebase();
    return failures ? 1 : 0;
}
