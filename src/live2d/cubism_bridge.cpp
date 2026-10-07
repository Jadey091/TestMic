#include "bongo_cat/file.h"
#include "bongo_cat/memory.h"
#include "bongo_cat/model.h"
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
#include <GL/glew.h>
#endif
#include "cubism_model.hpp"

#include <CubismFramework.hpp>
#include <SDL3/SDL_filesystem.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

using Csm::CubismFramework;

namespace {

class Allocator final : public Csm::ICubismAllocator {
public:
    void *Allocate(const Csm::csmSizeType size) override { return std::malloc(size); }
    void Deallocate(void *memory) override { std::free(memory); }
    void *AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment) override {
#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        void *memory = nullptr;
        return posix_memalign(&memory, alignment, size) == 0 ? memory : nullptr;
#endif
    }
    void DeallocateAligned(void *memory) override {
#ifdef _WIN32
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
};

Allocator allocator;
CubismFramework::Option framework_option;
std::string resource_root;
int runtime_count;

void log_message(const char *message) {
    if (message) std::fprintf(stderr, "[Cubism] %s\n", message);
}

Csm::csmByte *load_file(const std::string path, Csm::csmSizeInt *size) {
    if (size) *size = 0;
    FILE *file = bongo_cat_file_open(path.c_str(), "rb");
    if (!file) {
        const char *base = SDL_GetBasePath();
        if (base) file = bongo_cat_file_open((std::string(base) + path).c_str(), "rb");
    }
    if (!file && !resource_root.empty())
        file = bongo_cat_file_open((resource_root + "/" + path).c_str(), "rb");
    if (!file) return nullptr;
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    if (length <= 0) { std::fclose(file); return nullptr; }
    auto *bytes = static_cast<Csm::csmByte *>(std::malloc((size_t)length));
    if (!bytes || std::fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        std::free(bytes);
        bytes = nullptr;
    } else if (size) *size = (Csm::csmSizeInt)length;
    std::fclose(file);
    return bytes;
}

void release_file(Csm::csmByte *bytes) { std::free(bytes); }

bool start_framework(BongoCatError *error) {
    if (runtime_count++) return true;
#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
    glewExperimental = GL_TRUE;
    GLenum glew_result = glewInit();
    glGetError();
    if (glew_result != GLEW_OK) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM, "GLEW initialization failed: %s",
            reinterpret_cast<const char *>(glewGetErrorString(glew_result)));
        return false;
    }
    if (!glCreateShader || !glShaderSource || !glCompileShader ||
        !glGetShaderiv || !glCreateProgram || !glGenFramebuffers) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Required OpenGL 3.3 functions are unavailable");
        return false;
    }
#endif
    framework_option = CubismFramework::Option{};
    framework_option.LogFunction = log_message;
    framework_option.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
    framework_option.LoadFileFunction = load_file;
    framework_option.ReleaseBytesFunction = release_file;
    if (!CubismFramework::StartUp(&allocator, &framework_option)) {
        runtime_count = 0;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM, "Cubism Framework startup failed");
        return false;
    }
    CubismFramework::Initialize();
    return true;
}

void stop_framework() {
    if (--runtime_count > 0) return;
    CubismFramework::Dispose();
    CubismFramework::CleanUp();
    runtime_count = 0;
}

} // namespace

struct BongoCatLive2D {
    bongo_cat::NativeModel *model;
    int width = 612;
    int height = 354;
};

static bool restore_previous(BongoCatLive2D *runtime,
    bongo_cat::NativeModel *previous, BongoCatError *primary) noexcept {
    if (!previous) { runtime->model = nullptr; return true; }
    BongoCatError restore_error = {};
    try {
        if (previous->load_textures(&restore_error)) {
            runtime->model = previous;
            return true;
        }
    } catch (const std::bad_alloc &) {
        bongo_cat_error_set(&restore_error, BONGO_CAT_ERROR_MEMORY,
            "Out of memory while restoring the previous model");
    } catch (const std::exception &exception) {
        bongo_cat_error_set(&restore_error, BONGO_CAT_ERROR_CUBISM,
            "Previous model restore failed: %s", exception.what());
    } catch (...) {
        bongo_cat_error_set(&restore_error, BONGO_CAT_ERROR_CUBISM,
            "Previous model restore failed with an unknown exception");
    }
    if (primary) {
        char original[sizeof(primary->message)];
        std::snprintf(original, sizeof(original), "%s", primary->message);
        bongo_cat_error_set(primary, primary->code,
            "%s; restore failed: %s", original, restore_error.message);
    }
    delete previous;
    runtime->model = nullptr;
    return false;
}

extern "C" BongoCatLive2D *bongo_cat_live2d_create(const char *asset_root,
    BongoCatError *error) {
    resource_root = asset_root ? asset_root : "";
    if (!start_framework(error)) return nullptr;
    BongoCatLive2D *runtime = new(std::nothrow) BongoCatLive2D{};
    if (!runtime) {
        stop_framework();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY, "Cannot allocate Cubism runtime");
    }
    return runtime;
}

extern "C" void bongo_cat_live2d_destroy(BongoCatLive2D *runtime) {
    if (!runtime) return;
    delete runtime->model;
    delete runtime;
    stop_framework();
}

