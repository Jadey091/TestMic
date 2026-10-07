#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static char startup_log_path[BONGO_CAT_PATH_CAP];
static bool startup_is_ready;

static void SDLCALL log_output(void *userdata, int category,
    SDL_LogPriority priority, const char *message) {
    (void)userdata;
    FILE *file = startup_log_path[0]
        ? bongo_cat_file_open(startup_log_path, "ab") : NULL;
    if (file) {
        fprintf(file, "%lld [%d:%d] %s\n", (long long)time(NULL), category,
            (int)priority, message ? message : "");
        fclose(file);
    }
    fprintf(stderr, "%s\n", message ? message : "");
}

static bool store_argument(char *target, size_t capacity, const char *value,
    const char *name, BongoCatError *error) {
    int length = snprintf(target, capacity, "%s", value ? value : "");
    if (length >= 0 && (size_t)length < capacity) return true;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_ARGUMENT,
        "%s path is too long", name);
    return false;
}

static bool parse_arguments(BongoCatApp *app, int argc, char **argv,
    BongoCatError *error) {
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--autostart") == 0) app->autostart_launch = true;
        else if (strcmp(arg, "--ci-smoke") == 0) app->smoke = true;
        else if (strcmp(arg, "--ci-preferences") == 0) app->smoke_preferences = true;
        else if (strcmp(arg, "--ci-preference-shortcut") == 0)
            app->smoke_preference_shortcut = true;
        else if (strcmp(arg, "--ci-preference-model-select") == 0)
            app->smoke_preference_model_select = true;
        else if (strcmp(arg, "--ci-remove-imported") == 0) app->smoke_remove_imported = true;
        else if (strcmp(arg, "--ci-shortcuts") == 0) app->smoke_shortcuts = true;
        else if (strcmp(arg, "--ci-menu") == 0) app->smoke_menu = true;
        else if (strcmp(arg, "--ci-input-audit") == 0) app->smoke_input_audit = true;
        else if (strcmp(arg, "--ci-ignore-global-input") == 0) app->smoke_ignore_global_input = true;
        else if (strcmp(arg, "--ci-pass-through") == 0) app->smoke_pass_through = true;
        else if (strcmp(arg, "--ci-context-menu") == 0) app->smoke_context_menu = true;
        else if (strcmp(arg, "--ci-frame-series") == 0) app->smoke_frame_series = true;
        else if (strcmp(arg, "--ci-runtime-flow") == 0) app->smoke_runtime_flow = true;
        else if (strcmp(arg, "--ci-freeze-model") == 0) app->smoke_freeze_model = true;
        else if (strncmp(arg, "--ci-preference-page=", 21) == 0) {
            int page = atoi(arg + 21);
            if (page >= 0 && page < 5) app->smoke_preference_page = page;
        } else if (strncmp(arg, "--ci-language=", 14) == 0) {
            const char *name = arg + 14;
            for (int value = 0; value < BONGO_CAT_LANG_COUNT; ++value)
                if (strcmp(name, bongo_cat_language_name((BongoCatLanguage)value)) == 0)
                    app->smoke_language = value;
        } else if (strncmp(arg, "--ci-theme=", 11) == 0) {
            const char *name = arg + 11;
            for (int value = 0; value <= BONGO_CAT_THEME_DARK; ++value)
                if (strcmp(name, bongo_cat_theme_name((BongoCatTheme)value)) == 0)
                    app->smoke_theme = value;
        } else if (strncmp(arg, "--ci-exit-ms=", 13) == 0) {
            uint64_t delay = strtoull(arg + 13, NULL, 10);
            app->smoke_deadline_ns = delay > UINT64_MAX / 1000000ull
                ? UINT64_MAX : delay * 1000000ull;
        } else if (strncmp(arg, "--preferences=", 14) == 0) {
            if (!store_argument(app->preferences_path, sizeof(app->preferences_path),
                arg + 14, "Preferences", error)) return false;
        } else if (strncmp(arg, "--session=", 10) == 0) {
            if (!store_argument(app->session_path, sizeof(app->session_path),
                arg + 10, "Session", error)) return false;
        } else if (strncmp(arg, "--data-root=", 12) == 0) {
            if (!store_argument(app->data_root, sizeof(app->data_root),
                arg + 12, "Data", error)) return false;
        } else if (strncmp(arg, "--ci-import=", 12) == 0) {
            if (!store_argument(app->smoke_import_path, sizeof(app->smoke_import_path),
                arg + 12, "Import", error)) return false;
        } else if (strncmp(arg, "--ci-model=", 11) == 0) {
            if (!store_argument(app->smoke_model, sizeof(app->smoke_model),
                arg + 11, "Model", error)) return false;
        } else if (strncmp(arg, "--ci-viewer-trace=", 18) == 0) {
            if (!store_argument(app->smoke_viewer_trace,
                sizeof(app->smoke_viewer_trace), arg + 18,
                "Viewer trace", error)) return false;
        } else if (strncmp(arg, "--ci-live2d-scenario=", 21) == 0 &&
            !store_argument(app->smoke_live2d_scenario,
                sizeof(app->smoke_live2d_scenario), arg + 21, "Scenario", error)) return false;
    }
    if (app->smoke && !app->smoke_deadline_ns) app->smoke_deadline_ns = 1500000000ull;
    return true;
}

