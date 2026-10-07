#include "runtime.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stb_image_write.h>

typedef struct CoverWriter { FILE *file; bool ok; } CoverWriter;

static void cover_write(void *context, void *data, int size) {
    CoverWriter *writer = context;
    if (writer->ok && fwrite(data, 1, (size_t)size, writer->file) != (size_t)size)
        writer->ok = false;
}

void bongo_cat_frame_capture_pending(BongoCatApp *app, int width, int height) {
    if (!app || !app->pending_model_cover_path[0]) return;
    char path[BONGO_CAT_PATH_CAP];
    snprintf(path, sizeof(path), "%s", app->pending_model_cover_path);
    app->pending_model_cover_path[0] = '\0';
    if (width < 2 || height < 2 || width > 2048 || height > 2048) return;
    size_t pitch = (size_t)width * 4, bytes = pitch * (size_t)height;
    unsigned char *pixels = malloc(bytes);
    if (!pixels) return;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char *top = pixels + (size_t)y * pitch;
        unsigned char *bottom = pixels + (size_t)(height - y - 1) * pitch;
        for (size_t i = 0; i < pitch; ++i) {
            unsigned char value = top[i]; top[i] = bottom[i]; bottom[i] = value;
        }
    }
    FILE *file = bongo_cat_file_open(path, "wb");
    CoverWriter writer = {file, file != NULL};
    bool ok = file && stbi_write_png_to_func(cover_write, &writer,
        width, height, 4, pixels, (int)pitch) && writer.ok;
    if (file && fclose(file) != 0) ok = false;
    if (!ok) bongo_cat_file_remove(path);
    free(pixels);
}

typedef struct FrameStats {
    unsigned visible;
    unsigned alpha;
} FrameStats;

static FrameStats frame_stats(const unsigned char *pixels,
    int width, int height, size_t pitch) {
    FrameStats stats = {0};
    for (int y = 0; y < height; y += 4) {
        const unsigned char *row = pixels + (size_t)y * pitch;
        for (int x = 0; x < width; x += 4) {
            const unsigned char *pixel = row + (size_t)x * 4;
            if ((unsigned)pixel[0] + pixel[1] + pixel[2] > 60) stats.visible++;
            if (pixel[3] > 8) stats.alpha++;
        }
    }
    return stats;
}

static void record_frame(BongoCatApp *app, const unsigned char *pixels,
    int width, int height, size_t pitch) {
    if (!app->smoke_frame_series) return;
    char path[BONGO_CAT_PATH_CAP];
    bongo_cat_path_join(path, sizeof(path), app->data_root, "frame-series.csv");
    bool header = !bongo_cat_path_is_file(path);
    FILE *file = bongo_cat_file_open(path, "ab");
    if (!file) return;
    if (header) fputs("ticks_ns,width,height,visible_pixels,alpha_pixels,"
        "scale_percent,opacity_percent,window_opacity,model_mode,"
        "model_state_consistent,selection_serial,window_config_visible,"
        "window_os_visible\n", file);
    FrameStats stats = frame_stats(pixels, width, height, pitch);
    bool model_consistent = bongo_cat_live2d_ready(app->live2d) &&
        app->loaded_model[0] &&
        strcmp(app->loaded_model, app->config.current_model) == 0;
    bool os_visible = (SDL_GetWindowFlags(app->window) & SDL_WINDOW_HIDDEN) == 0;
    fprintf(file, "%llu,%d,%d,%u,%u,%.3f,%.3f,%.5f,%s,%d,%u,%d,%d\n",
        (unsigned long long)SDL_GetTicksNS(), width, height,
        stats.visible, stats.alpha,
        app->config.window.scale_percent, app->config.window.opacity_percent,
        bongo_cat_platform_get_opacity(&app->platform),
        bongo_cat_mode_name(app->config.current_mode), model_consistent,
        app->model_selection_serial, app->config.window.visible, os_visible);
    fclose(file);
}

void bongo_cat_frame_audit(BongoCatApp *app, int width, int height) {
    if (!app || !app->smoke || width < 2 || height < 2) return;
    char path[BONGO_CAT_PATH_CAP];
    if (!app->smoke_frame_audited) {
        app->smoke_frame_audited = true;
        const int points[][2] = {{0, 0}, {width - 1, 0}, {0, height - 1},
            {width - 1, height - 1}, {width / 2, height / 2}};
        unsigned transparent = 0, opaque = 0;
        for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
            unsigned char pixel[4] = {0};
            glReadPixels(points[i][0], points[i][1], 1, 1, GL_RGBA,
                GL_UNSIGNED_BYTE, pixel);
            if (pixel[3] < 16) transparent++;
            if (pixel[3] > 239) opaque++;
        }
        bongo_cat_path_join(path, sizeof(path), app->data_root, "frame-alpha.txt");
        FILE *file = bongo_cat_file_open(path, "wb");
        if (file) {
            int sample_buffers = 0, sample_count = 0;
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &sample_buffers);
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &sample_count);
            fprintf(file, "samples=5 transparent=%u opaque=%u sample_buffers=%d "
                "sample_count=%d gl_error=%u\n", transparent, opaque,
                sample_buffers, sample_count, (unsigned)glGetError());
            fclose(file);
        }
    }
    size_t pitch = (size_t)width * 4, bytes = pitch * (size_t)height;
    unsigned char *pixels = malloc(bytes);
    if (!pixels) return;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    record_frame(app, pixels, width, height, pitch);
    uint64_t now = SDL_GetTicksNS();
    if (app->frame_audit_bmp_ns && now >= app->frame_audit_bmp_ns &&
        now - app->frame_audit_bmp_ns < 50000000ull) {
        free(pixels); return;
    }
    app->frame_audit_bmp_ns = now;
    unsigned char *row = malloc(pitch);
    if (!row) { free(pixels); return; }
    for (int y = 0; y < height / 2; ++y) {
        unsigned char *top = pixels + (size_t)y * pitch;
        unsigned char *bottom = pixels + (size_t)(height - 1 - y) * pitch;
        memcpy(row, top, pitch); memcpy(top, bottom, pitch); memcpy(bottom, row, pitch);
    }
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height,
        SDL_PIXELFORMAT_RGBA32, pixels, (int)pitch);
    bongo_cat_path_join(path, sizeof(path), app->data_root, "frame.bmp");
    if (surface) { SDL_SaveBMP(surface, path); SDL_DestroySurface(surface); }
    free(row); free(pixels);
}
