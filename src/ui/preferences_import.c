#include "preferences_internal.h"
#include "preferences_state.h"
#include "preferences_model_cover.h"
#include "../runtime/model_import.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BongoCatImportJob {
    size_t count;
    char **paths;
    char data_root[BONGO_CAT_PATH_CAP];
    char first_id[BONGO_CAT_ID_CAP];
    size_t imported_count;
    BongoCatResult result;
    BongoCatError error;
    BongoCatImportDialog *dialog;
    bool completion;
} BongoCatImportJob;

enum {
    BONGO_CAT_IMPORT_EVENT_CODE = 0x42434e49,
    BONGO_CAT_IMPORT_COMPLETE_CODE = 0x42434e4a
};

struct BongoCatImportDialog {
    SDL_Mutex *mutex;
    Uint32 event_type;
    SDL_WindowID window_id;
    SDL_Thread *worker;
    BongoCatImportJob *worker_job;
    int references;
    bool active;
    bool open;
    bool busy;
};

static void free_job(BongoCatImportJob *job) {
    if (!job) return;
    for (size_t i = 0; i < job->count; ++i) SDL_free(job->paths[i]);
    SDL_free(job->paths);
    SDL_free(job);
}

static void release_dialog(BongoCatImportDialog *dialog) {
    bool destroy = false;
    SDL_LockMutex(dialog->mutex);
    destroy = --dialog->references == 0;
    SDL_UnlockMutex(dialog->mutex);
    if (destroy) {
        SDL_DestroyMutex(dialog->mutex);
        SDL_free(dialog);
    }
}

BongoCatImportDialog *bongo_cat_preferences_import_create(void) {
    BongoCatImportDialog *dialog = SDL_calloc(1, sizeof(*dialog));
    if (!dialog) return NULL;
    dialog->event_type = SDL_RegisterEvents(1);
    dialog->mutex = SDL_CreateMutex();
    if (dialog->event_type == (Uint32)-1 || !dialog->mutex) {
        if (dialog->mutex) SDL_DestroyMutex(dialog->mutex);
        SDL_free(dialog);
        return NULL;
    }
    dialog->references = 1;
    dialog->active = true;
    return dialog;
}

void bongo_cat_preferences_import_destroy(BongoCatImportDialog *dialog) {
    if (!dialog) return;
    SDL_LockMutex(dialog->mutex);
    dialog->active = false;
    dialog->open = false;
    SDL_Thread *worker = dialog->worker;
    SDL_UnlockMutex(dialog->mutex);
    if (worker) SDL_WaitThread(worker, NULL);
    bool release_worker = false;
    SDL_LockMutex(dialog->mutex);
    dialog->worker = NULL;
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, dialog->event_type,
        dialog->event_type) > 0) {
        if (event.user.data2 != dialog) continue;
        BongoCatImportJob *job = (BongoCatImportJob *)event.user.data1;
        release_worker = release_worker || (job && job->completion);
        free_job(job);
    }
    dialog->worker_job = NULL;
    dialog->busy = false;
    SDL_UnlockMutex(dialog->mutex);
    if (release_worker) release_dialog(dialog);
    release_dialog(dialog);
}

bool bongo_cat_preferences_import_is_open(const BongoCatImportDialog *dialog) {
    if (!dialog) return false;
    SDL_LockMutex(dialog->mutex);
    bool open = dialog->open || dialog->busy;
    SDL_UnlockMutex(dialog->mutex);
    return open;
}

static BongoCatImportJob *copy_job(const char *const *files) {
    if (!files || !files[0]) return NULL;
    size_t count = 0;
    while (files[count]) ++count;
    BongoCatImportJob *job = SDL_calloc(1, sizeof(*job));
    if (!job) return NULL;
    job->paths = SDL_calloc(count, sizeof(*job->paths));
    if (!job->paths) { SDL_free(job); return NULL; }
    job->count = count;
    for (size_t i = 0; i < count; ++i) {
        job->paths[i] = SDL_strdup(files[i]);
        if (!job->paths[i]) { free_job(job); return NULL; }
    }
    return job;
}

static BongoCatImportJob *copy_path(const char *path) {
    const char *files[] = {path, NULL};
    return path && path[0] ? copy_job(files) : NULL;
}

static int SDLCALL import_worker(void *userdata) {
    BongoCatImportJob *job = userdata;
    job->result = BONGO_CAT_ERROR_FORMAT;
    for (size_t i = 0; i < job->count; ++i) {
        BongoCatImportReceipt receipt;
        BongoCatError error = {0};
        BongoCatResult result = bongo_cat_import_install(job->paths[i],
            job->data_root, &receipt, &error);
        if (result == BONGO_CAT_OK) {
            if (!job->first_id[0] && receipt.count)
                snprintf(job->first_id, sizeof(job->first_id), "%s", receipt.ids[0]);
            job->imported_count += receipt.count;
            job->result = BONGO_CAT_OK;
        } else if (!job->error.message[0]) {
            job->result = result;
            job->error = error;
        }
    }
    if (!job->imported_count && !job->error.message[0])
        bongo_cat_error_set(&job->error, job->result,
            "No model could be imported");
    SDL_Event event = {0};
    SDL_LockMutex(job->dialog->mutex);
    bool notify = job->dialog->active;
    if (notify) {
        job->completion = true;
        event.type = job->dialog->event_type;
        event.user.windowID = job->dialog->window_id;
        event.user.code = BONGO_CAT_IMPORT_COMPLETE_CODE;
        event.user.data1 = job;
        event.user.data2 = job->dialog;
        notify = SDL_PushEvent(&event);
    }
    if (!notify) {
        job->dialog->worker_job = NULL;
        job->dialog->busy = false;
    }
    SDL_UnlockMutex(job->dialog->mutex);
    if (!notify) {
        BongoCatImportDialog *dialog = job->dialog;
        free_job(job);
        release_dialog(dialog);
    }
    return 0;
}