static bool writable_directory(const char *root) {
    char probe[BONGO_CAT_PATH_CAP];
    if (!root || !root[0] || !bongo_cat_path_create_directory(root) ||
        !bongo_cat_path_join(probe, sizeof(probe), root, ".startup-write-test")) return false;
    FILE *file = bongo_cat_file_open(probe, "wb");
    bool ok = file && fputc('1', file) != EOF;
    if (file && fclose(file) != 0) ok = false;
    bongo_cat_file_remove(probe);
    return ok;
}

static bool use_root(BongoCatApp *app, const char *root) {
    if (!root || !root[0] || !store_argument(app->data_root,
        sizeof(app->data_root), root, "Data", NULL)) return false;
    if (writable_directory(app->data_root)) return true;
    app->data_root[0] = '\0'; return false;
}

static bool locate_data_root(BongoCatApp *app, BongoCatError *error) {
    if (app->data_root[0]) {
        if (writable_directory(app->data_root)) return true;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "The selected data directory is not writable: %s", app->data_root);
        return false;
    }
    char *preferred = SDL_GetPrefPath("BongoCat", BONGO_CAT_NAME);
    bool found = use_root(app, preferred);
    SDL_free(preferred);
    char fallback[BONGO_CAT_PATH_CAP];
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (!found && home && bongo_cat_path_join(fallback, sizeof(fallback),
        home, ".bongo-cat")) found = use_root(app, fallback);
    const char *temporary = SDL_getenv(
#ifdef _WIN32
        "TEMP"); const char *temporary_name = "BongoCat";
#else
        "TMPDIR"); if (!temporary || !temporary[0]) temporary = "/tmp";
    char temporary_name[64]; snprintf(temporary_name, sizeof(temporary_name),
        "bongo-cat-%lu", (unsigned long)getuid());
#endif
    if (!found && temporary && bongo_cat_path_join(fallback, sizeof(fallback),
        temporary, temporary_name)) found = use_root(app, fallback);
    if (!found) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
        "No writable application data directory is available");
    return found;
}

static void begin_log(BongoCatApp *app) {
    char previous[BONGO_CAT_PATH_CAP], stage_path[BONGO_CAT_PATH_CAP];
    char interrupted[128] = {0};
    bongo_cat_path_join(startup_log_path, sizeof(startup_log_path),
        app->data_root, "startup.log");
    bongo_cat_path_join(previous, sizeof(previous), app->data_root,
        "startup-previous.log");
    bongo_cat_path_join(stage_path, sizeof(stage_path), app->data_root,
        "startup-stage.txt");
    FILE *stage = bongo_cat_file_open(stage_path, "rb");
    if (stage) {
        size_t length = fread(interrupted, 1, sizeof(interrupted) - 1, stage);
        interrupted[ferror(stage) ? 0 : length] = '\0';
        fclose(stage);
    }
    if (bongo_cat_path_is_file(startup_log_path))
        bongo_cat_file_replace(startup_log_path, previous, true);
    FILE *file = bongo_cat_file_open(startup_log_path, "wb");
    if (file) { fprintf(file, "BongoCat %s startup log\n", BONGO_CAT_VERSION); fclose(file); }
    SDL_SetLogOutputFunction(log_output, NULL);
    SDL_Log("Startup on %s; data=%s", SDL_GetPlatform(), app->data_root);
    if (interrupted[0]) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "Previous startup ended before readiness at stage: %s", interrupted);
}

