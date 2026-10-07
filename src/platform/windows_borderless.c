#include "windows_borderless.h"
#include "windows_capture.h"

#ifdef _WIN32
#include <stdlib.h>

static const wchar_t original_proc_property[] = L"BongoCat.BorderlessWindowProc";
static const wchar_t click_through_property[] = L"BongoCat.ClickThrough";
static const wchar_t menu_binding_property[] = L"BongoCat.MenuBinding";
static const wchar_t drag_binding_property[] = L"BongoCat.DragBinding";
#define BONGO_CAT_MENU_PREVIEW_TIMER ((UINT_PTR)0xBC4E)
#define BONGO_CAT_DRAG_MODAL_TIMER ((UINT_PTR)0xBC50)

typedef struct WindowsMenuBinding {
    BongoCatMenuPreview preview;
    void (*tick)(void *userdata);
    void *userdata;
} WindowsMenuBinding;

typedef struct WindowsDragBinding {
    BongoCatModalTick tick;
    void *userdata;
} WindowsDragBinding;

static LONG_PTR borderless_style(LONG_PTR style) {
    return (style & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
        WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP;
}

static LRESULT CALLBACK borderless_window_proc(HWND window, UINT message,
    WPARAM wparam, LPARAM lparam) {
    WNDPROC original = (WNDPROC)GetPropW(window, original_proc_property);
    WindowsMenuBinding *menu = GetPropW(window, menu_binding_property);
    WindowsDragBinding *drag = GetPropW(window, drag_binding_property);
    if (bongo_cat_windows_capture_handle_message(window, message, wparam)) {
        return 0;
    } else if (message == WM_NCHITTEST && GetPropW(window, click_through_property)) {
        return HTTRANSPARENT;
    } else if (message == WM_SYSCOMMAND &&
        (wparam & 0xFFF0u) == SC_MINIMIZE) {
        return 0;
    } else if (message == WM_STYLECHANGING && wparam == (WPARAM)GWL_STYLE && lparam) {
        STYLESTRUCT *styles = (STYLESTRUCT *)lparam;
        styles->styleNew = (DWORD)borderless_style(styles->styleNew);
    } else if (message == WM_STYLECHANGED && wparam == (WPARAM)GWL_STYLE) {
        LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
        LONG_PTR fixed = borderless_style(style);
        if (fixed != style) SetWindowLongPtrW(window, GWL_STYLE, fixed);
    } else if (message == WM_NCPAINT) {
        return 0;
    } else if (message == WM_NCACTIVATE) {
        return TRUE;
    } else if (message == WM_MOUSEACTIVATE) {
        return MA_ACTIVATE;
    } else if (message == WM_MENUSELECT && menu && menu->preview) {
        UINT id = LOWORD(wparam), flags = HIWORD(wparam);
        if (flags == 0xffff && !lparam) return CallWindowProcW(
            original ? original : DefWindowProcW, window, message, wparam, lparam);
        if ((flags & (MF_POPUP | MF_SEPARATOR)) || !id)
            menu->preview(menu->userdata, BONGO_CAT_MENU_NONE);
        else
            menu->preview(menu->userdata, (BongoCatMenuAction)id);
    } else if (message == WM_TIMER &&
        wparam == BONGO_CAT_MENU_PREVIEW_TIMER && menu && menu->tick) {
        menu->tick(menu->userdata);
        return 0;
    } else if (message == WM_TIMER &&
        wparam == BONGO_CAT_DRAG_MODAL_TIMER && drag && drag->tick) {
        drag->tick(drag->userdata);
        return 0;
    }
    return CallWindowProcW(original ? original : DefWindowProcW,
        window, message, wparam, lparam);
}

void bongo_cat_windows_begin_drag(HWND window, BongoCatModalTick modal_tick,
    void *userdata) {
    if (!window) return;
    WindowsDragBinding binding = {modal_tick, userdata};
    bool bound = modal_tick && SetPropW(window, drag_binding_property, &binding);
    if (bound) {
        modal_tick(userdata);
        SetTimer(window, BONGO_CAT_DRAG_MODAL_TIMER, 16, NULL);
    }
    ReleaseCapture();
    SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    if (bound) {
        KillTimer(window, BONGO_CAT_DRAG_MODAL_TIMER);
        RemovePropW(window, drag_binding_property);
        modal_tick(userdata);
    }
}

static void clear_menu_binding(HWND window) {
    WindowsMenuBinding *menu = window ?
        GetPropW(window, menu_binding_property) : NULL;
    if (!menu) return;
    KillTimer(window, BONGO_CAT_MENU_PREVIEW_TIMER);
    RemovePropW(window, menu_binding_property);
    free(menu);
}

void bongo_cat_windows_menu_preview(HWND window, BongoCatMenuPreview preview,
    void (*tick)(void *userdata), void *userdata) {
    if (!window) return;
    clear_menu_binding(window);
    if (!preview && !tick) return;
    WindowsMenuBinding *menu = calloc(1, sizeof(*menu));
    if (!menu) return;
    menu->preview = preview;
    menu->tick = tick;
    menu->userdata = userdata;
    if (!SetPropW(window, menu_binding_property, menu)) {
        free(menu);
        return;
    }
    if (tick) SetTimer(window, BONGO_CAT_MENU_PREVIEW_TIMER, 16, NULL);
}

void bongo_cat_windows_borderless_install(HWND window) {
    if (!window || GetPropW(window, original_proc_property)) return;
    LONG_PTR style = borderless_style(GetWindowLongPtrW(window, GWL_STYLE));
    SetWindowLongPtrW(window, GWL_STYLE, style);
    WNDPROC original = (WNDPROC)GetWindowLongPtrW(window, GWLP_WNDPROC);
    if (!original || !SetPropW(window, original_proc_property, (HANDLE)original)) return;
    SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)borderless_window_proc);
}

void bongo_cat_windows_borderless_uninstall(HWND window) {
    if (!window) return;
    clear_menu_binding(window);
    KillTimer(window, BONGO_CAT_DRAG_MODAL_TIMER);
    RemovePropW(window, drag_binding_property);
    WNDPROC original = (WNDPROC)GetPropW(window, original_proc_property);
    if (!original) return;
    SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)original);
    RemovePropW(window, original_proc_property);
    RemovePropW(window, click_through_property);
}

void bongo_cat_windows_borderless_set_click_through(HWND window, bool enabled) {
    if (!window) return;
    if (enabled) SetPropW(window, click_through_property, (HANDLE)(INT_PTR)1);
    else RemovePropW(window, click_through_property);
    LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    LONG_PTR next = enabled ? style | WS_EX_TRANSPARENT :
        style & ~WS_EX_TRANSPARENT;
    if (next != style) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, next);
        SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
#endif
