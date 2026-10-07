#include "model_import.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTABLE_CHILD_CAP 64
#define PORTABLE_ENTRY_CAP 256
#define PORTABLE_NODE_CAP 256
#define PORTABLE_SCAN_DEPTH 3
#define PORTABLE_SCAN_BUDGET_NS 2000000000ull

typedef struct ChildList {
    char paths[PORTABLE_CHILD_CAP][BONGO_CAT_PATH_CAP];
    size_t count, entries;
    bool limited;
} ChildList;

typedef struct PortableWorkspace {
    BongoCatImportDiscovery discovery;
    ChildList children;
} PortableWorkspace;

typedef struct PortableScan {
    BongoCatPortableVisitor visitor;
    void *userdata;
    BongoCatError *error;
    size_t nodes;
    uint64_t deadline;
    bool limited;
} PortableScan;

typedef struct PortableQueue {
    char paths[PORTABLE_NODE_CAP][BONGO_CAT_PATH_CAP];
    unsigned char depths[PORTABLE_NODE_CAP], priorities[PORTABLE_NODE_CAP];
    size_t head, count;
} PortableQueue;

typedef struct ModelHint { size_t entries; bool found; } ModelHint;

static BongoCatPathVisit collect_child(void *userdata,
    const char *dirname, const char *name) {
    ChildList *list = userdata;
    if (++list->entries > PORTABLE_ENTRY_CAP || list->count >= PORTABLE_CHILD_CAP) {
        list->limited = true; return BONGO_CAT_PATH_SUCCESS;
    }
    if (name[0] == '.') return BONGO_CAT_PATH_CONTINUE;
    char path[BONGO_CAT_PATH_CAP];
    if (bongo_cat_path_join(path, sizeof(path), dirname, name) &&
        bongo_cat_path_is_dir(path))
        snprintf(list->paths[list->count++], BONGO_CAT_PATH_CAP, "%s", path);
    return BONGO_CAT_PATH_CONTINUE;
}

static int compare_path(const void *left, const void *right) {
#ifdef _WIN32
    return SDL_strcasecmp(left, right);
#else
    return strcmp(left, right);
#endif
}

static bool within_budget(PortableScan *scan) {
    if (scan->nodes >= PORTABLE_NODE_CAP || SDL_GetTicksNS() >= scan->deadline) {
        scan->limited = true; return false;
    }
    scan->nodes++; return true;
}

static bool direct_image_root(const char *path) {
    char image[BONGO_CAT_PATH_CAP];
    return bongo_cat_path_join(image, sizeof(image), path, "img") &&
        bongo_cat_path_is_dir(image);
}

static BongoCatPathVisit find_model_hint(void *userdata,
    const char *dirname, const char *name) {
    ModelHint *hint = userdata;
    if (++hint->entries > PORTABLE_CHILD_CAP) return BONGO_CAT_PATH_SUCCESS;
    char path[BONGO_CAT_PATH_CAP];
    if (name[0] != '.' && bongo_cat_path_join(path, sizeof(path), dirname, name) &&
        bongo_cat_path_is_dir(path) && direct_image_root(path)) {
        hint->found = true; return BONGO_CAT_PATH_SUCCESS;
    }
    return BONGO_CAT_PATH_CONTINUE;
}

static bool model_hint(const char *path) {
    if (direct_image_root(path)) return true;
    ModelHint hint = {0};
    bongo_cat_path_enumerate(path, find_model_hint, &hint);
    return hint.found;
}

static void enqueue_children(PortableScan *scan, PortableQueue *queue,
    const ChildList *children, int depth) {
    for (size_t i = 0; i < children->count; ++i) {
        if (queue->count >= PORTABLE_NODE_CAP) {
            scan->limited = true; return;
        }
        snprintf(queue->paths[queue->count], BONGO_CAT_PATH_CAP, "%s",
            children->paths[i]);
        queue->depths[queue->count++] = (unsigned char)depth;
        queue->priorities[queue->count - 1] = model_hint(children->paths[i]);
    }
}