bool bongo_cat_startup_prepare(BongoCatApp *app, int argc, char **argv,
    BongoCatError *error) {
    if (!parse_arguments(app, argc, argv, error) || !locate_data_root(app, error)) return false;
    if ((!app->preferences_path[0] && !bongo_cat_path_join(app->preferences_path,
        sizeof(app->preferences_path), app->data_root, "preferences.json")) ||
        (!app->session_path[0] && !bongo_cat_path_join(app->session_path,
        sizeof(app->session_path), app->data_root, "session.json"))) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Configuration path is too long"); return false;
    }
    begin_log(app); bongo_cat_startup_stage(app, "paths-ready"); return true;
}

void bongo_cat_startup_stage(BongoCatApp *app, const char *stage) {
    if (!app || !app->data_root[0] || !stage) return;
    char path[BONGO_CAT_PATH_CAP];
    if (bongo_cat_path_join(path, sizeof(path), app->data_root, "startup-stage.txt")) {
        FILE *file = bongo_cat_file_open(path, "wb");
        if (file) { fputs(stage, file); fclose(file); }
    }
    SDL_Log("Startup stage: %s", stage);
}

void bongo_cat_startup_ready(BongoCatApp *app) {
    if (!app || startup_is_ready) return;
    startup_is_ready = true;
    const char *names[] = {"startup-stage.txt", "startup-error.log"};
    for (size_t i = 0; i < 2; ++i) {
        char path[BONGO_CAT_PATH_CAP];
        if (bongo_cat_path_join(path, sizeof(path), app->data_root, names[i]))
            bongo_cat_file_remove(path);
    }
    SDL_Log("Startup ready");
}

static void write_error(BongoCatApp *app, const char *name, const char *message) {
    if (!app || !app->data_root[0]) return;
    char path[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(path, sizeof(path), app->data_root, name)) return;
    FILE *file = bongo_cat_file_open(path, "wb");
    if (file) { fputs(message, file); fputc('\n', file); fclose(file); }
}

#ifdef _WIN32
static void native_error_box(const char *message) {
    int count = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
    wchar_t *wide = count > 0 ? calloc((size_t)count, sizeof(*wide)) : NULL;
    if (wide) {
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wide, count);
        MessageBoxW(NULL, wide, BONGO_CAT_NAME_W, MB_OK | MB_ICONERROR);
        free(wide);
    }
}
#endif

void bongo_cat_startup_failure(BongoCatApp *app, const BongoCatError *error) {
    const char *message = error && error->message[0] ? error->message : "Initialization failed";
    if (app) { bongo_cat_startup_stage(app, "failed"); write_error(app, "startup-error.log", message); }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Startup failed: %s", message);
    char body[BONGO_CAT_PATH_CAP + 384];
    snprintf(body, sizeof(body), "BongoCat could not start.\n\n%s%s%s", message,
        app && app->data_root[0] ? "\n\nDiagnostic log:\n" : "",
        app && app->data_root[0] ? startup_log_path : "");
    if (app && app->smoke) return;
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, BONGO_CAT_NAME, body,
        app ? app->window : NULL)) {
#ifdef _WIN32
        native_error_box(body);
#endif
    }
}

void bongo_cat_startup_ci_failure(BongoCatApp *app,
    const BongoCatError *error) {
    if (!app) return;
    app->exit_code = 1;
    write_error(app, "ci-error.log", error && error->message[0]
        ? error->message : "CI operation failed");
}
