#include "windows_capture.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shobjidl.h>

static UINT taskbar_created_message;
static UINT capture_refresh_message;
static bool removal_warning_emitted;
static bool style_warning_emitted;
#define BONGO_CAT_CAPTURE_REFRESH_TIMER ((UINT_PTR)0xBC51)

static bool read_extended_style(HWND window, LONG_PTR *style) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR value = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (!value && GetLastError() != ERROR_SUCCESS) return false;
    *style = value;
    return true;
}

static bool write_extended_style(HWND window, LONG_PTR style) {
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(window, GWL_EXSTYLE, style);
    return previous || GetLastError() == ERROR_SUCCESS;
}

static void register_messages(void) {
    if (!taskbar_created_message)
        taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    if (!capture_refresh_message)
        capture_refresh_message = RegisterWindowMessageW(
            L"BongoCat.CaptureWindow.RefreshTaskbar");
}

static HRESULT remove_taskbar_tab(HWND window) {
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return initialized;
    ITaskbarList *taskbar = NULL;
    HRESULT result = CoCreateInstance(&CLSID_TaskbarList, NULL,
        CLSCTX_INPROC_SERVER, &IID_ITaskbarList, (void **)&taskbar);
    if (SUCCEEDED(result)) result = taskbar->lpVtbl->HrInit(taskbar);
    if (SUCCEEDED(result)) result = taskbar->lpVtbl->DeleteTab(taskbar, window);
    if (taskbar) taskbar->lpVtbl->Release(taskbar);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return result;
}

static void refresh_transparency(HWND window) {
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(window, &margins);
    HRGN region = CreateRectRgn(-1, -1, 0, 0);
    if (!region) return;
    DWM_BLURBEHIND blur = {0};
    blur.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    blur.fEnable = TRUE;
    blur.hRgnBlur = region;
    DwmEnableBlurBehindWindow(window, &blur);
    DeleteObject(region);
}

static void refresh_taskbar(HWND window) {
    if (!window) return;
    HRESULT result = remove_taskbar_tab(window);
    if (FAILED(result) && IsWindowVisible(window) && !removal_warning_emitted) {
        removal_warning_emitted = true;
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Cannot remove the capture window from the taskbar (0x%08lx)",
            (unsigned long)result);
    }
}

static void schedule_refresh(HWND window) {
    register_messages();
    if (!window) return;
    if (capture_refresh_message)
        PostMessageW(window, capture_refresh_message, 0, 0);
    SetTimer(window, BONGO_CAT_CAPTURE_REFRESH_TIMER, 250, NULL);
}

bool bongo_cat_windows_capture_configure(HWND window) {
    if (!window || !IsWindow(window)) return false;
    register_messages();
    LONG_PTR style = 0;
    if (!read_extended_style(window, &style)) {
        if (!style_warning_emitted) {
            style_warning_emitted = true;
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                "Cannot read the window style required for OBS discovery");
        }
        return false;
    }
    LONG_PTR next = (style | WS_EX_APPWINDOW) & ~WS_EX_TOOLWINDOW;
    if (next != style) {
        bool shown = IsWindowVisible(window) != FALSE;
        if (shown) ShowWindow(window, SW_HIDE);
        if (write_extended_style(window, next)) {
            SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            refresh_transparency(window);
        }
        if (shown) {
            ShowWindow(window, SW_SHOWNOACTIVATE);
            UpdateWindow(window);
        }
    }
    LONG_PTR applied = 0;
    bool ready = read_extended_style(window, &applied) &&
        (applied & WS_EX_APPWINDOW) && !(applied & WS_EX_TOOLWINDOW);
    if (!ready && !style_warning_emitted) {
        style_warning_emitted = true;
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
            "Cannot configure the window for OBS discovery");
    }
    refresh_taskbar(window);
    schedule_refresh(window);
    return ready;
}

bool bongo_cat_windows_capture_handle_message(
    HWND window, UINT message, WPARAM wparam) {
    register_messages();
    if (capture_refresh_message && message == capture_refresh_message) {
        refresh_taskbar(window);
        return true;
    }
    if (message == WM_TIMER &&
        wparam == BONGO_CAT_CAPTURE_REFRESH_TIMER) {
        KillTimer(window, BONGO_CAT_CAPTURE_REFRESH_TIMER);
        refresh_taskbar(window);
        return true;
    }
    if (taskbar_created_message && message == taskbar_created_message)
        schedule_refresh(window);
    return false;
}
#endif