static bool start_job(BongoCatImportDialog *dialog, BongoCatApp *app,
    SDL_WindowID window_id, BongoCatImportJob *job) {
    if (!dialog || !app || !job || !app->data_root[0]) return false;
    snprintf(job->data_root, sizeof(job->data_root), "%s", app->data_root);
    job->dialog = dialog;
    SDL_LockMutex(dialog->mutex);
    if (!dialog->active || dialog->busy) {
        SDL_UnlockMutex(dialog->mutex);
        return false;
    }
    dialog->window_id = window_id;
    dialog->busy = true;
    dialog->worker_job = job;
    ++dialog->references;
    dialog->worker = SDL_CreateThread(import_worker, "bongo-cat-model-import", job);
    if (!dialog->worker) {
        dialog->worker_job = NULL;
        dialog->busy = false;
        --dialog->references;
    }
    SDL_UnlockMutex(dialog->mutex);
    return dialog->worker != NULL;
}

static void SDLCALL import_callback(void *userdata, const char *const *files,
    int filter) {
    (void)filter;
    BongoCatImportDialog *dialog = userdata;
    BongoCatImportJob *job = copy_job(files);
    SDL_Event event = {0};
    bool pushed = false;
    SDL_LockMutex(dialog->mutex);
    event.type = dialog->event_type;
    event.user.windowID = dialog->window_id;
    event.user.code = BONGO_CAT_IMPORT_EVENT_CODE;
    event.user.data1 = job;
    event.user.data2 = dialog;
    if (dialog->active && !dialog->busy && job) pushed = SDL_PushEvent(&event);
    if (!pushed) dialog->open = false;
    SDL_UnlockMutex(dialog->mutex);
    if (!pushed) free_job(job);
    release_dialog(dialog);
}

bool bongo_cat_preferences_import_open(BongoCatImportDialog *dialog,
    SDL_Window *window) {
    if (!dialog || !window) return false;
    SDL_LockMutex(dialog->mutex);
    if (!dialog->active || dialog->open || dialog->busy) {
        SDL_UnlockMutex(dialog->mutex);
        return false;
    }
    dialog->open = true;
    dialog->window_id = SDL_GetWindowID(window);
    ++dialog->references;
    SDL_UnlockMutex(dialog->mutex);
    SDL_ShowOpenFolderDialog(import_callback, dialog, window, NULL, true);
    return true;
}

static void complete_job(BongoCatImportDialog *dialog, BongoCatApp *app,
    BongoCatImportJob *job) {
    SDL_Thread *worker = NULL;
    SDL_LockMutex(dialog->mutex);
    if (dialog->worker_job == job) {
        worker = dialog->worker;
        dialog->worker = NULL;
        dialog->worker_job = NULL;
        dialog->busy = false;
    }
    SDL_UnlockMutex(dialog->mutex);
    if (worker) SDL_WaitThread(worker, NULL);
    if (job->result == BONGO_CAT_OK && job->first_id[0]) {
        SDL_GL_MakeCurrent(app->window, app->gl_context);
        bongo_cat_app_rescan_models(app);
        if (!bongo_cat_app_select_model(app, job->first_id)) {
            job->result = BONGO_CAT_ERROR_CUBISM;
            bongo_cat_error_set(&job->error, job->result,
                "Model imported but could not be loaded");
        } else {
            const BongoCatModelEntry *entry = bongo_cat_models_find(
                &app->models, job->first_id);
            bongo_cat_preferences_model_cover_capture(app, entry);
        }
    }
    bongo_cat_preferences_import_complete(app, job->result, &job->error,
        job->imported_count);
    free_job(job);
    release_dialog(dialog);
}

bool bongo_cat_preferences_import_event(BongoCatImportDialog *dialog,
    BongoCatApp *app, const SDL_Event *event) {
    if (!dialog || !event || event->type != dialog->event_type ||
        event->user.data2 != dialog) return false;
    BongoCatImportJob *job = (BongoCatImportJob *)event->user.data1;
    if (event->user.code == BONGO_CAT_IMPORT_EVENT_CODE) {
        SDL_Window *owner = SDL_GetWindowFromID(event->user.windowID);
        SDL_LockMutex(dialog->mutex);
        dialog->open = false;
        bool accept = dialog->active && owner != NULL;
        SDL_UnlockMutex(dialog->mutex);
        if (accept && app && job && !start_job(dialog, app,
            event->user.windowID, job)) free_job(job);
        else if (!accept) free_job(job);
        return true;
    }
    if (event->user.code == BONGO_CAT_IMPORT_COMPLETE_CODE && app && job) {
        complete_job(dialog, app, job);
        return true;
    }
    return false;
}

void bongo_cat_preferences_import_path(BongoCatApp *app,
    SDL_Window *window, const char *path) {
    if (!app || !app->preferences || !window || !path || !path[0]) return;
    BongoCatImportJob *job = copy_path(path);
    if (!job || !start_job(app->preferences->import_dialog, app,
        SDL_GetWindowID(window), job)) free_job(job);
    else app->preferences->render_dirty = true;
}
