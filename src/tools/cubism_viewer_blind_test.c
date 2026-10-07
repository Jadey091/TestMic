#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

#include "validation_image.h"
#include "windows_tool.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum { CANVAS = 800, MARGIN = 40 };

typedef struct Frame {
    wchar_t *name;
    wchar_t *viewer_path;
    wchar_t *native_path;
    BongoCatValidationImage viewer;
    BongoCatValidationImage native;
} Frame;

typedef struct FrameList {
    Frame *items;
    size_t count;
    size_t capacity;
} FrameList;

typedef struct Region {
    const char *name;
    int left, top, right, bottom;
} Region;

typedef struct DotNetRandom {
    int seed[56];
    int inext;
    int inextp;
} DotNetRandom;

static wchar_t *join_path(const wchar_t *directory, const wchar_t *name) {
    size_t a = wcslen(directory), b = wcslen(name);
    wchar_t *result = (wchar_t *)malloc((a + b + 2) * sizeof(*result));
    if (!result) return NULL;
    memcpy(result, directory, a * sizeof(*result));
    result[a] = L'\\';
    memcpy(result + a + 1, name, (b + 1) * sizeof(*result));
    return result;
}

static wchar_t *join_suffix(const wchar_t *directory, const wchar_t *name,
    const wchar_t *suffix) {
    size_t a = wcslen(directory), b = wcslen(name), c = wcslen(suffix);
    wchar_t *result = (wchar_t *)malloc((a + b + c + 3) * sizeof(*result));
    if (!result) return NULL;
    swprintf(result, a + b + c + 3, L"%ls\\%ls%ls", directory, name, suffix);
    return result;
}

static int reserve_frames(FrameList *list) {
    size_t capacity = list->capacity ? list->capacity * 2 : 32;
    Frame *items;
    if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(*items)) return 0;
    items = (Frame *)realloc(list->items, capacity * sizeof(*items));
    if (!items) return 0;
    list->items = items; list->capacity = capacity;
    return 1;
}