static void prioritize_next(PortableQueue *queue) {
    size_t preferred = queue->head;
    while (preferred < queue->count && !queue->priorities[preferred]) preferred++;
    if (preferred >= queue->count || preferred == queue->head) return;
    char path[BONGO_CAT_PATH_CAP];
    snprintf(path, sizeof(path), "%s", queue->paths[queue->head]);
    memmove(queue->paths[queue->head], queue->paths[preferred], BONGO_CAT_PATH_CAP);
    memmove(queue->paths[preferred], path, BONGO_CAT_PATH_CAP);
    unsigned char depth = queue->depths[queue->head];
    queue->depths[queue->head] = queue->depths[preferred];
    queue->depths[preferred] = depth;
    queue->priorities[preferred] = queue->priorities[queue->head];
    queue->priorities[queue->head] = 1;
}

static BongoCatResult scan_node(PortableScan *scan, PortableQueue *queue,
    const char *source, int depth) {
    if (!within_budget(scan)) return BONGO_CAT_OK;
    PortableWorkspace *work = calloc(1, sizeof(*work));
    if (!work) {
        bongo_cat_error_set(scan->error, BONGO_CAT_ERROR_MEMORY,
            "Cannot allocate portable model scan workspace");
        return BONGO_CAT_ERROR_MEMORY;
    }
    BongoCatError local = {0};
    int found = bongo_cat_import_mver_discover_exact(source, &work->discovery, &local);
    if (found <= 0) {
        memset(&work->discovery, 0, sizeof(work->discovery)); local = (BongoCatError){0};
        found = bongo_cat_import_mver_patch_discover_exact(source,
            &work->discovery, &local);
    }
    if (found > 0) {
        uint64_t started = SDL_GetTicksNS();
        BongoCatResult result = scan->visitor(scan->userdata, source,
            &work->discovery, scan->error);
        uint64_t finished = SDL_GetTicksNS(), elapsed = finished - started;
        if (scan->deadline <= UINT64_MAX - elapsed) scan->deadline += elapsed;
        else scan->deadline = UINT64_MAX;
        free(work); return result;
    }
    if (depth < PORTABLE_SCAN_DEPTH &&
        bongo_cat_path_enumerate(source, collect_child, &work->children)) {
        if (work->children.limited) scan->limited = true;
        qsort(work->children.paths, work->children.count,
            sizeof(work->children.paths[0]), compare_path);
        enqueue_children(scan, queue, &work->children, depth + 1);
    }
    free(work); return BONGO_CAT_OK;
}

BongoCatResult bongo_cat_import_portable_scan(const char *root,
    BongoCatPortableVisitor visitor, void *userdata, BongoCatError *error) {
    if (!root || !visitor) return BONGO_CAT_ERROR_ARGUMENT;
    PortableWorkspace *work = calloc(1, sizeof(*work));
    PortableQueue *queue = calloc(1, sizeof(*queue));
    if (!work || !queue) { free(work); free(queue); return BONGO_CAT_ERROR_MEMORY; }
    PortableScan scan = {visitor, userdata, error, 0,
        SDL_GetTicksNS() + PORTABLE_SCAN_BUDGET_NS, false};
    BongoCatResult result = BONGO_CAT_OK;
    if (bongo_cat_path_enumerate(root, collect_child, &work->children)) {
        if (work->children.limited) scan.limited = true;
        qsort(work->children.paths, work->children.count,
            sizeof(work->children.paths[0]), compare_path);
        enqueue_children(&scan, queue, &work->children, 1);
        while (queue->head < queue->count) {
            prioritize_next(queue);
            size_t current = queue->head++;
            BongoCatResult item = scan_node(&scan, queue,
                queue->paths[current], queue->depths[current]);
            if (item != BONGO_CAT_OK && result == BONGO_CAT_OK) result = item;
        }
    }
    if (scan.limited) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
        "Portable model discovery reached its startup scan budget");
    free(queue); free(work); return result;
}
