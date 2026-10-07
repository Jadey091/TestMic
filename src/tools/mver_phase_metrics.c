#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

#include "validation_image.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum { PIXEL_STEP = 4, REGION_COUNT = 4 };

typedef struct PathList {
    wchar_t **items;
    size_t count;
    size_t capacity;
} PathList;

typedef struct Frame {
    int width, height, grid_width, grid_height;
    unsigned char *bgr;
} Frame;

typedef struct Score {
    long long absolute[REGION_COUNT];
    long long samples[REGION_COUNT];
} Score;

typedef struct MotionScore {
    double dot[REGION_COUNT];
    double first_squared[REGION_COUNT];
    double second_squared[REGION_COUNT];
} MotionScore;

typedef struct Candidate {
    int offset;
    double seconds;
    int overlap;
    double similarity[REGION_COUNT];
    double motion[REGION_COUNT];
} Candidate;

static wchar_t *join_path(const wchar_t *directory, const wchar_t *name) {
    size_t a = wcslen(directory), b = wcslen(name);
    wchar_t *result = (wchar_t *)malloc((a + b + 2) * sizeof(*result));
    if (result) swprintf(result, a + b + 2, L"%ls\\%ls", directory, name);
    return result;
}

static int reserve_paths(PathList *list) {
    size_t capacity = list->capacity ? list->capacity * 2 : 32;
    wchar_t **items;
    if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(*items)) return 0;
    items = (wchar_t **)realloc(list->items, capacity * sizeof(*items));
    if (!items) return 0;
    list->items = items; list->capacity = capacity;
    return 1;
}

static void free_paths(PathList *list) {
    size_t index;
    for (index = 0; index < list->count; ++index) free(list->items[index]);
    free(list->items); memset(list, 0, sizeof(*list));
}

static int compare_paths(const void *left, const void *right) {
    return wcscmp(*(const wchar_t *const *)left, *(const wchar_t *const *)right);
}

static int collect_paths(const wchar_t *directory, PathList *list) {
    static const wchar_t *patterns[] = {L"*.png", L"*.bmp"};
    size_t pattern_index;
    for (pattern_index = 0; pattern_index < sizeof(patterns) / sizeof(patterns[0]);
        ++pattern_index) {
        WIN32_FIND_DATAW entry;
        wchar_t *pattern = join_path(directory, patterns[pattern_index]);
        HANDLE find;
        if (!pattern) return 0;
        find = FindFirstFileW(pattern, &entry); free(pattern);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            wchar_t *path;
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            path = join_path(directory, entry.cFileName);
            if (!path || (list->count == list->capacity && !reserve_paths(list))) {
                free(path); FindClose(find); return 0;
            }
            list->items[list->count++] = path;
        } while (FindNextFileW(find, &entry));
        FindClose(find);
    }
    qsort(list->items, list->count, sizeof(*list->items), compare_paths);
    return list->count != 0;
}

static int load_frame(const wchar_t *path, Frame *frame) {
    BongoCatValidationImage image = {0};
    int gx, gy;
    if (!bongo_cat_validation_image_load(path, &image)) return 0;
    frame->width = image.width; frame->height = image.height;
    frame->grid_width = (image.width + PIXEL_STEP - 1) / PIXEL_STEP;
    frame->grid_height = (image.height + PIXEL_STEP - 1) / PIXEL_STEP;
    frame->bgr = (unsigned char *)malloc((size_t)frame->grid_width *
        frame->grid_height * 3);
    if (!frame->bgr) { bongo_cat_validation_image_free(&image); return 0; }
    for (gy = 0; gy < frame->grid_height; ++gy)
    for (gx = 0; gx < frame->grid_width; ++gx) {
        int sx = min(image.width - 1, gx * PIXEL_STEP);
        int sy = min(image.height - 1, gy * PIXEL_STEP);
        const unsigned char *source = image.bgra + ((size_t)sy * image.width + sx) * 4;
        unsigned char *output = frame->bgr +
            ((size_t)gy * frame->grid_width + gx) * 3;
        memcpy(output, source, 3);
    }
    bongo_cat_validation_image_free(&image);
    return 1;
}

static void free_frames(Frame *frames, size_t count) {
    size_t index;
    for (index = 0; index < count; ++index) free(frames[index].bgr);
    free(frames);
}

static Frame *load_all(const PathList *paths) {
    Frame *frames = (Frame *)calloc(paths->count, sizeof(*frames));
    size_t index;
    if (!frames) return NULL;
    for (index = 0; index < paths->count; ++index) {
        if (!load_frame(paths->items[index], &frames[index]) || (index &&
            (frames[index].width != frames[0].width ||
                frames[index].height != frames[0].height))) {
            free_frames(frames, paths->count); return NULL;
        }
    }
    return frames;
}

