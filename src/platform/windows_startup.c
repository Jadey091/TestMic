#include "bongo_cat/platform.h"
#include "windows_startup.h"

#ifdef _WIN32
#include <SDL3/SDL.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>

static HANDLE instance_mutex;
static HANDLE instance_wake_event;
static wchar_t instance_title[96] = BONGO_CAT_PET_WINDOW_TITLE_W;
static wchar_t instance_mutex_name[128] = L"Local\\BongoCat.SingleInstance";
static wchar_t instance_wake_name[128] = L"Local\\BongoCat.WakeInstance";
static bool identity_ready;

static bool safe_identity(const char *value) {
    if (!value || !value[0] || strlen(value) > 32) return false;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; ++cursor)
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_') return false;
    return true;
}

static void initialize_identity(void) {
    if (identity_ready) return;
    identity_ready = true;
    const char *value = SDL_getenv_unsafe("BONGO_CAT_TEST_INSTANCE_ID");
    if (!safe_identity(value)) return;
    swprintf(instance_title, sizeof(instance_title) / sizeof(instance_title[0]),
        L"%ls [%hs]", BONGO_CAT_PET_WINDOW_TITLE_W, value);
    swprintf(instance_mutex_name,
        sizeof(instance_mutex_name) / sizeof(instance_mutex_name[0]),
        L"Local\\BongoCat.SingleInstance.%hs", value);
    swprintf(instance_wake_name,
        sizeof(instance_wake_name) / sizeof(instance_wake_name[0]),
        L"Local\\BongoCat.WakeInstance.%hs", value);
}

const wchar_t *bongo_cat_windows_instance_title(void) {
    initialize_identity(); return instance_title;
}

static void create_wake_event(void) {
    if (!instance_wake_event)
        instance_wake_event = CreateEventW(NULL, FALSE, FALSE, instance_wake_name);
}

static void wake_existing_instance(void) {
    for (int attempt = 0; attempt < 30; ++attempt) {
        HANDLE wake = OpenEventW(EVENT_MODIFY_STATE, FALSE, instance_wake_name);
        if (wake) {
            bool signaled = SetEvent(wake) != FALSE;
            CloseHandle(wake);
            if (signaled) return;
        }
        HWND existing = FindWindowW(NULL, instance_title);
        if (existing) {
            ShowWindowAsync(existing, IsIconic(existing) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(existing);
            return;
        }
        Sleep(100);
    }
}

bool bongo_cat_platform_single_instance_begin(void) {
    initialize_identity();
    if (SDL_getenv_unsafe("BONGO_CAT_ALLOW_TEST_INSTANCES")) return true;
    instance_mutex = CreateMutexW(NULL, FALSE, instance_mutex_name);
    if (!instance_mutex) return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        create_wake_event(); return true;
    }
    wake_existing_instance();
    CloseHandle(instance_mutex); instance_mutex = NULL;
    instance_mutex = CreateMutexW(NULL, FALSE, instance_mutex_name);
    if (instance_mutex && GetLastError() != ERROR_ALREADY_EXISTS) {
        create_wake_event(); return true;
    }
    if (instance_mutex) CloseHandle(instance_mutex);
    instance_mutex = NULL;
    return false;
}

bool bongo_cat_platform_single_instance_take_wake(void) {
    return instance_wake_event &&
        WaitForSingleObject(instance_wake_event, 0) == WAIT_OBJECT_0;
}

void bongo_cat_platform_single_instance_end(void) {
    if (instance_wake_event) CloseHandle(instance_wake_event);
    instance_wake_event = NULL;
    if (instance_mutex) CloseHandle(instance_mutex);
    instance_mutex = NULL;
}

BongoCatResult bongo_cat_platform_set_autostart(bool enabled,
    BongoCatError *error) {
    HKEY key;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0,
        KEY_SET_VALUE, NULL, &key, NULL);
    if (result != ERROR_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Cannot open autostart registry key");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    if (enabled) {
        wchar_t executable[BONGO_CAT_PATH_CAP];
        DWORD length = GetModuleFileNameW(NULL, executable, BONGO_CAT_PATH_CAP);
        if (!length || length >= BONGO_CAT_PATH_CAP) {
            RegCloseKey(key); bongo_cat_error_set(error,
                BONGO_CAT_ERROR_PLATFORM, "Cannot determine executable path");
            return BONGO_CAT_ERROR_PLATFORM;
        }
        wchar_t command[BONGO_CAT_PATH_CAP + 20];
        swprintf(command, BONGO_CAT_PATH_CAP + 20, L"\"%ls\" --autostart", executable);
        result = RegSetValueExW(key, BONGO_CAT_NAME_W, 0, REG_SZ,
            (const BYTE *)command, (DWORD)((wcslen(command) + 1) * sizeof(wchar_t)));
    } else result = RegDeleteValueW(key, BONGO_CAT_NAME_W);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Cannot update autostart setting");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    return BONGO_CAT_OK;
}
#endif
