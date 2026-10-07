#ifndef BONGO_CAT_WINDOWS_CAPTURE_H
#define BONGO_CAT_WINDOWS_CAPTURE_H

#ifdef _WIN32
#include <stdbool.h>
#include <windows.h>

bool bongo_cat_windows_capture_configure(HWND window);
bool bongo_cat_windows_capture_handle_message(
    HWND window, UINT message, WPARAM wparam);
#endif

#endif