extern "C" BongoCatResult bongo_cat_live2d_load(BongoCatLive2D *runtime,
    const char *directory, const char *setting, bool preset,
    const BongoCatLive2DRenderOptions *render_options, BongoCatError *error) {
    if (!runtime) return BONGO_CAT_ERROR_ARGUMENT;
    bongo_cat::NativeModel *previous = runtime->model;
    bongo_cat::NativeModel *model = nullptr;
    bool previous_released = false;
    try {
        model = new(std::nothrow) bongo_cat::NativeModel();
        if (!model) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
                "Cannot allocate Live2D model");
            return BONGO_CAT_ERROR_MEMORY;
        }
        if (render_options) model->set_render_options(*render_options);
        if (!model->load(directory, setting, preset, error)) {
            delete model;
            return error ? error->code : BONGO_CAT_ERROR_CUBISM;
        }
        if (previous) {
            previous_released = true;
            previous->release_render_resources();
            glFinish();
        }
        model->reshape(runtime->width, runtime->height);
        if (!model->load_textures(error)) {
            BongoCatResult result = error ? error->code : BONGO_CAT_ERROR_CUBISM;
            delete model;
            glFinish();
            restore_previous(runtime, previous, error);
            glFinish();
            bongo_cat_platform_trim_memory();
            return result;
        }
        delete previous;
        runtime->model = model;
        glFinish();
        bongo_cat_platform_trim_memory();
        return BONGO_CAT_OK;
    } catch (const std::bad_alloc &) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Out of memory while loading the Live2D model");
    } catch (const std::exception &exception) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "Live2D model load failed: %s", exception.what());
    } catch (...) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_CUBISM,
            "Live2D model load failed with an unknown exception");
    }
    delete model;
    if (previous_released) {
        glFinish();
        restore_previous(runtime, previous, error);
        glFinish();
        bongo_cat_platform_trim_memory();
    }
    return error ? error->code : BONGO_CAT_ERROR_CUBISM;
}

extern "C" bool bongo_cat_live2d_ready(const BongoCatLive2D *runtime) {
    return runtime && runtime->model;
}

extern "C" void bongo_cat_live2d_resize(BongoCatLive2D *runtime, int width, int height) {
    if (!runtime) return;
    if (width > 0 && height > 0) {
        runtime->width = width;
        runtime->height = height;
    }
    if (runtime->model) runtime->model->resize(width, height);
}
extern "C" void bongo_cat_live2d_reshape(BongoCatLive2D *runtime, int width, int height) {
    if (!runtime) return;
    if (width > 0 && height > 0) {
        runtime->width = width;
        runtime->height = height;
    }
    if (runtime->model) runtime->model->reshape(width, height);
}
extern "C" bool bongo_cat_live2d_update(BongoCatLive2D *runtime, float elapsed) {
    return runtime && runtime->model && runtime->model->update(elapsed);
}
extern "C" void bongo_cat_live2d_draw(BongoCatLive2D *runtime) {
    if (runtime && runtime->model) runtime->model->draw();
}
extern "C" void bongo_cat_live2d_set_mirror(BongoCatLive2D *runtime, bool mirror) {
    if (runtime && runtime->model) runtime->model->set_mirror(mirror);
}
extern "C" void bongo_cat_live2d_set_render_options(BongoCatLive2D *runtime,
    const BongoCatLive2DRenderOptions *options) {
    if (runtime && runtime->model && options)
        runtime->model->set_render_options(*options);
}
extern "C" void bongo_cat_live2d_set_dragging(BongoCatLive2D *runtime,
    float x, float y) {
    if (runtime && runtime->model) runtime->model->set_dragging(x, y);
}
extern "C" void bongo_cat_live2d_prepare_viewer_audit(BongoCatLive2D *runtime) {
    if (runtime && runtime->model) runtime->model->prepare_viewer_audit();
}
extern "C" bool bongo_cat_live2d_set_parameter(BongoCatLive2D *runtime, const char *id, float value) {
    return runtime && runtime->model && runtime->model->set_parameter(id, value);
}
extern "C" bool bongo_cat_live2d_parameter(BongoCatLive2D *runtime, const char *id,
    BongoCatParameterRange *range) {
    return runtime && runtime->model && range && runtime->model->parameter(id,
        &range->minimum, &range->maximum, &range->value);
}
extern "C" bool bongo_cat_live2d_start_motion(BongoCatLive2D *runtime, const char *group, int index) {
    return runtime && runtime->model && runtime->model->start_motion(group, index);
}
extern "C" bool bongo_cat_live2d_set_expression(BongoCatLive2D *runtime, int index) {
    return runtime && runtime->model && runtime->model->set_expression(index);
}
extern "C" int bongo_cat_live2d_expression(const BongoCatLive2D *runtime) {
    return runtime && runtime->model ? runtime->model->expression() : -1;
}
extern "C" bool bongo_cat_live2d_visual_state(const BongoCatLive2D *runtime,
    BongoCatLive2DVisualState *state) {
    return runtime && runtime->model && runtime->model->visual_state(state);
}
