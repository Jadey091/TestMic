#include "bongo_cat/file.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/image.h"

#ifdef _WIN32
#define COBJMACROS
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stb_image.h>
#include <stdlib.h>
#include <string.h>

#define BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT 2048

#ifdef _WIN32
static bool needs_wic_scaling(const char *path, int limit) {
    FILE *file = bongo_cat_file_open(path, "rb");
    int width = 0, height = 0, channels = 0;
    bool known = file && stbi_info_from_file(file, &width, &height, &channels);
    if (file) fclose(file);
    return !known || width > limit || height > limit;
}

static wchar_t *wide_path(const char *path) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, NULL, 0);
    wchar_t *wide = count > 0 ? malloc((size_t)count * sizeof(*wide)) : NULL;
    if (wide && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, wide, count)) {
        free(wide); return NULL;
    }
    return wide;
}

static bool wic_scaled_image(const char *path, BongoCatImage *image,
    UINT max_width, UINT max_height) {
    memset(image, 0, sizeof(*image));
    HRESULT initialized = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool uninitialize = initialized == S_OK || initialized == S_FALSE;
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapScaler *scaler = NULL;
    IWICFormatConverter *converter = NULL;
    wchar_t *wide = wide_path(path);
    HRESULT result = wide ? CoCreateInstance(&CLSID_WICImagingFactory, NULL,
        CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory) : E_FAIL;
    if (SUCCEEDED(result)) result = IWICImagingFactory_CreateDecoderFromFilename(factory,
        wide, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(result)) result = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    UINT source_width = 0, source_height = 0;
    if (SUCCEEDED(result)) result = IWICBitmapFrameDecode_GetSize(frame,
        &source_width, &source_height);
    UINT target_width = source_width, target_height = source_height;
    if (max_width && max_height &&
        (source_width > max_width || source_height > max_height)) {
        if ((uint64_t)max_width * source_height <=
            (uint64_t)max_height * source_width) {
            target_width = max_width;
            target_height = (UINT)((uint64_t)source_height * max_width /
                source_width);
        } else {
            target_height = max_height;
            target_width = (UINT)((uint64_t)source_width * max_height /
                source_height);
        }
        if (!target_width) target_width = 1;
        if (!target_height) target_height = 1;
        if (SUCCEEDED(result)) result = IWICImagingFactory_CreateBitmapScaler(factory, &scaler);
        if (SUCCEEDED(result)) result = IWICBitmapScaler_Initialize(scaler,
            (IWICBitmapSource *)frame, target_width, target_height,
            WICBitmapInterpolationModeFant);
    }
    IWICBitmapSource *source = scaler ? (IWICBitmapSource *)scaler :
        (IWICBitmapSource *)frame;
    if (SUCCEEDED(result))
        result = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (SUCCEEDED(result)) result = IWICFormatConverter_Initialize(converter, source,
        &GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0,
        WICBitmapPaletteTypeCustom);
    size_t stride = (size_t)target_width * 4;
    size_t bytes = stride * target_height;
    unsigned char *pixels = bytes <= UINT_MAX ? malloc(bytes) : NULL;
    if (SUCCEEDED(result) && pixels) result = IWICFormatConverter_CopyPixels(converter,
        NULL, (UINT)stride, (UINT)bytes, pixels);
    bool ok = SUCCEEDED(result) && pixels;
    if (ok) {
        image->pixels = pixels;
        image->width = (int)target_width;
        image->height = (int)target_height;
    } else free(pixels);
    if (converter) IWICFormatConverter_Release(converter);
    if (scaler) IWICBitmapScaler_Release(scaler);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);
    free(wide);
    if (uninitialize) CoUninitialize();
    return ok;
}
#endif

BongoCatResult bongo_cat_image_load(const char *path, BongoCatImage *image, BongoCatError *error) {
    if (!path || !image) return BONGO_CAT_ERROR_ARGUMENT;
    memset(image, 0, sizeof(*image));
    int channels;
    FILE *file = bongo_cat_file_open(path, "rb");
    image->pixels = file ? stbi_load_from_file(file, &image->width,
        &image->height, &channels, STBI_rgb_alpha) : NULL;
    image->pixels_stbi = image->pixels != NULL;
    if (file) fclose(file);
    if (!image->pixels) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot decode image: %s", path);
        return BONGO_CAT_ERROR_IO;
    }
    image->surface = SDL_CreateSurfaceFrom(image->width, image->height,
        SDL_PIXELFORMAT_RGBA32, image->pixels, image->width * 4);
    if (!image->surface) {
        bongo_cat_image_free(image);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "Cannot create image surface");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    return BONGO_CAT_OK;
}