static int in_region(int region, int x, int y, int width, int height) {
    double nx = x / (double)width, ny = y / (double)height;
    if (region == 0) return 1;
    if (region == 1) return nx >= .27 && nx <= .74 && ny >= .24 && ny <= .67;
    if (region == 2) return ny <= .36 || ((nx <= .30 || nx >= .71) && ny <= .70);
    return nx >= .16 && nx <= .58 && ny >= .58 && ny <= .92;
}

static void add_pair(Score *score, const Frame *first, const Frame *second,
    int spatial_stride) {
    int gx, gy, region;
    for (gy = 0; gy < first->grid_height; gy += spatial_stride)
    for (gx = 0; gx < first->grid_width; gx += spatial_stride) {
        const unsigned char *a = first->bgr +
            ((size_t)gy * first->grid_width + gx) * 3;
        const unsigned char *b = second->bgr +
            ((size_t)gy * second->grid_width + gx) * 3;
        int first_max = max(a[0], max(a[1], a[2]));
        int second_max = max(b[0], max(b[1], b[2]));
        int absolute, x, y;
        if (first_max <= 4 && second_max <= 4) continue;
        absolute = abs((int)a[0] - b[0]) + abs((int)a[1] - b[1]) +
            abs((int)a[2] - b[2]);
        x = gx * PIXEL_STEP; y = gy * PIXEL_STEP;
        for (region = 0; region < REGION_COUNT; ++region) if (in_region(region,
            x, y, first->width, first->height)) {
            score->absolute[region] += absolute;
            ++score->samples[region];
        }
    }
}

static void add_motion_pair(MotionScore *score, const Frame *first_previous,
    const Frame *first, const Frame *second_previous, const Frame *second,
    int spatial_stride) {
    int gx, gy, channel, region;
    for (gy = 0; gy < first->grid_height; gy += spatial_stride)
    for (gx = 0; gx < first->grid_width; gx += spatial_stride) {
        size_t offset = ((size_t)gy * first->grid_width + gx) * 3;
        int x = gx * PIXEL_STEP, y = gy * PIXEL_STEP;
        for (channel = 0; channel < 3; ++channel) {
            double first_delta = first->bgr[offset + channel] -
                first_previous->bgr[offset + channel];
            double second_delta = second->bgr[offset + channel] -
                second_previous->bgr[offset + channel];
            if (first_delta == 0.0 && second_delta == 0.0) continue;
            for (region = 0; region < REGION_COUNT; ++region) if (in_region(
                region, x, y, first->width, first->height)) {
                score->dot[region] += first_delta * second_delta;
                score->first_squared[region] += first_delta * first_delta;
                score->second_squared[region] += second_delta * second_delta;
            }
        }
    }
}

static double similarity(const Score *score, int region) {
    return score->samples[region] == 0 ? 0.0 : 100.0 * (1.0 -
        score->absolute[region] / (score->samples[region] * 3.0 * 255.0));
}

static double motion_similarity(const MotionScore *score, int region) {
    double denominator = sqrt(score->first_squared[region] *
        score->second_squared[region]);
    double correlation;
    if (denominator <= 0.0) return 0.0;
    correlation = score->dot[region] / denominator;
    correlation = max(-1.0, min(1.0, correlation));
    return 50.0 * (correlation + 1.0);
}

static Candidate measure(const Frame *mver, size_t mver_count,
    const Frame *native, size_t native_count, int offset,
    int interval_milliseconds, int spatial_stride) {
    int mver_start = max(0, -offset), native_start = max(0, offset);
    int overlap = min((int)mver_count - mver_start,
        (int)native_count - native_start);
    Score score = {{0}, {0}};
    MotionScore motion = {{0}, {0}, {0}};
    Candidate candidate;
    int frame, region;
    memset(&candidate, 0, sizeof(candidate));
    for (frame = 0; frame < overlap; frame += 2)
        add_pair(&score, &mver[mver_start + frame],
            &native[native_start + frame], spatial_stride);
    for (frame = 1; frame < overlap; ++frame)
        add_motion_pair(&motion, &mver[mver_start + frame - 1],
            &mver[mver_start + frame], &native[native_start + frame - 1],
            &native[native_start + frame], spatial_stride);
    candidate.offset = offset;
    candidate.seconds = offset * interval_milliseconds / 1000.0;
    candidate.overlap = overlap;
    for (region = 0; region < REGION_COUNT; ++region) {
        candidate.similarity[region] = similarity(&score, region);
        candidate.motion[region] = motion_similarity(&motion, region);
    }
    return candidate;
}

