#include "windows_direct_input.h"
#include "bongo_cat/file.h"

#ifdef _WIN32
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef struct BongoCatDirectInput {
    LPDIRECTINPUT8A input;
    LPDIRECTINPUTDEVICE8A mouse;
    POINT absolute;
    bool absolute_ready;
    bool rebase_pending;
    ULONGLONG last_read_ms;
    FILE *audit;
    unsigned long long reads;
    unsigned long long reacquires;
    unsigned long long failures;
    long long total_x;
    long long total_y;
} BongoCatDirectInput;

void bongo_cat_windows_direct_input_destroy(BongoCatPlatform *platform) {
    BongoCatDirectInput *state = platform ? platform->relative_pointer : NULL;
    if (!state) return;
    if (state->mouse) {
        state->mouse->lpVtbl->Unacquire(state->mouse);
        state->mouse->lpVtbl->Release(state->mouse);
    }
    if (state->input) state->input->lpVtbl->Release(state->input);
    if (state->audit) {
        fprintf(state->audit,
            "summary reads=%llu reacquires=%llu failures=%llu total_x=%lld total_y=%lld\n",
            state->reads, state->reacquires, state->failures,
            state->total_x, state->total_y);
        fclose(state->audit);
    }
    free(state);
    platform->relative_pointer = NULL;
}

void bongo_cat_windows_direct_input_reset(BongoCatPlatform *platform) {
    double x = 0.0, y = 0.0;
    bongo_cat_platform_relative_pointer(platform, &x, &y);
    BongoCatDirectInput *state = platform ? platform->relative_pointer : NULL;
    if (state) state->rebase_pending = true;
}

bool bongo_cat_windows_direct_input_create(BongoCatPlatform *platform,
    void *window) {
    if (!platform || !window) return false;
    BongoCatDirectInput *state = calloc(1, sizeof(*state));
    if (!state || FAILED(DirectInput8Create(GetModuleHandleW(NULL),
        DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void **)&state->input,
        NULL))) {
        free(state);
        return false;
    }
    platform->relative_pointer = state;
    HRESULT result = state->input->lpVtbl->CreateDevice(state->input,
        &GUID_SysMouse, &state->mouse, NULL);
    if (SUCCEEDED(result)) result = state->mouse->lpVtbl->SetCooperativeLevel(
        state->mouse, (HWND)window, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    if (SUCCEEDED(result)) result = state->mouse->lpVtbl->SetDataFormat(
        state->mouse, &c_dfDIMouse);
    DIPROPDWORD property = {{sizeof(property), sizeof(property.diph),
        0, DIPH_DEVICE}, 16};
    if (SUCCEEDED(result)) result = state->mouse->lpVtbl->SetProperty(
        state->mouse, DIPROP_BUFFERSIZE, &property.diph);
    if (SUCCEEDED(result)) result = state->mouse->lpVtbl->Acquire(state->mouse);
    if (SUCCEEDED(result)) {
        state->absolute_ready = GetPhysicalCursorPos(&state->absolute) != FALSE;
        state->rebase_pending = true;
        state->last_read_ms = GetTickCount64();
        const char *audit_path = getenv("BONGO_CAT_DIRECT_INPUT_AUDIT_FILE");
        if (audit_path && audit_path[0]) {
            state->audit = bongo_cat_file_open(audit_path, "wb");
            if (state->audit) {
                fprintf(state->audit, "direct_input initialized=1\n");
                fflush(state->audit);
            }
        }
        return true;
    }
    bongo_cat_windows_direct_input_destroy(platform);
    return false;
}

bool bongo_cat_platform_relative_pointer(BongoCatPlatform *platform,
    double *x, double *y) {
    BongoCatDirectInput *state = platform ? platform->relative_pointer : NULL;
    if (!state || !state->mouse || !x || !y) return false;
    ULONGLONG now_ms = GetTickCount64();
    bool stale = state->last_read_ms && now_ms - state->last_read_ms > 250;
    state->last_read_ms = now_ms;
    DIMOUSESTATE mouse = {0};
    POINT absolute = {0};
    bool absolute_ready = GetPhysicalCursorPos(&absolute) != FALSE;
    bool fallback_ready = absolute_ready && state->absolute_ready;
    LONG fallback_x = fallback_ready ? absolute.x - state->absolute.x : 0;
    LONG fallback_y = fallback_ready ? absolute.y - state->absolute.y : 0;
    if (absolute_ready) {
        state->absolute = absolute;
        state->absolute_ready = true;
    } else state->absolute_ready = false;
    HRESULT result = state->mouse->lpVtbl->GetDeviceState(
        state->mouse, sizeof(mouse), &mouse);
    bool reacquired = false;
    if (FAILED(result)) {
        result = state->mouse->lpVtbl->Acquire(state->mouse);
        reacquired = SUCCEEDED(result);
        if (reacquired) result = state->mouse->lpVtbl->GetDeviceState(
            state->mouse, sizeof(mouse), &mouse);
    }
    state->reads++;
    if (reacquired) state->reacquires++;
    bool sample_ready = SUCCEEDED(result);
    bool available = sample_ready || fallback_ready;
    bool rebase = state->rebase_pending || stale || reacquired || !sample_ready;
    bool use_fallback = !stale && fallback_ready && (!sample_ready ||
        ((mouse.lX == 0 && mouse.lY == 0) &&
            (fallback_x != 0 || fallback_y != 0)));
    if (rebase) {
        *x = 0.0;
        *y = 0.0;
    } else if (sample_ready && !use_fallback) {
        *x = mouse.lX;
        *y = mouse.lY;
    } else if (fallback_ready) {
        *x = fallback_x;
        *y = fallback_y;
    } else {
        *x = 0.0;
        *y = 0.0;
    }
    state->total_x += (long long)*x;
    state->total_y += (long long)*y;
    state->rebase_pending = !available;
    if (!sample_ready) state->failures++;
    if (state->audit) {
        fprintf(state->audit,
            "read=%llu result=0x%08lx reacquired=%d stale=%d rebase=%d fallback=%d dx=%.0f dy=%.0f total_x=%lld total_y=%lld\n",
            state->reads, (unsigned long)result, reacquired, stale, rebase,
            use_fallback, *x, *y,
            state->total_x, state->total_y);
        fflush(state->audit);
    }
    return available && !rebase;
}
#endif