void bongo_cat_image_free(BongoCatImage *image) {
    if (!image) return;
    if (image->surface) SDL_DestroySurface(image->surface);
    if (image->pixels) {
        if (image->pixels_stbi) stbi_image_free(image->pixels);
        else free(image->pixels);
    }
    memset(image, 0, sizeof(*image));
}

static unsigned int upload(const BongoCatImage *image, GLuint texture, bool model_texture) {
    bool created = texture == 0;
    if (created) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (created) {
        /* Pixi's default Live2D texture source uses linear filtering without
         * generated mipmaps. This keeps the same sharpness and saves texture
         * memory for large model atlases. */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (model_texture && SDL_GL_ExtensionSupported(
            "GL_EXT_texture_filter_anisotropic")) {
            GLfloat maximum = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
            if (maximum > 1.0f)
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    SDL_min(maximum, 8.0f));
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (created) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width,
        image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height,
        GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    return texture;
}

unsigned int bongo_cat_image_texture(const char *path, int *width, int *height, BongoCatError *error) {
    BongoCatImage image;
    if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    GLuint texture = upload(&image, 0, false);
    if (width) *width = image.width;
    if (height) *height = image.height;
    bongo_cat_image_free(&image);
    return texture;
}

unsigned int bongo_cat_image_texture_thumbnail(const char *path, int max_width,
    int max_height, int *width, int *height, BongoCatError *error) {
    BongoCatImage image;
#ifdef _WIN32
    if (max_width > 0 && max_height > 0 && wic_scaled_image(path, &image,
        (UINT)max_width, (UINT)max_height)) {
        GLuint texture = upload(&image, 0, false);
        if (width) *width = image.width;
        if (height) *height = image.height;
        bongo_cat_image_free(&image);
        return texture;
    }
#endif
    if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    int target_width = image.width, target_height = image.height;
    if (max_width > 0 && max_height > 0 &&
        (target_width > max_width || target_height > max_height)) {
        float scale = SDL_min((float)max_width / target_width,
            (float)max_height / target_height);
        target_width = SDL_max(1, (int)(target_width * scale + .5f));
        target_height = SDL_max(1, (int)(target_height * scale + .5f));
        SDL_Surface *scaled = SDL_ScaleSurface(image.surface, target_width,
            target_height, SDL_SCALEMODE_LINEAR);
        if (scaled) {
            BongoCatImage thumbnail = {
                .pixels = scaled->pixels, .width = scaled->w, .height = scaled->h};
            GLuint texture = upload(&thumbnail, 0, false);
            if (width) *width = thumbnail.width;
            if (height) *height = thumbnail.height;
            SDL_DestroySurface(scaled);
            bongo_cat_image_free(&image);
            return texture;
        }
        target_width = image.width;
        target_height = image.height;
    }
    GLuint texture = upload(&image, 0, false);
    if (width) *width = target_width;
    if (height) *height = target_height;
    bongo_cat_image_free(&image);
    return texture;
}
unsigned int bongo_cat_image_texture_model(const char *path, bool direct_decode,
    int *width, int *height, BongoCatImageAlphaMask *alpha, BongoCatError *error) {
    BongoCatImage image;
    GLint hardware_limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &hardware_limit);
    if (hardware_limit < 1) hardware_limit = BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT;
#ifdef _WIN32
    int decode_limit = hardware_limit;
    /* Preset atlases are verified byte-identical in WIC and stb. Keep WIC for
       custom PNG color metadata and for memory-bounded oversized decoding. */
    bool scaled = needs_wic_scaling(path, decode_limit);
    if (!direct_decode || scaled) {
        if (!wic_scaled_image(path, &image, (UINT)decode_limit,
            (UINT)decode_limit) &&
            bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    } else if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
    if (scaled) SDL_Log("Live2D texture constrained to %dx%d by a %d pixel "
        "texture limit: %s", image.width, image.height, decode_limit, path);
    else if (image.width > BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT ||
        image.height > BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT)
        SDL_Log("High-resolution Live2D texture preserved at %dx%d: %s",
            image.width, image.height, path);
#else
    (void)direct_decode;
    if (bongo_cat_image_load(path, &image, error) != BONGO_CAT_OK) return 0;
#endif
    bongo_cat_image_make_alpha_mask(&image, alpha);
    bongo_cat_gl_clear_errors();
    GLuint texture = upload(&image, 0, true);
    GLenum upload_error = glGetError();
#ifdef _WIN32
    if (upload_error == GL_OUT_OF_MEMORY &&
        (image.width > BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT ||
            image.height > BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT)) {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
        bongo_cat_image_free(&image);
        int fallback_limit = SDL_min(hardware_limit,
            BONGO_CAT_LIVE2D_EFFICIENT_TEXTURE_LIMIT);
        if (wic_scaled_image(path, &image, (UINT)fallback_limit,
            (UINT)fallback_limit)) {
            bongo_cat_image_make_alpha_mask(&image, alpha);
            bongo_cat_gl_clear_errors();
            texture = upload(&image, 0, true);
            upload_error = glGetError();
            if (upload_error == GL_NO_ERROR) SDL_LogWarn(
                SDL_LOG_CATEGORY_RENDER,
                "Live2D texture fell back to %dx%d after full-resolution "
                "GPU upload exhausted memory: %s", image.width, image.height, path);
        }
    }
#endif
    if (!texture || upload_error != GL_NO_ERROR) {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
        bongo_cat_error_set(error, upload_error == GL_OUT_OF_MEMORY
            ? BONGO_CAT_ERROR_MEMORY : BONGO_CAT_ERROR_PLATFORM,
            "OpenGL texture upload failed (0x%x): %s", (unsigned)upload_error, path);
    }
    if (width) *width = image.width;
    if (height) *height = image.height;
    bongo_cat_image_free(&image);
    return texture;
}

static void erase_paw(BongoCatImage *image, bool left) {
    float cx = left ? .700f : .275f, cy = left ? .515f : .397f;
    float rx = left ? .080f : .070f, ry = left ? .170f : .160f;
    for (int y = 0; y < image->height; ++y) for (int x = 0; x < image->width; ++x) {
        float dx = (x / (float)image->width - cx) / rx;
        float dy = (y / (float)image->height - cy) / ry;
        unsigned char *pixel = image->pixels + ((size_t)y * image->width + x) * 4;
        if (dx * dx + dy * dy < 1.0f && pixel[3])
            pixel[0] = pixel[1] = pixel[2] = 255;
    }
}

static bool blend_file(BongoCatImage *base, const char *path, BongoCatError *error) {
    if (!path || !path[0]) return true;
    BongoCatImage layer;
    if (bongo_cat_image_load(path, &layer, error) != BONGO_CAT_OK) return false;
    bool valid = layer.width == base->width && layer.height == base->height;
    if (valid) for (int i = 0; i < base->width * base->height; ++i) {
        unsigned char *dst = base->pixels + i * 4, *src = layer.pixels + i * 4;
        unsigned alpha = src[3], inverse = 255 - alpha, destination_alpha = dst[3];
        unsigned output_alpha = alpha * 255 + destination_alpha * inverse;
        if (!output_alpha) { memset(dst, 0, 4); continue; }
        for (int channel = 0; channel < 3; ++channel) {
            unsigned color = src[channel] * alpha * 255 +
                dst[channel] * destination_alpha * inverse;
            dst[channel] = (unsigned char)((color + output_alpha / 2) / output_alpha);
        }
        dst[3] = (unsigned char)((output_alpha + 127) / 255);
    }
    bongo_cat_image_free(&layer);
    return valid;
}

unsigned int bongo_cat_image_composite_texture(const char *base, const char *left,
    const char *right, unsigned int texture, bool erase_left, bool erase_right,
    BongoCatError *error) {
    BongoCatImage image;
    if (bongo_cat_image_load(base, &image, error) != BONGO_CAT_OK) return 0;
    if (erase_left) erase_paw(&image, true);
    if (erase_right) erase_paw(&image, false);
    bool valid = blend_file(&image, left, error) && blend_file(&image, right, error);
    if (valid) texture = upload(&image, texture, false);
    bongo_cat_image_free(&image);
    return texture;
}