static int compare_candidates(const void *left, const void *right) {
    const Candidate *a = (const Candidate *)left, *b = (const Candidate *)right;
    if (a->motion[0] < b->motion[0]) return 1;
    if (a->motion[0] > b->motion[0]) return -1;
    return a->offset < b->offset ? -1 : a->offset > b->offset;
}

static void print_candidate(const Candidate *candidate, int comma) {
    printf("  {\"NativeOffsetFrames\":%d,\"NativeOffsetSeconds\":%.17g,"
        "\"OverlapFrames\":%d,\"FullSimilarity\":%.17g,"
        "\"FaceSimilarity\":%.17g,\"HairSimilarity\":%.17g,"
        "\"HandSimilarity\":%.17g,\"FullMotionSimilarity\":%.17g,"
        "\"FaceMotionSimilarity\":%.17g,\"HairMotionSimilarity\":%.17g,"
        "\"HandMotionSimilarity\":%.17g}%s\n", candidate->offset,
        candidate->seconds, candidate->overlap, candidate->similarity[0],
        candidate->similarity[1], candidate->similarity[2],
        candidate->similarity[3], candidate->motion[0], candidate->motion[1],
        candidate->motion[2], candidate->motion[3], comma ? "," : "");
}

int wmain(int argc, wchar_t **argv) {
    PathList mver_paths = {0}, native_paths = {0};
    Frame *mver = NULL, *native = NULL;
    Candidate *coarse = NULL, *fine = NULL;
    size_t coarse_count = 0, fine_count = 0, index;
    int max_lag, minimum_overlap, interval_ms, offset, result = 1;
    int selected_offsets[8], selected_count = 0;
    if (argc < 3 || argc > 6) {
        fwprintf(stderr, L"usage: mver-phase-metrics MVER_DIRECTORY "
            L"NATIVE_DIRECTORY [max-lag-frames] [minimum-overlap-frames] "
            L"[interval-ms]\n");
        return 2;
    }
    setlocale(LC_NUMERIC, "C");
    max_lag = argc > 3 ? _wtoi(argv[3]) : 30;
    minimum_overlap = argc > 4 ? _wtoi(argv[4]) : 2;
    interval_ms = argc > 5 ? _wtoi(argv[5]) : 17;
    if (max_lag < 0 || minimum_overlap < 2) return 2;
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) return 1;
    if (!collect_paths(argv[1], &mver_paths) || !collect_paths(argv[2], &native_paths) ||
        mver_paths.count < (size_t)minimum_overlap ||
        native_paths.count < (size_t)minimum_overlap) goto done;
    mver = load_all(&mver_paths); native = load_all(&native_paths);
    if (!mver || !native) goto done;
    coarse = (Candidate *)malloc((size_t)(max_lag * 2 + 1) * sizeof(*coarse));
    fine = (Candidate *)malloc(8 * sizeof(*fine));
    if (!coarse || !fine) goto done;
    for (offset = -max_lag; offset <= max_lag; ++offset) {
        Candidate candidate = measure(mver, mver_paths.count, native,
            native_paths.count, offset, interval_ms, 3);
        if (candidate.overlap >= minimum_overlap) coarse[coarse_count++] = candidate;
    }
    if (!coarse_count) goto done;
    qsort(coarse, coarse_count, sizeof(*coarse), compare_candidates);
    for (index = 0; index < min((size_t)7, coarse_count); ++index)
        selected_offsets[selected_count++] = coarse[index].offset;
    {
        int found_zero = 0;
        for (index = 0; index < (size_t)selected_count; ++index)
            if (selected_offsets[index] == 0) found_zero = 1;
        if (!found_zero) selected_offsets[selected_count++] = 0;
    }
    for (index = 0; index < (size_t)selected_count; ++index) {
        Candidate candidate = measure(mver, mver_paths.count, native,
            native_paths.count, selected_offsets[index], interval_ms, 1);
        if (candidate.overlap >= minimum_overlap) fine[fine_count++] = candidate;
    }
    qsort(fine, fine_count, sizeof(*fine), compare_candidates);
    printf("[\n");
    for (index = 0; index < fine_count; ++index)
        print_candidate(&fine[index], index + 1 < fine_count);
    printf("]\n");
    result = 0;
done:
    free(coarse); free(fine);
    free_frames(mver, mver_paths.count); free_frames(native, native_paths.count);
    free_paths(&mver_paths); free_paths(&native_paths);
    CoUninitialize();
    if (result) fwprintf(stderr, L"phase analysis failed\n");
    return result;
}