static void free_frames(FrameList *list) {
    size_t index;
    for (index = 0; index < list->count; ++index) {
        free(list->items[index].name);
        free(list->items[index].viewer_path);
        free(list->items[index].native_path);
        bongo_cat_validation_image_free(&list->items[index].viewer);
        bongo_cat_validation_image_free(&list->items[index].native);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static wchar_t *without_extension(const wchar_t *name) {
    const wchar_t *dot = wcsrchr(name, L'.');
    size_t length = dot ? (size_t)(dot - name) : wcslen(name);
    wchar_t *result = (wchar_t *)malloc((length + 1) * sizeof(*result));
    if (result) { memcpy(result, name, length * sizeof(*result)); result[length] = L'\0'; }
    return result;
}

static wchar_t *find_native(const wchar_t *directory, const wchar_t *name) {
    static const wchar_t *extensions[] = {L".bmp", L".png"};
    size_t index;
    for (index = 0; index < sizeof(extensions) / sizeof(extensions[0]); ++index) {
        wchar_t *path = join_suffix(directory, name, extensions[index]);
        if (!path) return NULL;
        if (_waccess(path, 0) == 0) return path;
        free(path);
    }
    return NULL;
}

static int compare_frames(const void *left, const void *right) {
    const Frame *a = (const Frame *)left, *b = (const Frame *)right;
    return wcscmp(a->name, b->name);
}

static int collect_frames(const wchar_t *viewer_directory,
    const wchar_t *native_directory, FrameList *list) {
    WIN32_FIND_DATAW entry;
    HANDLE find;
    wchar_t *pattern;
    static const wchar_t *extensions[] = {L"*.png", L"*.bmp"};
    size_t extension;
    for (extension = 0; extension < sizeof(extensions) / sizeof(extensions[0]); ++extension) {
        pattern = join_path(viewer_directory, extensions[extension]);
        if (!pattern) return 0;
        find = FindFirstFileW(pattern, &entry);
        free(pattern);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            wchar_t *name, *viewer_path, *native_path;
            Frame *frame;
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            name = without_extension(entry.cFileName);
            viewer_path = join_path(viewer_directory, entry.cFileName);
            native_path = name ? find_native(native_directory, name) : NULL;
            if (!name || !viewer_path) {
                free(name); free(viewer_path); free(native_path);
                FindClose(find); return 0;
            }
            if (!native_path) { free(name); free(viewer_path); continue; }
            if (list->count == list->capacity && !reserve_frames(list)) {
                free(name); free(viewer_path); free(native_path);
                FindClose(find); return 0;
            }
            frame = &list->items[list->count++];
            memset(frame, 0, sizeof(*frame));
            frame->name = name; frame->viewer_path = viewer_path;
            frame->native_path = native_path;
        } while (FindNextFileW(find, &entry));
        FindClose(find);
    }
    qsort(list->items, list->count, sizeof(*list->items), compare_frames);
    return list->count != 0;
}

static int foreground_pixel(const unsigned char *pixel) {
    return min(pixel[0], min(pixel[1], pixel[2])) < 237;
}

static int write_metrics(FILE *writer, const Frame *frame, const Region *region,
    const BongoCatValidationImage *viewer, const BongoCatValidationImage *native,
    const BongoCatValidationImage *viewer_base,
    const BongoCatValidationImage *native_base) {
    long long count = 0, within = 0, intersection = 0, union_count = 0;
    double absolute = 0, viewer_change = 0, native_change = 0;
    int x, y;
    for (y = region->top; y < region->bottom; y += 2)
    for (x = region->left; x < region->right; x += 2) {
        const unsigned char *a = viewer->bgra + ((size_t)y * viewer->width + x) * 4;
        const unsigned char *b = native->bgra + ((size_t)y * native->width + x) * 4;
        const unsigned char *av = viewer_base->bgra +
            ((size_t)y * viewer_base->width + x) * 4;
        const unsigned char *bn = native_base->bgra +
            ((size_t)y * native_base->width + x) * 4;
        int first = foreground_pixel(a), second = foreground_pixel(b);
        int maximum = 0, channel;
        if (first || second) ++union_count;
        if (first && second) ++intersection;
        if (!first && !second) continue;
        for (channel = 0; channel < 3; ++channel) {
            int delta = abs((int)a[channel] - b[channel]);
            maximum = max(maximum, delta); absolute += delta;
            viewer_change += abs((int)a[channel] - av[channel]);
            native_change += abs((int)b[channel] - bn[channel]);
        }
        if (maximum <= 8) ++within;
        ++count;
    }
    {
        double similarity = count == 0 ? 100.0 : 100.0 *
            (1.0 - absolute / (count * 3.0 * 255.0));
        double near_value = count == 0 ? 100.0 : 100.0 * within / count;
        double iou = union_count == 0 ? 100.0 : 100.0 * intersection / union_count;
        double viewer_ratio = count == 0 ? 0.0 : viewer_change /
            (count * 3.0 * 255.0);
        double native_ratio = count == 0 ? 0.0 : native_change /
            (count * 3.0 * 255.0);
        double ratio = viewer_ratio < 0.000001 ?
            (native_ratio < 0.000001 ? 1.0 : 999.0) : native_ratio / viewer_ratio;
        return fprintf(writer, "%ls,%s,%.4f,%.4f,%.4f,%.6f,%.6f,%.4f,%lld\n",
            frame->name, region->name, similarity, near_value, iou, viewer_ratio,
            native_ratio, ratio, count) >= 0;
    }
}

static int write_ballot(const wchar_t *path, const Frame *frame, int viewer_first) {
    BongoCatValidationImage ballot = {0};
    const BongoCatValidationImage *first = viewer_first ? &frame->viewer : &frame->native;
    const BongoCatValidationImage *second = viewer_first ? &frame->native : &frame->viewer;
    int y, x;
    ballot.width = CANVAS * 2 + 30; ballot.height = CANVAS + 60;
    ballot.bgra = (unsigned char *)malloc((size_t)ballot.width * ballot.height * 4);
    if (!ballot.bgra) return 0;
    for (y = 0; y < ballot.height; ++y) for (x = 0; x < ballot.width; ++x) {
        unsigned char *pixel = ballot.bgra + ((size_t)y * ballot.width + x) * 4;
        pixel[0] = pixel[1] = pixel[2] = 238; pixel[3] = 255;
    }
    for (y = 0; y < CANVAS; ++y) {
        memcpy(ballot.bgra + ((size_t)(y + 60) * ballot.width) * 4,
            first->bgra + (size_t)y * CANVAS * 4, CANVAS * 4);
        memcpy(ballot.bgra + ((size_t)(y + 60) * ballot.width + CANVAS + 30) * 4,
            second->bgra + (size_t)y * CANVAS * 4, CANVAS * 4);
    }
    if (!bongo_cat_validation_image_draw_label(&ballot, L"A",
            CANVAS / 2.0f - 10.0f, 15.0f) ||
        !bongo_cat_validation_image_draw_label(&ballot, L"B",
            CANVAS + 30.0f + CANVAS / 2.0f - 10.0f, 15.0f)) {
        bongo_cat_validation_image_free(&ballot); return 0;
    }
    x = bongo_cat_validation_image_save(path, &ballot, 1);
    bongo_cat_validation_image_free(&ballot);
    return x;
}

static void random_init(DotNetRandom *random, int seed) {
    int subtraction = seed == INT_MIN ? INT_MAX : abs(seed);
    int mj = 161803398 - subtraction, mk = 1, i, ii, k;
    memset(random, 0, sizeof(*random));
    random->seed[55] = mj;
    for (i = 1; i < 55; ++i) {
        ii = (21 * i) % 55; random->seed[ii] = mk;
        mk = mj - mk; if (mk < 0) mk += INT_MAX; mj = random->seed[ii];
    }
    for (k = 1; k < 5; ++k) for (i = 1; i < 56; ++i) {
        int n = 1 + (i + 30) % 55;
        random->seed[i] -= random->seed[n];
        if (random->seed[i] < 0) random->seed[i] += INT_MAX;
    }
    random->inext = 0; random->inextp = 21;
}

static int random_next_two(DotNetRandom *random) {
    int loc, locp, value;
    loc = random->inext + 1; if (loc >= 56) loc = 1; random->inext = loc;
    locp = random->inextp + 1; if (locp >= 56) locp = 1; random->inextp = locp;
    value = random->seed[loc] - random->seed[locp];
    if (value == INT_MAX) --value;
    if (value < 0) value += INT_MAX;
    random->seed[loc] = value;
    return (int)((value / (double)INT_MAX) * 2.0);
}

static int write_summary(const wchar_t *path, int seed, size_t frames) {
    FILE *file = _wfopen(path, L"wb");
    if (!file) return 0;
    fprintf(file, "{\n  \"seed\": %d,\n  \"frames\": %zu,\n  \"canvas\": 800\n}\n",
        seed, frames);
    return fclose(file) == 0;
}

int wmain(int argc, wchar_t **argv) {
    FrameList frames = {0};
    wchar_t *normalized = NULL, *ballots = NULL, *metrics_path = NULL;
    wchar_t *key_path = NULL, *summary_path = NULL;
    BongoCatValidationRect viewer_bounds, native_bounds;
    size_t baseline_index = 0, index;
    int seed, result = 1;
    const Region regions[] = {
        {"full", 0, 0, CANVAS, CANVAS},
        {"face", 225, 170, 575, 455},
        {"hair", 70, 40, 730, 495},
        {"hands", 55, 395, 745, 710}
    };
    setlocale(LC_NUMERIC, "C");
    if (argc < 4) {
        fwprintf(stderr, L"usage: blind-test.exe viewer native output [seed]\n");
        return 2;
    }
    seed = argc > 4 ? _wtoi(argv[4]) : 20260807;
    if (seed == 0) seed = (int)(GetTickCount() & INT_MAX);
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) return 1;
    if (!bongo_cat_tool_ensure_directory(argv[3]) ||
        !collect_frames(argv[1], argv[2], &frames))
        goto done;
    normalized = join_path(argv[3], L"normalized");
    ballots = join_path(argv[3], L"ballots");
    metrics_path = join_path(argv[3], L"metrics.csv");
    key_path = join_path(argv[3], L"answer-key.csv");
    summary_path = join_path(argv[3], L"summary.json");
    if (!normalized || !ballots || !metrics_path || !key_path || !summary_path ||
        !bongo_cat_tool_ensure_directory(normalized) ||
        !bongo_cat_tool_ensure_directory(ballots)) goto done;
    for (index = 0; index < frames.count; ++index)
        if (wcscmp(frames.items[index].name, L"track-000") == 0) baseline_index = index;
    {
        wchar_t *idle = join_suffix(argv[1], L"idle", L".png");
        int have_idle = idle && _waccess(idle, 0) == 0;
        wchar_t *viewer_bounds_path = have_idle ? idle : frames.items[baseline_index].viewer_path;
        BongoCatValidationImage bounds_image = {0};
        if (!bongo_cat_validation_image_load(viewer_bounds_path, &bounds_image) ||
            !bongo_cat_validation_image_bounds(&bounds_image, &viewer_bounds)) {
            bongo_cat_validation_image_free(&bounds_image); free(idle); goto done;
        }
        bongo_cat_validation_image_free(&bounds_image);
        if (!bongo_cat_validation_image_load(frames.items[baseline_index].native_path,
                &bounds_image) || !bongo_cat_validation_image_bounds(&bounds_image,
                &native_bounds)) {
            bongo_cat_validation_image_free(&bounds_image); free(idle); goto done;
        }
        bongo_cat_validation_image_free(&bounds_image); free(idle);
    }
    for (index = 0; index < frames.count; ++index) {
        wchar_t *viewer_output, *native_output;
        if (!bongo_cat_validation_image_normalize_file(
                frames.items[index].viewer_path, viewer_bounds,
                &frames.items[index].viewer) ||
            !bongo_cat_validation_image_normalize_file(
                frames.items[index].native_path, native_bounds,
                &frames.items[index].native) ||
            !bongo_cat_validation_image_keep_largest(&frames.items[index].viewer) ||
            !bongo_cat_validation_image_keep_largest(&frames.items[index].native)) {
            goto done;
        }
        viewer_output = join_suffix(normalized, frames.items[index].name, L"-viewer.png");
        native_output = join_suffix(normalized, frames.items[index].name, L"-native.png");
        if (!viewer_output || !native_output ||
            !bongo_cat_validation_image_save(viewer_output, &frames.items[index].viewer, 1) ||
            !bongo_cat_validation_image_save(native_output, &frames.items[index].native, 1)) {
            free(viewer_output); free(native_output); goto done;
        }
        free(viewer_output); free(native_output);
    }
    {
        FILE *metrics = _wfopen(metrics_path, L"w");
        FILE *key = _wfopen(key_path, L"w");
        DotNetRandom random;
        if (!metrics || !key) { if (metrics) fclose(metrics); if (key) fclose(key); goto done; }
        fprintf(metrics, "frame,region,similarity,within8,foreground_iou,viewer_change,native_change,response_ratio,samples\n");
        fprintf(key, "frame,A,B\n");
        random_init(&random, seed);
        for (index = 0; index < frames.count; ++index) {
            size_t region;
            for (region = 0; region < sizeof(regions) / sizeof(regions[0]); ++region)
                if (!write_metrics(metrics, &frames.items[index], &regions[region],
                    &frames.items[index].viewer, &frames.items[index].native,
                    &frames.items[baseline_index].viewer, &frames.items[baseline_index].native)) {
                    fclose(metrics); fclose(key); goto done;
                }
            {
                int viewer_first = random_next_two(&random) == 0;
                wchar_t *ballot = join_suffix(ballots, frames.items[index].name, L".png");
                if (!ballot || !write_ballot(ballot, &frames.items[index], viewer_first)) {
                    free(ballot); fclose(metrics); fclose(key); goto done;
                }
                free(ballot);
                fprintf(key, "%ls,%s\n", frames.items[index].name,
                    viewer_first ? "Viewer,Native" : "Native,Viewer");
            }
        }
        fclose(metrics); fclose(key);
    }
    if (!write_summary(summary_path, seed, frames.count)) goto done;
    wprintf(L"%ls\n", argv[3]);
    result = 0;
done:
    free(normalized); free(ballots); free(metrics_path); free(key_path); free(summary_path);
    free_frames(&frames);
    CoUninitialize();
    if (result) fwprintf(stderr, L"blind test failed\n");
    return result;
}
